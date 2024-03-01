/**
 * Class: ECSE 427 - Operating Systems
 * Authors: 
 * Ziheng Zhou 260955157
 * Wasif Somji 261003295
*/

#include "shell.h"

int MAX_USER_INPUT = 1000;
int parseInput(char ui[]);

void cleanup() {
    // Code to remove the backing store directory
    system("rm -rf ./backing_store*");
    printf("Backing store directory removed.\n");
}

int main(int argc, char *argv[])
{
    printf("%s\n", "Shell v2.0");
    printf("Frame Store Size = %d; Variable Store Size = %d\n", FRAME_STORE_SIZE, VARIABLE_STORE_SIZE);

    char prompt = '$';              // Shell prompt
    char userInput[MAX_USER_INPUT]; // user's input stored here
    int errorCode = 0;              // zero means no error, default

    // init user input
    for (int i = 0; i < MAX_USER_INPUT; i++)
        userInput[i] = '\0';

    // init shell memory
    mem_init();

    // init PCB store
    initPCBStore();

    // Remove all contents in backing store directory if it exists
    system("rm -rf ./backing_store*");

    // Create backing store directory if it does not exist
    system("mkdir ./backing_store");

    while (1)
    {
        if (isatty(fileno(stdin))){
            printf("%c ", prompt);
        }

        char *str = fgets(userInput, MAX_USER_INPUT - 1, stdin);
        if (feof(stdin))
        {
            freopen("/dev/tty", "r", stdin);
        }

        if (strlen(userInput) > 0)
        {
            errorCode = parseInput(userInput);
            if (errorCode == -1)
                exit(99); // ignore all other errors
            memset(userInput, 0, sizeof(userInput));
        }
    }

    atexit(cleanup); 

    return 0;
}

int parseInput(char *ui)
{
    char tmp[200];
    char *words[100];
    memset(words, 0, sizeof(words));

    int a = 0;
    int b;
    int w = 0; // wordID
    int errorCode;
    for (a = 0; ui[a] == ' ' && a < 1000; a++)
        ; // skip white spaces

    while (a < 1000 && a < strlen(ui) && ui[a] != '\n' && ui[a] != '\0')
    {
        while (ui[a] == ' ')
            a++;
        if (ui[a] == '\0')
            break;
        for (b = 0; ui[a] != ';' && ui[a] != '\0' && ui[a] != '\n' && ui[a] != ' ' && a < 1000; a++, b++)
            tmp[b] = ui[a];
        tmp[b] = '\0';
        if (strlen(tmp) == 0)
            continue;
        words[w] = strdup(tmp);
        if (ui[a] == ';')
        {
            w++;
            errorCode = interpreter(words, w);
            if (errorCode == -1)
                return errorCode;
            a++;
            w = 0;
            for (; ui[a] == ' ' && a < 1000; a++)
                ; // skip white spaces
            continue;
        }
        w++;
        a++;
    }
    errorCode = interpreter(words, w);
    return errorCode;
}