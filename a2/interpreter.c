/**
 * Class: ECSE 427 - Operating Systems
 * Authors: 
 * Ziheng Zhou 260955157
 * Wasif Somji 261003295
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#include "shellmemory.h"
#include "shell.h"
#include "kernel.h"
#include "ready_queue.h"
#include "interpreter.h"

int MAX_ARGS_SIZE = 7;
char my_page_replacement_policy[50];
bool user_set_page_replacement_policy = false;

char* error_msgs[] = {
	"no error",
	"file does not exist",
	"file could not be loaded",
	"no space left in shell memory",
	"ready queue is full",
	"scheduling policy error",
	"too many tokens",
	"too few tokens",
	"non-alphanumeric token",
	"unknown name",
	"cd",
	"mkdir"
};

int handle_error(enum Error error_code){
	printf("Bad command: %s\n", error_msgs[error_code]);
	return error_code;
}

int help();
int quit();
int set(char* var, char* value);
int print(char* var);
int run(char* script);
int echo(char* var);
int my_ls();
int my_mkdir(char* dirname);
int my_touch(char* filename);
int my_cd(char* dirname);
int my_cp (char* source_dir, char *filename, char* destination_dir);
char* formulateFileName(char *fname);//, char* policy, bool background, bool mt);
int exec(char *fname1, char *fname2, char *fname3); 
int is_file_empty(char* file);

// Interpret commands and their arguments
int interpreter(char* command_args[], int args_size){
	if (args_size < 1)
		return handle_error(TOO_FEW_TOKENS);
	if (args_size > MAX_ARGS_SIZE)
		return handle_error(TOO_MANY_TOKENS);

	for (int i=0; i<args_size; i++)
	{ // strip spaces, newline, etc.
		command_args[i][strcspn(command_args[i], "\r\n")] = 0;
	}

	if (strcmp(command_args[0], "help")==0)
	{ // help
	    if (args_size > 1) return handle_error(TOO_MANY_TOKENS);
	    return help();
	}
	else if (strcmp(command_args[0], "quit") == 0 || strcmp(command_args[0], "exit") == 0)
	{ // quit
		if (args_size > 1) return handle_error(TOO_MANY_TOKENS);

		return quit();
	}
	else if (strcmp(command_args[0], "set")==0)
	{ //set
		if (args_size < 3) return handle_error(TOO_FEW_TOKENS);
		int total_len = 0;
		for(int i=2; i<args_size; i++){
			total_len+=strlen(command_args[i])+1;
		}
		char *value = (char*) calloc(1, total_len);
		char spaceChar = ' ';
		for(int i = 2; i < args_size; i++){
			strncat(value, command_args[i], strlen(command_args[i]));
			if(i < args_size-1){
				strncat(value, &spaceChar, 1);
			}
		}
		int errCode = set(command_args[1], value);
		free(value);
		return errCode;
	}
	else if (strcmp(command_args[0], "print") == 0)
	{ // print
		if (args_size < 2) return handle_error(TOO_FEW_TOKENS);
		if (args_size > 2) return handle_error(TOO_MANY_TOKENS);
		return print(command_args[1]);
	}
	else if (strcmp(command_args[0], "run") == 0)
	{ // run
		if (args_size < 2) return handle_error(TOO_FEW_TOKENS);
		if (args_size > 2) return handle_error(TOO_MANY_TOKENS);	
		return run(command_args[1]);
	}
	else if (strcmp(command_args[0], "echo") == 0)
	{ // echo
		if (args_size > 2) return handle_error(TOO_MANY_TOKENS);
		return echo(command_args[1]);
	}
	else if (strcmp(command_args[0], "my_ls") == 0)
	{ // ls
		if (args_size > 1) return handle_error(TOO_MANY_TOKENS);
		return my_ls(command_args[0]);
	}
	else if (strcmp(command_args[0], "my_mkdir")==0)
	{
		if (args_size > 2) return handle_error(TOO_MANY_TOKENS);
		return my_mkdir(command_args[1]);
	}
	else if (strcmp(command_args[0], "my_touch")==0)
	{
		if (args_size > 2) return handle_error(TOO_MANY_TOKENS);
		return my_touch(command_args[1]);
	}
	else if (strcmp(command_args[0], "my_cd")==0)
	{
		if (args_size > 2) return handle_error(TOO_MANY_TOKENS);
		return my_cd(command_args[1]);	
	} 
	else if (strcmp(command_args[0], "exec")==0)
	{
		if (args_size <= 1) return handle_error(TOO_FEW_TOKENS);
		if (args_size > 5) return handle_error(TOO_MANY_TOKENS);
		if(args_size == 2)
			return exec(command_args[1],NULL,NULL);
		else if(args_size == 3)
			return exec(command_args[1],command_args[2],NULL); 
		else if(args_size == 4)
			return exec(command_args[1],command_args[2],command_args[3]);
	} 

	else if(strcmp(command_args[0], "resetmem") == 0){
		if(args_size > 1) return handle_error(TOO_MANY_TOKENS);
		return resetmem();
	}
	else if(strcmp(command_args[0], "pagepolicy") == 0){
		if(args_size > 2) return handle_error(TOO_MANY_TOKENS);
		return pageReplacementPolicy(command_args[1]);
	}	
	return handle_error(BAD_COMMAND);
}

int help(){

	char help_string[] = "COMMAND			DESCRIPTION\n \
help			Displays all the commands\n \
quit			Exits / terminates the shell with “Bye!”\n \
set VAR STRING		Assigns a value to shell memory\n \
print VAR		Displays the STRING assigned to VAR\n \
run SCRIPT.TXT		Executes the file SCRIPT.TXT\n ";
	printf("%s\n", help_string);
	return 0;
}

int quit(){
	my_cd("..");
	printf("%s\n", "Bye!");
	ready_queue_destory();

	//Delete backing_store before quit
    int errCode = system("rm -rf ./backing_store");

    if (errCode != 0) {
        perror("Error deleting backing_store");
        return errCode;
    }
	
	exit(0);
}

int set(char* var, char* value){
	char *link = "=";
	char buffer[1000];
	strcpy(buffer, var);
	strcat(buffer, link);
	strcat(buffer, value);
	mem_set_value(var, value);
	return 0;
}

int print(char* var){
	char *value = mem_get_value(var);
	if(value == NULL) value = "\n";
	printf("%s\n", value); 
	return 0;
}

int echo(char* var){
	if(var[0] == '$') print(++var);
	else printf("%s\n", var); 
	return 0; 
}

int my_ls(){
	int errCode = system("ls | sort");
	return errCode;
}

int my_mkdir(char *dirname){
	char *dir = dirname;
	if(dirname[0] == '$'){
		char *value = mem_get_value(++dirname);
		if(value == NULL || strchr(value, ' ') != NULL){
			return handle_error(ERROR_MKDIR);
		}
		dir = value;
	}
	int namelen = strlen(dir);
	char* command = (char*) calloc(1, 7+namelen); 
	strncat(command, "mkdir ", 7);
	strncat(command, dir, namelen);
	int errCode = system(command);
	free(command);
	return errCode;
}

int my_touch(char* filename){
	int namelen = strlen(filename);
	char* command = (char*) calloc(1, 7+namelen); 
	strncat(command, "touch ", 7);
	strncat(command, filename, namelen);
	int errCode = system(command);
	free(command);
	return errCode;
}

int my_cd(char* dirname){
	struct stat info;
	if(stat(dirname, &info) == 0 && S_ISDIR(info.st_mode)) {
		//the object with dirname must exist and is a directory
		int errCode = chdir(dirname);
		return errCode;
	}
	return handle_error(ERROR_CD);
}

int run(char* script){
	//errCode 11: bad command file does not exist
	int errCode = 0;
	char nameBuffer[1000];

	//load script into shell if script is not null
	if(script != NULL && !is_file_empty(script)){
		strcpy(nameBuffer, formulateFileName(script));
		errCode = process_initialize(nameBuffer);

		my_cd("..");

		if(errCode == 11){
			return handle_error(errCode);
		}
	}

	//run with FCFS
	errCode = schedule_by_policy("FCFS"); //, false);
	return errCode;
}

/**
 * my_cp: command to copy file from source directory to destination directory
*/
int my_cp (char* source_dir, char *filename, char* destination_dir){
	//copy file to backing_store
	int source_dir_namelen = strlen(source_dir);
	int file_namelen = strlen(filename);
	int blank_len = strlen(" ");
	int dest_dir_namelen = strlen(destination_dir);

	//formulate correct path and cp command
	char* command = (char*) calloc(1, 2048); 
	strncat(command, "cp ", 4);
	strncat(command, source_dir, source_dir_namelen);
	strncat(command, filename, file_namelen);
	strncat(command, " ", blank_len);
	strncat(command, destination_dir, dest_dir_namelen);
	int errCode = system(command);
	free(command);
	return errCode;
}

/**
 * formulateFileName: creates a unique file name for copying into backing_store
*/
char* formulateFileName(char *fname){
	//formulate correct file name
	char* tempDestDirectory = "backing_store/";
	char* destDirectory = "/backing_store";
	char* currentDirectory;
	char randCharBuffer[100];
	char nameBuffer[1000];

	randCharBuffer[0] = '\0';

	//set unique number
	sprintf(randCharBuffer, "%d", rand());

	//formulate correct destination directory (fname + random number)
	strcpy(nameBuffer, tempDestDirectory);
	strcat(nameBuffer, fname);
	strncat(nameBuffer, randCharBuffer, sizeof(randCharBuffer));

	my_cp("", fname, nameBuffer);//copy

	//go to backing_store
	currentDirectory =  (char*)malloc(1024);
	getcwd(currentDirectory, 1024);
	strcat(currentDirectory, destDirectory);
	my_cd(currentDirectory); 
	free(currentDirectory);

	//clear nameBuffer, return correct fname
	nameBuffer[0] = '\0'; 
	strcpy(nameBuffer, fname);
	strncat(nameBuffer, randCharBuffer, sizeof(randCharBuffer));

	return strdup(nameBuffer);

}

int is_file_empty(char* file) {
	FILE* f = fopen(file, "r");
	if (f == NULL) {
		return 1;
	}
	fseek(f, 0, SEEK_END);
	long size = ftell(f); 
	fclose(f);
	return size == 0;
}

// modified exec command
int exec(char *fname1, char *fname2, char *fname3) {
	int error_code = 0;
	char nameBuffer[1000];
	
	if(fname1 != NULL && !is_file_empty(fname1)){
		//clear nameBuffer
		nameBuffer[0] = '\0'; 

		strcpy(nameBuffer, formulateFileName(fname1));
        error_code = process_initialize(nameBuffer);

		//go back to parent directory of backing_store to delete it when 'quit'
		my_cd("..");

		if(error_code != 0){
			return handle_error(error_code);
		}
    }
    if(fname2 != NULL && !is_file_empty(fname2)){
		//clear nameBuffer
		nameBuffer[0] = '\0'; 

		strcpy(nameBuffer, formulateFileName(fname2));
        error_code = process_initialize(nameBuffer);

		//go back to parent directory of backing_store to delete it when 'quit'
		my_cd("..");

		if(error_code != 0){
			return handle_error(error_code);
		}
    }
    if(fname3 != NULL && !is_file_empty(fname3)){
		//clear nameBuffer
		nameBuffer[0] = '\0'; 

		strcpy(nameBuffer, formulateFileName(fname3));
        error_code = process_initialize(nameBuffer);

		//go back to parent directory of backing_store to delete it when 'quit'
		my_cd("..");

		if(error_code != 0){
			return handle_error(error_code);
		}
    } 
	error_code = schedule_by_policy("RR");
	if(error_code != 0){
		return handle_error(error_code);
	}
	return 0; 
}

// Use user defined page replacement policy
// if defined page replacement policy is system-unsupported, use LRU automatically
int pageReplacementPolicy(char* page_replacement_policy){
	user_set_page_replacement_policy = true;
	strcpy(my_page_replacement_policy, page_replacement_policy);
	return 0;
}

char* getPageReplacementPolicy(){
	if(user_set_page_replacement_policy) return my_page_replacement_policy;
	return "LRU";
}

// Check if user choose a specific page replacement policy
bool userSetPageReplacementPolicy(){
	return user_set_page_replacement_policy;
}