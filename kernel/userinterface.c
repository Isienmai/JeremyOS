#include <userinterface.h>
#include <keyboard.h>
#include <console.h>


//Reads input from the keyboard, returning on Enter
//inputString is a buffer to be filled with the user input
//Note that inputs longer than 255 characters are cropped down to 255 characters
void ReadStringFromKeyboard(char* inputString)
{
	uint32_t currentCharIndex = 0;
	keycode lastKey = 0;
	
	//Loop until enter
	bool enterHit = false;
	while(!enterHit)
	{
		lastKey = KeyboardGetCharacter();
		switch(lastKey)
		{
			case KEY_RETURN:
				ConsoleWriteCharacter(KEY_RETURN);
				ConsoleWriteCharacter('\n');
				
				//Make sure all input is null terminated
				inputString[currentCharIndex] = 0;
				enterHit = true;
				break;
			case KEY_BACKSPACE:
				if(currentCharIndex != 0)
				{
					ConsoleWriteCharacter(KEY_BACKSPACE);
					
					//update input string and console display
					inputString[currentCharIndex] = 0;
					ConsoleWriteCharacter(' ');
					
					ConsoleWriteCharacter(KEY_BACKSPACE);
					--currentCharIndex;
				}
				break;
			default:
				//Only react to valid ascii characters (ignore shift, caps lock, etc)
				if(KeyboardConvertKeyToASCII(lastKey))
				{					
					inputString[currentCharIndex] = KeyboardConvertKeyToASCII(lastKey);
					ConsoleWriteCharacter(inputString[currentCharIndex]);
					++currentCharIndex;
					
					//do not allow more than 255 characters
					if(currentCharIndex > 255) --currentCharIndex;
				}
				break;
		}	
	}	
}


//modify a null terminated string to match processing requirements
//specifically, all characters are to be UPPERCASE, and '\' must be replaced with '/'
void FormatInputString(char* nullTerminatedInput)
{
	int index = 0;
	
	//loop through each character, modifying it if necessary
	while(nullTerminatedInput[index] != 0)
	{
		//force all letters to uppercase
		if(	nullTerminatedInput[index] > 96 && 
			nullTerminatedInput[index] < 123 )
		{
			nullTerminatedInput[index] -= 32;
		}
		//replace all '\' with '/'
		else if( nullTerminatedInput[index] == '\\')
		{
			nullTerminatedInput[index] = '/';
		}
		
		++index;
	}
}



//Store the number of lines that have been written to
uint32_t displayBufferLineCount = 0;

//Reset the DisplayBuffer() function to start at 0 lines written so far
void InitialiseDisplayBuffer()
{
	displayBufferLineCount = 0;
}

//given a buffer, use DisplayBuffer() to write it to the screen. returning 0 if the user cancels mid-display
unsigned int DisplayEntireBuffer(uint8_t* buffer, uint16_t size, Display_Types format)
{	
	keycode lastKey = 0;
	int bytesShown = 0;		
	while(bytesShown < size)
	{	
		//display the buffer until its end is reached or a certain number of lines have been written to the screen
		bytesShown += DisplayBuffer(buffer + bytesShown, size - bytesShown, format);
					
		//if the entire buffer hasn't been displayed, let the user decide whether or not to keep going
		if(bytesShown < size)
		{			
			ConsoleWriteString("\nEnter to see more, Q to quit.\n");
			while(lastKey != KEY_RETURN && lastKey != KEY_Q)
			{
				lastKey = KeyboardGetCharacter();
			}			
			if(lastKey == KEY_Q)
			{
				return 0;
			}	
			lastKey = 0;
		}		
	}
	
	return 1;
}

//display the contents of a buffer, stopping after writing a certain number of lines (to stop the data from scrolling offscreen)
//return the number of bytes successfully displayed
unsigned int DisplayBuffer(uint8_t* buffer, uint16_t size, Display_Types format)
{	
	//Keep track of the cursor position
	unsigned xPos = 0,yPos = 1;
	ConsoleGetXY(&xPos,&yPos);
	uint32_t lastXpos = xPos;
	
	//store the max number of lines that can be written to screen
	uint8_t maxLinesAtOnce = ConsoleGetHeight() - 15;
	
	//loop through the buffer displaying characters until the buffer has been exhausted, or the number of lines written has exceeded the above value
	unsigned int byteIndex = 0;
	for(;byteIndex < size; ++byteIndex)
	{
		switch(format)
		{
			case HEX:
				ConsoleWriteIntMinLength(buffer[byteIndex], 16, 2);
				break;
			case DEC:
				ConsoleWriteInt(buffer[byteIndex], 10);
				break;
			case CHAR:
				ConsoleWriteCharacter(buffer[byteIndex]);
				break;
		}		
		
		
		//Keep track of the number of lines that have been written
		ConsoleGetXY(&xPos,&yPos);
		if(lastXpos > xPos)
		{
			++displayBufferLineCount;
		}

		//If too many lines have been written, RETURN EARLY
		if(displayBufferLineCount >= maxLinesAtOnce)
		{	
			displayBufferLineCount = 0;
			return byteIndex;
		}
		
		//update the last X position
		lastXpos = xPos;
	}
	
	return byteIndex;
}
