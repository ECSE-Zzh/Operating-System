/**
 * Class: ECSE 427 - Operating Systems
 * Authors: 
 * Ziheng Zhou 260955157
 * Wasif Somji 261003295
*/

#ifndef PCB_H
#define PCB_H
#include <stdbool.h>

#define PAGE_TABLE_SIZE 100

/*
 * Struct:  PCB 
 * --------------------
 * pid: process(task) id
 * PC: program counter, stores the index of line that the task is executing
 * start: the first line in shell memory that belongs to this task
 * end: the last line in shell memory that belongs to this task
 * job_length_score: for EXEC AGING use only, stores the job length score
 */
typedef struct
{
    bool priority;
    int pid;
    int PC;
    int start;
    int end;
    int job_length_score;
    int PAGE_TABLE[PAGE_TABLE_SIZE];
    char file_name[100];
}PCB;

int generatePID();
PCB * makePCB(char* filename);
void initPCBStore();
bool findPCB(PCB** pcb, char* filename);
#endif