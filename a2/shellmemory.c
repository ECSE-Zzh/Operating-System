#include "shellmemory.h"
struct memory_struct{
	char *var;
	char *value;
};

struct memory_struct shellmemory[SHELL_MEM_LENGTH];
int time_log[FRAME_STORE_SIZE/3]; // each frame has its corresponding time stamp
int time = 0;

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
	int i; //physical address
	for (i=0; i<SHELL_MEM_LENGTH; i++){		
		shellmemory[i].var = "none";
		shellmemory[i].value = "none";
		if (i < FRAME_STORE_SIZE) updateTimeLog(i/3);
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
int load_file(FILE* fp, int* pEnd, char* filename, int* page_table, int page_allowed_load)
{
    int i;
    int error_code = 0;
	bool hasSpaceLeft = false;
	bool first = true;
	int candidate;
	int lineCount = 0;
	int fileLineCount = 0;
	char line[100];
	
	//get file size before loading
	while(fgets(line, sizeof(line), fp) != NULL){
		fileLineCount++;
	}
	*pEnd = fileLineCount-1;
	rewind(fp);

	//load the number of pages allowed
	while(page_allowed_load--){
		// end of file reached
		if(feof(fp)) {
			break;
		}
		//find the first available frame
		i = 0;
		for (i; i < FRAME_STORE_SIZE; i+=3){
			if(strcmp(shellmemory[i].var,"none") == 0){
				hasSpaceLeft = true;
				break;
			}
		}
		candidate = i/3; //next free frame number

		// shell memory is full
		if(!hasSpaceLeft) candidate = pick_victim();

		//store frame number into page table
		page_table[lineCount/3] = candidate; 

		//load file line by line starting from candidate*3 (convert to physical memory)
		//if end of file reached terminate
		for (int j = candidate*3; j < candidate*3+3; j++){
			if (fgets(line, sizeof(line), fp) == NULL)
			{
				shellmemory[j].var = strdup(filename);
				shellmemory[j].value = '\0';
				updateTimeLog(candidate);
				continue;
			}
			shellmemory[j].var = strdup(filename);
			shellmemory[j].value = strndup(line, strlen(line));
			updateTimeLog(candidate);
			lineCount++;
		}
		hasSpaceLeft = false;		//recheck hasSpaceLeft in the next iteration
	}
    
    return error_code;
}

void updateTimeLog(int frame_num){
	time_log[frame_num] = time++;
}

/**
 * findLRU(): finds the LRU frame number
*/
int findLRU() {
	// find the LRU frame number (smallest time stamp)
    int LRU_frame_num = 0; // Assume the first element is the minimum

    for (int i = 1; i < FRAME_STORE_SIZE/3; ++i) {
        if (time_log[i] < time_log[LRU_frame_num]) {
            LRU_frame_num = i; // Update LRU_frame_num if a smaller element is found
        }
    }

    return LRU_frame_num; //frame number
}

/**
 * endsWithNewline: boolean method to determine ending with new line
*/
bool endsWithNewline(char *str) {
    if (str == NULL || str[0] == '\0') {
        // Handle empty string case
        return false;
    }

    int length = strlen(str);

    // Check if the last two characters are '\n'
    return length >= 2 && str[length - 1] == '\n';
}

/**
 * pick_victim: picks a frame to evict from the frame store
*/
int pick_victim(){
	//pick a random number within the range of frame store size
	int victim = 0;
	int line_to_replace = 3;
	int candidate;
	PCB* pcb;
	char victim_name_buffer[100]; 

	// victim is a physical address (shellmemory)
	victim = 3*findLRU();
	candidate = victim/3; // candidate: next free frame number

	printf("%s\n", "Page fault! Victim page contents:");
	// save victim name for updating its page table
	strcpy(victim_name_buffer, shellmemory[victim].var);

	// update page table, search corresponding PCB based on victim file name
	if(findPCB(&pcb, victim_name_buffer)){
		for(int i = 0; i < PAGE_TABLE_SIZE; i++){
			if(candidate == pcb->PAGE_TABLE[i]){
				pcb->PAGE_TABLE[i]=-1;
				break;
			}
		}
	}

	//evict three lines starting from victim (physical address)
	while(line_to_replace){
		//if value is none, skip this line
		if (shellmemory[victim].value == NULL || 
			shellmemory[victim].value[0] == '\0' ||
			strcmp(shellmemory[victim].value, "none") == 0) {
			victim++;
			line_to_replace--;
			continue;
		}

		printf("%s", shellmemory[victim].value);
		
		// check if line end with \n
		if(!endsWithNewline(shellmemory[victim].value)) printf("%s", "\n");
		
		//print the evicted line and reset its variable name and value to none
		shellmemory[victim].var = "none";
		shellmemory[victim].value = "none";
		victim++;
		line_to_replace--;
	}
	printf("%s\n", "End of victim page contents.");
	
	
	return candidate;
}

/**
 * handlePageFault: responsible for handling any page faults
*/
void handlePageFault(PCB* pcb){
    //find free spot in memory
	int candidate;
	FILE* fp;
	char* destDirectory = "/backing_store";
	char* currentDirectory;
	int lines_to_load = 3;
	char lineBuffer[100];

	int hasSpaceLeft = 0;

	//if find first free frame spot in shellmemory
	for (int i = 0; i < FRAME_STORE_SIZE; i+=3){
		if(strcmp(shellmemory[i].var,"none") == 0){
			candidate = i/3; //next frame number address
			hasSpaceLeft = 1;
			break;
		}
	}


	//if no space left, kick out a frame
	if(hasSpaceLeft == 0) candidate = pick_victim();

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
			shellmemory[candidate*3+(i - pcb->PC)].var = strdup(pcb->file_name);
			shellmemory[candidate*3+(i - pcb->PC)].value = strndup(lineBuffer, strlen(lineBuffer));
			updateTimeLog(candidate);
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
	updateTimeLog(index/3); //update time log when reading from memory
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