/**
 * Class: ECSE 427 - Operating Systems
 * Authors: 
 * Ziheng Zhou 260955157
 * Wasif Somji 261003295
*/

#ifndef KERNEL
#define KERNEL

#include "pcb.h"
int process_initialize(char *filename);
int schedule_by_policy(char* policy); //, bool mt);
int shell_process_initialize();
void ready_queue_destory();

int transPC(int PC, int* page_table);
#endif