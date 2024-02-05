// Authors: Ziheng Zhou, Wasif Somji
// Class: ECSE 427 - Operating Systems

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <ctype.h>
#include <sys/types.h> //for my_mkdir
#include <sys/stat.h> // these could be useful? for my_mkdir
#include <fcntl.h> //for my_touch
#include "shellmemory.h"
#include "shell.h"

int MAX_ARGS_SIZE = 3;

// Allocate commands for bad commands and error cases

// For set command only
int badSetCommand(){
	printf("%s\n", "Bad command: set");
	fflush(stdout); // clears print statement buffer
	return 1;
}

int badIfCommand(){
	printf("%s\n", "Empty if clause");
	fflush(stdout); // clears print statement buffer
	return 1;
}

int badCatCommand(){
	printf("%s\n", "Bad command: my_cat");
	fflush(stdout); // clears print statement buffer
	return 1;
}

int badCdCommand(){
	printf("%s\n", "Bad command: my_cd");
	fflush(stdout); // clears print statement buffer
	return 1;
}

int badcommand(){
	printf("%s\n", "Unknown Command");
	fflush(stdout); // clears print statement buffer
	return 1;
}

// For run command only
int badcommandFileDoesNotExist(){
	printf("%s\n", "Bad command: File not found");
	fflush(stdout); // clears print statement buffer
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
	char new_dirname_buffer[200]="";
	char file_content_buffer[200]="";
	char* check_dollar = "";

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
		if (args_size > 7 || args_size < 3) return badSetCommand();

		//variable name can only be alphanumeric
		if (!isalnum(*command_args[1])) return badSetCommand();

		// concatenate char elements in one string
		for(int j = 2; j < args_size; j++){

			//set: only takes alphanumeric tokens
			if(!isalnum(*command_args[j])) return badSetCommand(); 

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

		if(args_size != 2) return badcommand(); //echo only takes one alphanumeric string

		//echo $var: get and print var value
		check_dollar = command_args[1];
		int length = strlen(check_dollar);
		if(check_dollar[0] == '$'){
			//remove the "$" sign from input and get its value
    		for (int j = 0; j < length; j++) {
        		check_dollar[j] = check_dollar[j + 1];
    		}

			if(!isalnum(*check_dollar)) return badcommand(); //var name can only be alphanumeric
			
			check_dollar =  mem_get_value(check_dollar);

			return echo(check_dollar);
		} 

		//non-alphanumeric input (other than $) is not valid
		if (!isalnum(*command_args[1])) return badcommand(); //take only one token string
		return echo(command_args[1]);

	} else if (strcmp(command_args[0], "my_ls")==0) {
		//my_ls: no input
		if (args_size != 1) return badcommand(); 
		return my_ls();

	} else if (strcmp(command_args[0], "my_mkdir")==0) {
		//my_mkdir: only take one alphanumeric input: directory name
		if(args_size != 2) return badcommand();
		return my_mkdir(command_args[1]);

	} else if (strcmp(command_args[0], "my_touch")==0) {
		//my_touch: only take one alphanumeric input: file name
		if(args_size != 2) return badcommand();
		return my_touch(command_args[1]);

	} else if (strcmp(command_args[0], "my_cd")==0) {
		//my_cd: only take one input: directory
		if(args_size != 2) return badCdCommand();
		return my_cd(command_args[1]);

	} else if (strcmp(command_args[0], "my_cat")==0) {
		//my_cat: only take one input: file name
		if(args_size != 2) return badCatCommand();
		return my_cat(command_args[1]);

	} else if (strcmp(command_args[0], "if")==0) {
		if(args_size != 11)	return badIfCommand();	
		if(strcmp(command_args[4], "then") != 0 || strcmp(command_args[7], "else") != 0 || strcmp(command_args[10], "fi") != 0){
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

	fflush(stdout); // clears print statement buffer

	exit(0);
}

int set(char* var, char* value){

	mem_set_value(var, value);

	return 0;
}

int print(char* var){
	printf("%s\n", mem_get_value(var)); 

	fflush(stdout); // clears print statement buffer

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

/**
 * echo: displays string passed as argument
*/
int echo(char* value){
	printf("%s\n", value);

	fflush(stdout); // clears print statement buffer

	return 0;
}

/**
 * my_ls: lists all files present in the current directory
*/
int my_ls(){

	fflush(stdout); // clears print statement buffer

    if (system("ls") == -1) {
        return -1;
    }

    return 0;
}

/**
 * my_mkdir: creates a new directory called dirname in the current directory
*/
int my_mkdir(char* dirname){

	fflush(stdout); // clears print statement buffer

	return mkdir(dirname, S_IRWXU);
}

/**
 * my_touch: creates a file with given file name in current directory 
*/
int my_touch(char* file_name){

	fflush(stdout); // clears print statement buffer

	//Create a file if it doesn't exit
	if(open(file_name, O_CREAT) == -1) {
		return -1;
	}

	return 0;
}

/**
 * my_cd: change the current directory to the specified directory
*/
int my_cd(char* dirname){
	char cur_dirname[1024];

	fflush(stdout); // clears print statement buffer

	if(chdir(dirname) != 0)  return badCdCommand(); //directory name does not exist

	return 0;
}

/**
 * my_cat: open file and read its content
*/
int my_cat (char* file_name){
	FILE *myFile = fopen(file_name, "r"); //open file
	char file_content[1000] = "";

	//check if file is opened successfully
	if(myFile == NULL) return badCatCommand(); 
	
	//read file
	while(fgets(file_content, sizeof(file_content), myFile) != NULL){
		printf("%s", file_content);
		fflush(stdout); // clears print statement buffer
	}

	//close file
	fclose(myFile);

	return 0;
}

int my_if(char* identifier1, char* op, char* identifier2, char* myShell_command1, char* val1, char* myShell_command2, char* val2){
	int length1 = strlen(identifier1);
	int length2 = strlen(identifier2);
	int d = 0;
	char* command_args1[2] = {myShell_command1, val1};
	char* command_args2[2] = {myShell_command2, val2};

	//get $identifier1 value
	if(identifier1[0] == '$'){
		//remove the "$" sign from input
    	for (int j = 0; j < length1; j++) {
        	identifier1[j] = identifier1[j + 1];
    	}		
		identifier1 =  mem_get_value(identifier1);
	}

	//get $identifier2 value
	if(identifier2[0] == '$'){
		//remove the "$" sign from input
    	for (int j = 0; j < length2; j++) {
        	identifier2[j] = identifier2[j + 1];
    	}		
		identifier2 =  mem_get_value(identifier2);
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
