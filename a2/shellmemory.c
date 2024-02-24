#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<stdbool.h>
#include "pcb.h"

#define SHELL_MEM_LENGTH 1000

#define FRAME_STORE_SIZE 600 //200*3
#define VARIABLE_STORE_SIZE 400 //1000-600

struct memory_struct{
	char *var;
	char *value;
};

struct memory_struct shellmemory[SHELL_MEM_LENGTH];

// Helper functions
int match(char *model, char *var) {
	int i, len=strlen(var), matchCount=0;
	for(i=0;i<len;i++)
		if (*(model+i) == *(var+i)) matchCount++;
	if (matchCount == len)
		return 1;
	else
		return 0;
}

char *extract(char *model) {
	char token='=';    // look for this to find value
	char value[1000];  // stores the extract value
	int i,j, len=strlen(model);
	for(i=0;i<len && *(model+i)!=token;i++); // loop till we get there
	// extract the value
	for(i=i+1,j=0;i<len;i++,j++) value[j]=*(model+i);
	value[j]='\0';
	return strdup(value);
}

// Shell memory functions
void mem_init(){
	int i;
	for (i=0; i<1000; i++){		
		shellmemory[i].var = "none";
		shellmemory[i].value = "none";
	}
}

int resetmem(){
	for(int i = FRAME_STORE_SIZE; i<SHELL_MEM_LENGTH; i++){
		shellmemory[i].var = "none";
		shellmemory[i].value = "none";
	}
	return 0;
}

// Set key value pair
void mem_set_value(char *var_in, char *value_in) {
	int i;
	for (i=FRAME_STORE_SIZE; i<1000; i++){
		if (strcmp(shellmemory[i].var, var_in) == 0){
			shellmemory[i].value = strdup(value_in);
			return;
		} 
	}

	//Value does not exist, need to find a free spot.
	for (i=FRAME_STORE_SIZE; i<1000; i++){
		if (strcmp(shellmemory[i].var, "none") == 0){
			shellmemory[i].var = strdup(var_in);
			shellmemory[i].value = strdup(value_in);
			return;
		} 
	}

	return;

}

//get value based on input key
char *mem_get_value(char *var_in) {
	int i;
	for (i=FRAME_STORE_SIZE; i<1000; i++){
		if (strcmp(shellmemory[i].var, var_in) == 0){
			return strdup(shellmemory[i].value);
		} 
	}
	return NULL;

}

void printShellMemory(){
	int count_empty = 0;
	for (int i = 0; i < SHELL_MEM_LENGTH; i++){
		if(strcmp(shellmemory[i].var,"none") == 0){
			count_empty++;
		}
		else{
			printf("\nline %d: key: %s\t\tvalue: %s\n", i, shellmemory[i].var, shellmemory[i].value);
		}
    }
	printf("\n\t%d lines in total, %d lines in use, %d lines free\n\n", SHELL_MEM_LENGTH, SHELL_MEM_LENGTH-count_empty, count_empty);
}

/*
 * Function:  addFileToMem 
 * 	Added in A2
 * --------------------
 * Load the source code of the file fp into the shell memory:
 * 		Loading format - var stores fileID, value stores a line
 *		Note that the first 100 lines are for set command, the rests are for run and exec command
 *
 *  pStart: This function will store the first line of the loaded file 
 * 			in shell memory in here
 *	pEnd: This function will store the last line of the loaded file 
 			in shell memory in here
 *  fileID: Input that need to provide when calling the function, 
 			stores the ID of the file
 * 
 * returns: error code, 21: no space left
 */
int load_file(FILE* fp, int* pStart, int* pEnd, char* filename)
{
    size_t i;
    int error_code = 0;
	bool hasSpaceLeft = false;
	bool first = true;
	i = 0;
	size_t candidate;
	int lineCount = 0;
	int fileLineCount = 0;
	char line[100];

	while(true){
		//find the first available frame
		for (i; i < FRAME_STORE_SIZE; i+=3){
			if(strcmp(shellmemory[i].var,"none") == 0){
				if (first) {
					*pStart = i;
					first = false;
				}
				hasSpaceLeft = true;
				break;
			}
		}
		candidate = i; //next free frame number

		//shell memory is full
		if(hasSpaceLeft == 0){
			error_code = 21;
			return error_code;
		}
		//load file line by line until e.o.f.
		if(feof(fp)) {
			break;
		}
		for (size_t j = i; j < i+3; j++){		
			// line = calloc(1, FRAME_STORE_SIZE);
			if (fgets(line, sizeof(line), fp) == NULL)
			{
				shellmemory[j].var = strdup(filename);
				shellmemory[j].value = '\0';
				fileLineCount++;
				continue;
			}
			shellmemory[j].var = strdup(filename);
			shellmemory[j].value = strndup(line, strlen(line));
			fileLineCount++;
			*pEnd = (int)fileLineCount%3+(int)candidate-1;
			if (fileLineCount%3 == 0 && fileLineCount>0) *pEnd = *pEnd + 3;
			//printf("end = %d, flc = %d, candidate = %d\n", *pEnd, fileLineCount, candidate);
		}
	}
    
	//no space left to load the entire file into shell memory
	if(!feof(fp)){
		error_code = 21;
		//clean up the file in memory
		for(int j = 1; i <= FRAME_STORE_SIZE; i ++){
			shellmemory[j].var = "none";
			shellmemory[j].value = "none";
    	}
		return error_code;
	}
    return error_code;
}

char * mem_get_value_at_line(int index){
	if(index<0 || index > SHELL_MEM_LENGTH) return NULL; 
	return shellmemory[index].value;
}

void mem_free_lines_between(int start, int end){
	for (int i=start; i<=end && i<SHELL_MEM_LENGTH; i++){
		if(shellmemory[i].var != NULL){
			free(shellmemory[i].var);
		}	
		if(shellmemory[i].value != NULL){
			free(shellmemory[i].value);
		}	
		shellmemory[i].var = "none";
		shellmemory[i].value = "none";
	}
}