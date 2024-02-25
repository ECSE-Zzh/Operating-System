#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "pcb.h"
#include "kernel.h"
#include "shell.h"
#include "shellmemory.h"
#include "interpreter.h"
#include "ready_queue.h"
#include "interpreter.h"

#define PAGE_TABLE_SIZE 100

bool active = false;
bool debug = false;
bool in_background = false;

int process_initialize(char *filename){
    FILE* fp;
    int* start = (int*)malloc(sizeof(int));
    int* end = (int*)malloc(sizeof(int));
    int* page_table = (int *)malloc(sizeof(int)*PAGE_TABLE_SIZE);

    //initialize all page table value = -1
    for(int i = 0; i < PAGE_TABLE_SIZE; i++){
        page_table[i] = -1;
    }

    fp = fopen(filename, "rt");//open file in backing store
    if(fp == NULL){
		return FILE_DOES_NOT_EXIST;
    }
    int error_code = load_file(fp, start, end, filename, page_table, 2);
    if(error_code != 0){
        fclose(fp);
        return FILE_ERROR;
    }

    //enqueue process pcb into job queue
    PCB* newPCB = makePCB(*start,*end);
    newPCB -> PAGE_TABLE = page_table;
    newPCB -> file_name = filename;

    QueueNode *node = malloc(sizeof(QueueNode));
    node->pcb = newPCB;

    ready_queue_add_to_tail(node);

    fclose(fp);
    return 0;
}

int shell_process_initialize(){
    //Note that "You can assume that the # option will only be used in batch mode."
    //So we know that the input is a file, we can directly load the file into ram
    int* start = (int*)malloc(sizeof(int));
    int* end = (int*)malloc(sizeof(int));
    int page_table[PAGE_TABLE_SIZE];

    //initialize all page table value = -1
    for(int i = 0; i < PAGE_TABLE_SIZE; i++){
        page_table[i] = -1;
    }

    int error_code = 0;
    error_code = load_file(stdin, start, end, "_SHELL", page_table, 2);
    if(error_code != 0){
        return error_code;
    }
    PCB* newPCB = makePCB(*start,*end);
    newPCB->priority = true;
    QueueNode *node = malloc(sizeof(QueueNode));
    node->pcb = newPCB;

    ready_queue_add_to_head(node);

    freopen("/dev/tty", "r", stdin);
    return 0;
}

int transPC(int PC, int* page_table){
    //translate PC to physical memory address
    int pageNumber, frameNumber, physical_addr;

    pageNumber = PC/3;
    frameNumber = page_table[pageNumber];

    if (frameNumber < 0) return -1;
    physical_addr = frameNumber * 3 + PC % 3;

    return physical_addr; // add offset, PC % 3 is offset
}

bool execute_process(QueueNode *node, int quanta){
    char *line = NULL;
    PCB *pcb = node->pcb;
    for(int i=0; i<quanta; i++){
    int physical_addr;

        // line = mem_get_value_at_line(pcb->PC++); 
        physical_addr = transPC(pcb->PC, pcb->PAGE_TABLE);
        if(physical_addr < 0) {
            //page fault
            handlePageFault(pcb);
            return false;
        }
        pcb->PC++;
        line = mem_get_value_at_line(physical_addr);//physical address

        in_background = true;
        if(pcb->priority) {
            pcb->priority = false;
        }
        if(pcb->PC > pcb->end){
            parseInput(line);
            terminate_process(node);
            in_background = false;
            return true;
        }
        parseInput(line);
        in_background = false;
    }
    return false;
}

void *scheduler_FCFS(){
    QueueNode *cur;
    while(true){
        if(is_ready_empty()) {
            if(active) continue;
            else break;   
        }
        cur = ready_queue_pop_head();
        execute_process(cur, MAX_INT);
    }
    return 0;
}

void *scheduler_SJF(){
    QueueNode *cur;
    while(true){
        if(is_ready_empty()) {
            if(active) continue;
            else break;
        }
        cur = ready_queue_pop_shortest_job();
        execute_process(cur, MAX_INT);
    }
    return 0;
}

void *scheduler_AGING_alternative(){
    QueueNode *cur;
    while(true){
        if(is_ready_empty()) {
            if(active) continue;
            else break;
        }
        cur = ready_queue_pop_shortest_job();
        ready_queue_decrement_job_length_score();
        if(!execute_process(cur, 1)) {
            ready_queue_add_to_head(cur);
        }   
    }
    return 0;
}

void *scheduler_AGING(){
    QueueNode *cur;
    int shortest;
    sort_ready_queue();
    while(true){
        if(is_ready_empty()) {
            if(active) continue;
            else break;
        }
        cur = ready_queue_pop_head();
        shortest = ready_queue_get_shortest_job_score();
        if(shortest < cur->pcb->job_length_score){
            ready_queue_promote(shortest);
            ready_queue_add_to_tail(cur);
            cur = ready_queue_pop_head();
        }
        ready_queue_decrement_job_length_score();
        if(!execute_process(cur, 1)) {
            ready_queue_add_to_head(cur);
        }
    }
    return 0;
}

void *scheduler_RR(void *arg){
    int quanta = ((int *) arg)[0];
    QueueNode *cur;
    while(true){
        if(is_ready_empty()){
            if(active) continue;
            else break;
        }
        cur = ready_queue_pop_head();
        if(!execute_process(cur, quanta)) {
            ready_queue_add_to_tail(cur);
        }
    }
    return 0;
}

int schedule_by_policy(char* policy){ //, bool mt){
    if(strcmp(policy, "FCFS")!=0 && strcmp(policy, "SJF")!=0 && 
        strcmp(policy, "RR")!=0 && strcmp(policy, "AGING")!=0 && strcmp(policy, "RR30")!=0){
            return SCHEDULING_ERROR;
    }
    if(active) return 0;
    if(in_background) return 0;
    int arg[1];
    if(strcmp("FCFS",policy)==0){
        scheduler_FCFS();
    }else if(strcmp("SJF",policy)==0){
        scheduler_SJF();
    }else if(strcmp("RR",policy)==0){
        arg[0] = 2;
        scheduler_RR((void *) arg);
    }else if(strcmp("AGING",policy)==0){
        scheduler_AGING();
    }else if(strcmp("RR30", policy)==0){
        arg[0] = 30;
        scheduler_RR((void *) arg);
    }
    return 0;
}

