/**
 * Class: ECSE 427 - Operating Systems
 * Authors: 
 * Ziheng Zhou 260955157
 * Wasif Somji 261003295
*/

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "pcb.h"

#define PCB_STORE_SIZE 3

int pid_counter = 1;
PCB pcb_store[PCB_STORE_SIZE];

int generatePID(){
    return pid_counter++;
}

// helper methods for PCB storage and execution
void initPCBStore(){
    //init PCB.pid at the beginning of execution
    for (int i = 0; i < PCB_STORE_SIZE; i++){
        pcb_store[i].pid = 0;
    }
}

// finds all unused PCB's
int findUnusedPCB(){
    for(int i = 0; i < PCB_STORE_SIZE; i++){
        if(pcb_store[i].pid == 0){
            return i;
        }
    }
    return -1;
}

bool findPCB(PCB** pcb, char* filename){
    // find pcb based on file name
    for(int i = 0; i < PCB_STORE_SIZE; i++){
        if(strcmp(filename, pcb_store[i].file_name)==0){ // if file name matches
            *pcb = &pcb_store[i];
            return true;
        }
    }
    return false;
}

//In this implementation, Pid is the same as file ID 
PCB* makePCB(char* filename){
    int index = findUnusedPCB();
    PCB * newPCB = &pcb_store[index];
    newPCB->pid = generatePID();
    newPCB->PC = 0;
    newPCB->priority = false;
    strcpy(newPCB->file_name, filename);
    return newPCB;
}