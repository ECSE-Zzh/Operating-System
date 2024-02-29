/**
 * Class: ECSE 427 - Operating Systems
 * Authors: 
 * Ziheng Zhou 260955157
 * Wasif Somji 261003295
*/

#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

#include "interpreter.h"
#include "pcb.h"

#ifndef FRAME_STORE_SIZE
	#define FRAME_STORE_SIZE 600//200*3
#endif

#ifndef VARIABLE_STORE_SIZE
	#define VARIABLE_STORE_SIZE 400//1000-600
#endif

#define SHELL_MEM_LENGTH (FRAME_STORE_SIZE + VARIABLE_STORE_SIZE)

#ifndef SHELLMEMORY_H
#define SHELLMEMORY_H

void mem_init();
char *mem_get_value(char *var);

void mem_set_value(char *var, char *value);
int load_file(FILE* fp, int* pEnd, char* filename, int* page_table, int page_allowed_load);
char * mem_get_value_at_line(int index);
void mem_free_lines_between(int start, int end);
void printShellMemory();

int resetmem();
void handlePageFault(PCB* pcb);
int pick_victim();
void updateTimeLog(int frame_num);
#endif