#ifndef _COMMAND_H
#define _COMMAND_H
#include <stdint.h>


void InitialiseCommandsArray();

//Core Loop
void Run();
void ReactToCommand(char* inputString);


//All command functions
void helpFunction(char* arguments);
void cls(char* arguments);
void exit(char* arguments);
void prompt(char* arguments);
void readdisk(char* arguments);
void ls(char* arguments);
void pwd(char* arguments);
void cd(char* arguments);
void showFileInfo(char* arguments);
void read(char* arguments);
void dbg(char* arguments);


void incorrectFunction(char* arguments);

#endif