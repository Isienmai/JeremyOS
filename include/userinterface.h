#ifndef User_Interface_H
#define User_Interface_H
#include <stdint.h>

//Used to decide how to display each byte
typedef enum
{
	HEX = 0,
	DEC = 1,
	CHAR = 2
} Display_Types;


//Commands to parse user input
void ReadStringFromKeyboard(char* inputString);
void FormatInputString(char* nullTerminatedInput);


//Commands to display a block of data in manageable chunks
void InitialiseDisplayBuffer();
unsigned int DisplayEntireBuffer(uint8_t* buffer, uint16_t size, Display_Types format);
unsigned int DisplayBuffer(uint8_t* buffer, uint16_t size, Display_Types format);


#endif