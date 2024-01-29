#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <sys/types.h> //for my_mkdir
#include <sys/stat.h> // these could be useful? for my_mkdir
#include <fcntl.h> //for my_touch
#include "shellmemory.h"
#include "shell.h"

int MAX_ARGS_SIZE = 3;

// For set command only
int badSetCommand(){
	printf("%s\n", "Bad command: set");
	return 1;
}

int badIfCommand(){
	printf("%s\n", "Empty if clause");
	return 1;
}

int badcommand(){
	printf("%s\n", "Unknown Command");
	return 1;
}

// For run command only
int badcommandFileDoesNotExist(){
	printf("%s\n", "Bad command: File not found");
	return 3;
}

int help();
int quit();
int set(char* var, char* value);
int print(char* var);
int run(char* script);
int echo(char* value);
int my_ls();
int my_mkdir(char* dirname);
int my_touch(char* file_name);
int my_cd(char* dirname);
int my_cat(char* file_name);
int my_if(char* identifier1, char* op, char* identifier2, char* myShell_command1, char* val1, char* myShell_command2, char* val2);
int badcommandFileDoesNotExist();

// Interpret commands and their arguments
int interpreter(char* command_args[], int args_size){
	int i;
	char value_buffer[600]="";
	char dirname_buffer[200]="";
	char file_name_buffer[200]="";
	char new_dirname_buffer[200]="";
	char file_content_buffer[200]="";

	//avoid blank line as an input
	if ( args_size < 1 ){
		return badcommand();
	}

	for ( i=0; i<args_size; i++){ //strip spaces new line etc
		command_args[i][strcspn(command_args[i], "\r\n")] = 0;
	}

	if (strcmp(command_args[0], "help")==0){
	    //help
	    if (args_size != 1) return badcommand();
	    return help();
	
	} else if (strcmp(command_args[0], "quit")==0) {
		//quit
		if (args_size != 1) return badcommand();
		return quit();

	} else if (strcmp(command_args[0], "set")==0) {
		//set: takes at least 1 token and at most five tokens
		if (args_size > 7 || args_size < 3) {
			// printf("%s\n", "Bad command: set");
			return badSetCommand();
		}

		// concatenate char elements in one string
		for(int j = 2; j < args_size; j++){
			strcat(value_buffer, command_args[j]);
			if(j < args_size - 1){
				strcat(value_buffer, " ");
			}
		}	
		return set(command_args[1], value_buffer);	
	
	} else if (strcmp(command_args[0], "print")==0) {
		if (args_size != 2) return badcommand();
		return print(command_args[1]);
	
	} else if (strcmp(command_args[0], "run")==0) {
		if (args_size != 2) return badcommand();
		return run(command_args[1]);
	
	} else if (strcmp(command_args[0], "echo")==0) {
		//echo
		if (args_size != 2) return badcommand(); //take only one token string
		return echo(command_args[1]);

	} else if (strcmp(command_args[0], "my_ls")==0) {
		//my_ls
		if (args_size != 1) return badcommand(); 
		return my_ls();

	} else if (strcmp(command_args[0], "my_mkdir")==0) {
		//my_mkdir
		if (args_size < 2) return badcommand(); //directory name cannot be blank

		// concatenate char elements in one string
		for(int j = 1; j < args_size; j++){
			strcat(dirname_buffer, command_args[j]);
			if(j < args_size - 1){
				strcat(dirname_buffer, " "); //allow space in directory names
			}
		}
		return my_mkdir(dirname_buffer);

	} else if (strcmp(command_args[0], "my_touch")==0) {
		//my_touch
		if (args_size < 2) return badcommand(); //file name cannot be blank

		// concatenate char elements in one string
		for(int j = 1; j < args_size; j++){
			strcat(file_name_buffer, command_args[j]);
			if(j < args_size - 1){
				strcat(file_name_buffer, " "); //allow space in file names
			}
		}
		return my_touch(file_name_buffer);

	} else if (strcmp(command_args[0], "my_cd")==0) {
		//my_cd
		if (args_size < 2 ) return my_cd(".."); //empty input: go one level up

		for(int j = 1; j < args_size; j++){
			strcat(new_dirname_buffer, command_args[j]);
			if(j < args_size - 1){
				strcat(new_dirname_buffer, " "); //allow space in directory
			}
		}
		return my_cd(new_dirname_buffer);

	} else if (strcmp(command_args[0], "my_cat")==0) {
		//my_cat
		for(int j = 1; j < args_size; j++){
			strcat(file_content_buffer, command_args[j]);
			if(j < args_size - 1){
				strcat(file_content_buffer, " "); //allow space in file name
			}
		}
		return my_cat(file_content_buffer);

	} else if (strcmp(command_args[0], "if")==0) {
		if(strcmp(command_args[4], "then") != 0 || strcmp(command_args[7], "else") != 0 || strcmp(command_args[10], "fi") != 0) {
			// printf("%s\n%s\n%s\n%s\n", command_args[0], command_args[4], command_args[7], command_args[10]);
			return badIfCommand();
		}
		return my_if(command_args[1], command_args[2], command_args[3], command_args[5], command_args[6], command_args[8], command_args[9]);

	} else return badcommand();
}

int help(){

	char help_string[] = "COMMAND			DESCRIPTION\n \
help			Displays all the commands\n \
quit			Exits / terminates the shell with “Bye!”\n \
set VAR STRING		Assigns a value to shell memory\n \
print VAR		Displays the STRING assigned to VAR\n \
run SCRIPT.TXT		Executes the file SCRIPT.TXT\n ";
	printf("%s\n", help_string);
	fflush(stdout); // clears print statement buffer
	return 0;
}

int quit(){
	printf("%s\n", "Bye!");
	exit(0);
}

int set(char* var, char* value){

	mem_set_value(var, value);

	return 0;
}

int print(char* var){
	printf("%s\n", mem_get_value(var)); 
	return 0;
}

int run(char* script){
	int errCode = 0;
	char line[1000];
	FILE *p = fopen(script,"rt");  // the program is in a file

	if(p == NULL){
		return badcommandFileDoesNotExist();
	}

	fgets(line,999,p);
	while(1){
		errCode = parseInput(line);	// which calls interpreter()
		memset(line, 0, sizeof(line));

		if(feof(p)){
			break;
		}
		fgets(line,999,p);
	}

    fclose(p);

	return errCode;
}

int echo(char* value){
	char* dollar_echo = "";
	int length = strlen(value);

	if(value[0] == '$'){

		//remove the "$" sign from input
    	for (int j = 0; j < length; j++) {
        	value[j] = value[j + 1];
    	}		

		dollar_echo =  mem_get_value(value);
		printf("%s\n", dollar_echo);
		return 0;
	} 

	printf("%s\n", value);
	return 0;
}

//my_ls: list all the files present in the current directory
int my_ls(){
    if (system("ls") == -1) {
        // printf("%s\n", "invoking ls failed"); //invoking system ls failed
        return -1;
    }

    return 0;
}

int my_mkdir(char* dirname){
	return mkdir(dirname, S_IRWXU);
}

//my_touch: create a file with given file name in current directory
int my_touch(char* file_name){
	//Create a file if it doesn't exit
	if(open(file_name, O_CREAT) == -1) {
		// printf("%s\n", "failed to create file");
		return -1;
	}

	return 0;
}

//my_cd: change the current directory to the specified directory
int my_cd(char* dirname){

	if(chdir(dirname) != 0) printf("%s\n", "Bad command: my_cd"); //directory name does not exist
	// char cur_dirname[1024];

	// if(chdir(dirname) == 0){

	// 	//get current working directory and store it in cur_dirname
	// 	if(getcwd(cur_dirname, sizeof(cur_dirname)) != NULL){
	// 		printf("%s\n", cur_dirname);
	// 		return 0;
	// 	} else {
	// 		printf("%s\n", "failed to get current working directory");
	// 		// return -1;
	// 	}	
	// } else {
	// 	printf("%s\n", "Bad command: my_cd"); //directory name does not exist
	// 	// return -1;
	// }

	return 0;
}

//my_cat: open file and read its content
int my_cat (char* file_name){
	FILE *myFile = fopen(file_name, "r"); //open file
	char file_content[1000] = "";

	//check if file is opened successfully
	if(myFile == NULL) printf("%s\n", "Bad command: my_cat");

	//read file
	while(fgets(file_content, sizeof(file_content), myFile) != NULL){
		printf("%s", file_content);
	}

	//close file
	fclose(myFile);
	return 0;
}

int my_if(char* identifier1, char* op, char* identifier2, char* myShell_command1, char* val1, char* myShell_command2, char* val2){
	// char* dollar_identifier1 = "";
	int length1 = strlen(identifier1);
	// char* dollar_identifier2 = "";
	int length2 = strlen(identifier2);
	int d = 0;
	char* command_args1[2] = {myShell_command1, val1};
	char* command_args2[2] = {myShell_command2, val2};

	if(identifier1[0] == '$'){
		//remove the "$" sign from input
    	for (int j = 0; j < length1; j++) {
        	identifier1[j] = identifier1[j + 1];
    	}		
		identifier1 =  mem_get_value(identifier1);
	}

	if(identifier2[0] == '$'){
		//remove the "$" sign from input
    	for (int j = 0; j < length2; j++) {
        	identifier2[j] = identifier2[j + 1];
    	}		
		identifier2 =  mem_get_value(identifier2);
		// printf("%s\n", identifier2);
	}

	if(strcmp(op, "==")==0){
		d = 1;
	} else if(strcmp(op, "!=")==0){
		d = 2;
	}

	switch (d)
	{
	case 1:
		// op: ==
		if(strcmp(identifier1,identifier2) == 0){
			interpreter(command_args1, 2); //when == is true
		} else {
			interpreter(command_args2, 2); //when == is false
		}
		break;

	case 2:
		// op: !=
		if(strcmp(identifier1,identifier2) != 0){
			interpreter(command_args1, 2); //when != is true
		} else {
			interpreter(command_args2, 2); //when != is false
		}
		break;

	default:
		break;
	}
	return 0;
}