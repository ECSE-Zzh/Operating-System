/**
 * Class: ECSE 427 - Operating Systems
 * Authors: 
 * Ziheng Zhou 260955157
 * Wasif Somji 261003295
*/

#ifndef INTERPRETER_H
#define INTERPRETER_H
#include <stdbool.h>

int interpreter(char* command_args[], int args_size);
int my_cd(char* dirname);
int help();
int pageReplacementPolicy(char* page_replacement_policy);
char* getPageReplacementPolicy();
bool userSetPageReplacementPolicy();

enum Error {
	NO_ERROR,
	FILE_DOES_NOT_EXIST,
	FILE_ERROR,
	NO_MEM_SPACE,
	READY_QUEUE_FULL,
	SCHEDULING_ERROR,
	TOO_MANY_TOKENS,
	TOO_FEW_TOKENS,
	NON_ALPHANUMERIC_TOKEN,
	BAD_COMMAND,
	ERROR_CD,
	ERROR_MKDIR,
};

#endif