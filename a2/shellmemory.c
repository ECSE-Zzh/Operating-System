#include "shellmemory.h"
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
	for (i=0; i<SHELL_MEM_LENGTH; i++){		
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
	for (i=FRAME_STORE_SIZE; i<SHELL_MEM_LENGTH; i++){
		if (strcmp(shellmemory[i].var, var_in) == 0){
			shellmemory[i].value = strdup(value_in);
			return;
		} 
	}

	//Value does not exist, need to find a free spot.
	for (i=FRAME_STORE_SIZE; i<SHELL_MEM_LENGTH; i++){
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
	for (i=FRAME_STORE_SIZE; i<SHELL_MEM_LENGTH; i++){
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
int load_file(FILE* fp, int* pStart, int* pEnd, char* filename, int* page_table, int page_allowed_load)
{
    int i;
    int error_code = 0;
	bool hasSpaceLeft = false;
	bool first = true;
	int candidate;
	int lineCount = 0;
	int fileLineCount = 0;
	char line[100];
	PCB pcb;

	*pStart = 0;
	
	//get file size before loading
	while(fgets(line, sizeof(line), fp) != NULL){
		fileLineCount++;
	}
	*pEnd = fileLineCount-1;
	rewind(fp);

	//load the number of pages allowed
	while(page_allowed_load--){
		//find the first available frame
		i = 0;
		for (i; i < FRAME_STORE_SIZE; i+=3){
			if(strcmp(shellmemory[i].var,"none") == 0){
				hasSpaceLeft = true;
				break;
			}
		}
		candidate = i/3; //next free frame number

		//shell memory is full
		if(hasSpaceLeft == 0){
			printf("%s\n", "no space left");
			error_code = 21;
			return error_code;
		}
		//load file line by line, if end of file reached terminate
		if(feof(fp)) {
			break;
		}
		
		page_table[lineCount/3] = candidate; //store frame number into page table

		for (int j = i; j < i+3; j++){		
			// line = calloc(1, FRAME_STORE_SIZE);
			if (fgets(line, sizeof(line), fp) == NULL)
			{
				shellmemory[j].var = strdup(filename);
				shellmemory[j].value = '\0';
				continue;
			}
			shellmemory[j].var = strdup(filename);
			shellmemory[j].value = strndup(line, strlen(line));
			lineCount++;
		}
	}

	// *pEnd = fileLineCount-1;
    
    return error_code;
}

void handlePageFault(PCB* pcb){
    //find free spot in memory
	int candidate;
	FILE* fp;
	char* destDirectory = "/backing_store";
	char* currentDirectory;
	int lines_to_load = 3;
	char lineBuffer[100];

	int hasSpaceLeft = 0;
	int victim_page = 0;
	
	//if find first free frame spot in shellmemory
	for (int i = 0; i < FRAME_STORE_SIZE; i+=3){
		if(strcmp(shellmemory[i].var,"none") == 0){
			candidate = i/3; //next frame number address
			hasSpaceLeft = 1;
			break;
		}
	}

	//if no space left, kick out a random frame
	if(hasSpaceLeft == 0){
		victim_page = 3*(rand() % (FRAME_STORE_SIZE/3));//pick a random number within the range of frame store size
		candidate = victim_page/3;
		printf("victim page= %d\n", victim_page);

		printf("%s\n", "Page fault! Victim page contents:");
		while(lines_to_load){
			printf("%s\n", shellmemory[victim_page].value);		
			victim_page++;
			lines_to_load--;
		}
		printf("%s\n", "End of victim page contents.");
	}

	//update page table
	//index=page number, page table[index]=frame number
	pcb->PAGE_TABLE[pcb->PC/3]=candidate; 

	//go to backing_store
	my_cd("./backing_store"); 

	fp = fopen(pcb->file_name, "rt");//open file in backing store
	rewind(fp);

	// load (lines_to_load) lines starting from PC into shellmemory
	int i = 0;
	while(lines_to_load) {
		if(feof(fp)){
			break;
		}
		fgets(lineBuffer, sizeof(lineBuffer), fp); 
		if (i >= pcb->PC) {
			shellmemory[candidate*3+(i - pcb->PC)].var = strdup(pcb -> file_name);
			shellmemory[candidate*3+(i - pcb->PC)].value = strndup(lineBuffer, strlen(lineBuffer));
			lines_to_load--;
		}
		i++;
	}
	fclose(fp);
	//go back to parent directory of backing_store to delete it when 'quit'
	my_cd("..");

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