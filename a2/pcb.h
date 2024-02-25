#ifndef PCB_H
#define PCB_H
#include <stdbool.h>

// typedef struct{
//     int page_entries;
//     int line_index[3];
//     // int valid_bit[3];
// }PAGE_TABLE;

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
    int* PAGE_TABLE;
    char* file_name;
}PCB;

int generatePID();
PCB * makePCB(int start, int end);
#endif