#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<stdbool.h>
#include "pcb.h"

#define SHELL_MEM_LENGTH 1000

#define FRAME_STORE_SIZE 600 //200*3
#define VARIABLE_STORE_SIZE 400 //1000-600

int page_end = 0;

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
	char *line;
	PCB pcb;
    size_t i;
    int error_code = 0;
	bool hasSpaceLeft = false;
	bool flag = true;
	i = 0;
	size_t candidate;
	int empty_line_per_page = 0;
	int tempEnd = 0;
	int lineCount = 0;
	page_end = 0;

	while(flag){
		flag = false;
		for (i; i < FRAME_STORE_SIZE; i+=3){
			if(strcmp(shellmemory[i].var,"none") == 0){
				*pStart = (int)i;
				hasSpaceLeft = true;
				break;
			}
		}
		candidate = i;

	}
	i = candidate;
	//shell memory is full
	if(hasSpaceLeft == 0){
		error_code = 21;
		return error_code;
	}
    
    for (size_t j = i; j < FRAME_STORE_SIZE; j++){		
		// printf("%s %d  %s %d\n", "line count: ",lineCount, "j here: ",j);

        if(feof(fp))
        {
            *pEnd = (int)lineCount-1;
			tempEnd = (int)lineCount-1;
			// printf("%s %d\n", "line count at file end: ",lineCount);

			//Unused lines in current page should be left blank, and move to next page
			if(lineCount % 3 != 0){
				empty_line_per_page = 3 - (lineCount % 3); //lineCount % 3: #empty lines in current page
				page_end = (int)tempEnd + (int)empty_line_per_page;
				for(int k = 0; k < empty_line_per_page; k++){
					line = calloc(1, FRAME_STORE_SIZE);
					shellmemory[tempEnd+k].var = "nothing";
					shellmemory[tempEnd+k].value = "nothing";
					free(line);			
				}
			} 
            break;
        }else{
			line = calloc(1, FRAME_STORE_SIZE);
			if (fgets(line, FRAME_STORE_SIZE, fp) == NULL)
			{
				// printf("%s %d\n", "j shoud NOT be here!!", j);
				// shellmemory[j].var = "why";
            	// shellmemory[j].value = "why?";
				lineCount++;
				continue;
			}
			shellmemory[j].var = strdup(filename);
            shellmemory[j].value = strndup(line, strlen(line));
			free(line);
			lineCount++;
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
	lineCount = 0;
    return error_code;
}

char * mem_get_value_at_line(int index){
	if(index<0 || index > SHELL_MEM_LENGTH) return NULL; 
	return shellmemory[index].value;
}

void mem_free_lines_between(int start, int end){
	// printShellMemory();

	for (int i=start; i<=end && i<SHELL_MEM_LENGTH; i++){
		// printf("%s %d %s %d\n","start: ", start, "end: ", end);
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