/**
 * Class: ECSE 427 - Operating Systems
 * Authors: 
 * Ziheng Zhou 260955157
 * Wasif Somji 261003295
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "interpreter.h"
#include "shellmemory.h"
#include "pcb.h"
#include "kernel.h"

#ifndef SHELL_H
#define SHELL_H
int parseInput(char *ui);
#endif