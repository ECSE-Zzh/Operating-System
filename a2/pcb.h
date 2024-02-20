#ifndef PCB_H
#define PCB_H
#include <stdbool.h>

typedef struct PAGE PAGE;

struct PAGE {
    int index[3];
    int valid_bit[3];
    int page_index;
    int page_pid;
};

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
}PCB;

int generatePID();
PCB * makePCB(int start, int end);
#endif