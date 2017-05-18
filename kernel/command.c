#include <command.h>
#include <keyboard.h>
#include <console.h>
#include <string.h>
#include <floppydisk.h>
#include <ctype.h>
#include <fat12_functions.h>
#include <userinterface.h>

//The command prompt can be a max of 255 characters and is stored in PS1
char PS1[255] = "Command>";
bool _running = true;

//Store commands and their command strings in parallel arrays
//when commands[i] is typed in by the user, the function in commandPtrs[i] will be executed
void (*commandPtrs[255])(char*);
char *commands[255];
int commandNum;

//To add a new command, put its function reference and command string into this list
//Note: all functions must return void and take a char* as an argument
void InitialiseCommandsArray()
{
	commandNum = 0;
	
	commands[commandNum] = "HELP";
	commandPtrs[commandNum] = &helpFunction;
	++commandNum;
	
	commands[commandNum] = "CLS";
	commandPtrs[commandNum] = &cls;
	++commandNum;
	
	commands[commandNum] = "EXIT";
	commandPtrs[commandNum] = &exit;
	++commandNum;
	
	commands[commandNum] = "PROMPT";
	commandPtrs[commandNum] = &prompt;
	++commandNum;
	
	commands[commandNum] = "READDISK";
	commandPtrs[commandNum] = &readdisk;
	++commandNum;
	
	//LS and DIR are synonyms
	commands[commandNum] = "LS";
	commandPtrs[commandNum] = &ls;
	++commandNum;
	commands[commandNum] = "DIR";
	commandPtrs[commandNum] = &ls;
	++commandNum;
	
	commands[commandNum] = "PWD";
	commandPtrs[commandNum] = &pwd;
	++commandNum;
	
	commands[commandNum] = "CD";
	commandPtrs[commandNum] = &cd;
	++commandNum;
	
	commands[commandNum] = "FILE";
	commandPtrs[commandNum] = &showFileInfo;
	++commandNum;
	
	commands[commandNum] = "READ";
	commandPtrs[commandNum] = &read;
	++commandNum;
	
	commands[commandNum] = "DBG";
	commandPtrs[commandNum] = &dbg;
	++commandNum;
	
	
	//Include a function to be called if the command string doesn't match any other
	//This function MUST be last in the list and commandNum must NOT be incremented after it
	//This is so that it can be called by executing commandPtrs[commandNum]
	commands[commandNum] = "";
	commandPtrs[commandNum] = &incorrectFunction;
}

//Core loop: 
//	Display the command prompt
//	Read the next command from the console
//	Execute said command
void Run() 
{
	InitialiseCommandsArray();
	char inputString[255] = "";
	while(_running)
	{
		ConsoleWriteString(PS1);
		ReadStringFromKeyboard(inputString);
		ReactToCommand(inputString);
	}
}


void ReactToCommand(char* inputString)
{
	//Split the input into a command and its arguments by separating the string at the first space character
	char command[255] = "";
	char arguments[255] = "";
		
	uint32_t i = 0;
	while(inputString[i] != ' ' && inputString[i] != 0)
	{
		command[i] = inputString[i];
		++i;
	}
	
	if(inputString[i] != 0)
	{
		//skip past multiple spaces
		while(inputString[i] == ' ') ++i;
	
		uint32_t argumentStart = i;
		
		while(inputString[i] != 0)
		{
			arguments[i - argumentStart] = inputString[i];
			++i;
		}
	}	
	
	
	//Locate the function which matches the requested command and execute it
	//If no function matches, execute the last function in the list
	for(i = 0; i < commandNum; ++i)
	{
		if(strcmp(command, commands[i]) == 0)
		{
			break;
		}
	}
	
	(*commandPtrs[i])(arguments);
}


//List the available commands
void helpFunction(char* arguments)
{
	ConsoleWriteString("The following commands are available:\n");
	for(int i = 0; i < commandNum; ++i)
	{
		ConsoleWriteString(commands[i]);
		ConsoleWriteCharacter('\n');
	}
}

//Clear the screen
void cls(char* arguments)
{
	ConsoleClearScreen(ConsoleGetCurrentColour());
}

//Shutdown the OS
void exit(char* arguments)
{
	_running = false;
	ConsoleWriteString("Shutting down...");
}

//Change the command prompt to match "arguments"
void prompt(char* arguments)
{
	//Make sure the argument is valid
	if(arguments[0] == 0) 
	{
		ConsoleWriteString("Invalid argument!\n");
		return;
	}
	
	//update the command prompt
	uint32_t j = 0;
	while(j < 255 && (PS1[j] != 0 || arguments[j] != 0))
	{
		PS1[j] = arguments[j];
		++j;
	}
}

//Display the requested sector in the requested format (default to sector 0 in HEX)
void readdisk(char* arguments)
{	
	//Argument variables	
	uint32_t sectorToRead = 0;
	Display_Types displayAs = HEX;
	
	//Update argument variables to match the provided arguments
	char argument[255] = "";
	uint32_t index;
	
	while(*arguments != 0)
	{
		index = 0;
		//pass over any possible gap at the start of the arguments
		while(*arguments != ' ' && *arguments != '\0')
		{
			argument[index++] = *(arguments++);
		}
		argument[index] = 0;
		
		//compare the argument to the possible valid arguments
		if(strcmp(argument, "-c") == 0)
		{
			displayAs = CHAR;
			ConsoleWriteString("Displaying as characters.\n");
		}
		else if(strcmp(argument, "-d") == 0)
		{
			displayAs = DEC;
			ConsoleWriteString("Displaying as decimal.\n");
		}
		else
		{
			sectorToRead = strToInt(argument);
			ConsoleWriteString("Reading Sector: ");
			ConsoleWriteInt(sectorToRead, 10);
			ConsoleWriteCharacter('\n');
		}
		
		arguments++;
	}
	
	
	//Read the requested sector and display it, periodically asking the user if they wish to cancel
	uint8_t* sectorContents = FloppyDriveReadSector(sectorToRead);		
	ConsoleWriteCharacter('\n');
	InitialiseDisplayBuffer();
	DisplayEntireBuffer(sectorContents, 512, displayAs);
	
	ConsoleWriteString("\n");
}

//list all directory entries in the current directory
void ls(char* arguments)
{	
	FsFat12_DisplayAllCurrentDirectoryEntries();
}

//display the full filepath of the current directory
void pwd(char* arguments)
{	
	ConsoleWriteString(FsFat12_GetCurrentDirectoryName());
	
	ConsoleWriteCharacter('\n');
}

//navigate to a new current directory
void cd(char* arguments)
{	
	//reformat all filepaths
	FormatInputString(arguments);
	
	FsFat12_ChangeDirectory(arguments);
}

//display all variables in the FILE specified by the input filepath
void showFileInfo(char* arguments)
{
	//reformat all filepaths
	FormatInputString(arguments);
	
	FsFat12_GetEntryInfo(arguments);
	ConsoleWriteCharacter('\n');
}


//display the contents of the specified file as text
char dataBuffer[2048];
uint16_t bufferSize = 2048;
void read(char* arguments)
{
	//reformat all filepaths
	FormatInputString(arguments);
	
	
	//	OPEN THE FILE
	
	FILE openedFile = FsFat12_Open(arguments);
	
	if(openedFile.Flags != FS_FILE)
	{
		ConsoleWriteString("Specified File is not valid!\n");
		return;
	}
	
	if(openedFile.FileLength == 0)
	{
		ConsoleWriteString("Specified File is empty.\n");
		return;
	}
	
	
	//	CONTINUOUSLY READ THE FILE AND DISPLAY IT UNTIL THE FILE ENDS OR THE USER CANCELS
		
	InitialiseDisplayBuffer();
	
	unsigned int keepDisplaying = 1;
	while(openedFile.Eof != 1 && keepDisplaying == 1)
	{
		FsFat12_Read(&openedFile, dataBuffer, bufferSize);
				
		keepDisplaying = DisplayEntireBuffer(dataBuffer, bufferSize, CHAR);
	}
	
	
	ConsoleWriteCharacter('\n');
}

//NOTE: this command is used to call various testing functions
void dbg(char* arguments)
{
	//TESTGetFatEntry(2);
	//TESTGetFatEntry(83);
	
	
	//TESTReadCluster(483);
	//TESTReadCluster(0);
	
	
	//TESTGetNextClusterOfCurrentFile(483, 1);
	//TESTGetNextClusterOfCurrentFile(483, 2);
	//TESTGetNextClusterOfCurrentFile(483, 3);
	//TESTGetNextClusterOfCurrentFile(488, 1);
	//TESTGetNextClusterOfCurrentFile(0, 1);
	
	//TESTExtractFileNameAndExtension("test.zip/testingtoolong.zip/test.toolong/./../...../hi.my.bro/1.bp");
	
	
	//test in ROOT directory
	//TESTGetDirectoryEntryByIndex(0,0);
	//TESTGetDirectoryEntryByIndex(4,0);
	//TESTGetDirectoryEntryByIndex(20,0);
	//TESTGetDirectoryEntryByIndex(-2,0);	
	//test in TESTING directory
	//TESTGetDirectoryEntryByIndex(0,510);
	//TESTGetDirectoryEntryByIndex(4,510);
	//TESTGetDirectoryEntryByIndex(20,510);
	//TESTGetDirectoryEntryByIndex(-2,510);	
	//test in TESTING/SECLOR.TXT
	//TESTGetDirectoryEntryByIndex(0,931);
	//TESTGetDirectoryEntryByIndex(4,931);
	//TESTGetDirectoryEntryByIndex(20,931);
	//TESTGetDirectoryEntryByIndex(-2,931);	
	//test invalid clusters
	//TESTGetDirectoryEntryByIndex(0,-4);
	//TESTGetDirectoryEntryByIndex(0,2000);
	//TESTGetDirectoryEntryByIndex(0,4000);
	
	
	//test in ROOT directory
	//TESTGetDirectoryEntryWithName("ALLSTAR.TXT",0);
	//TESTGetDirectoryEntryWithName("SECLOR.TXT",0);
	//TESTGetDirectoryEntryWithName("KERNEL.SYS",0);
	//TESTGetDirectoryEntryWithName("KERNEL",0);
	//TESTGetDirectoryEntryWithName("TESTING",0);
	//TESTGetDirectoryEntryWithName("TESTING.BLA",0);
	//TESTGetDirectoryEntryWithName("..",0);
	//TESTGetDirectoryEntryWithName("CRIM    7.TXT",0);
	//TESTGetDirectoryEntryWithName("CRIM.TXT",0);
	//test in TESTING directory
	//TESTGetDirectoryEntryWithName("ALLSTAR.TXT",510);
	//TESTGetDirectoryEntryWithName("SECLOR.TXT",510);
	//TESTGetDirectoryEntryWithName("KERNEL.SYS",510);
	//TESTGetDirectoryEntryWithName("KERNEL",510);
	//TESTGetDirectoryEntryWithName("TESTING",510);
	//TESTGetDirectoryEntryWithName("TESTING.BLA",510);
	//TESTGetDirectoryEntryWithName("..",510);
	//TESTGetDirectoryEntryWithName("CRIM    7.TXT",510);
	//TESTGetDirectoryEntryWithName("CRIM.TXT",510);
	//test in invalid clauster
	//TESTGetDirectoryEntryWithName("ALLSTAR.TXT",-4);
	//TESTGetDirectoryEntryWithName("ALLSTAR.TXT",2000);
	//TESTGetDirectoryEntryWithName("ALLSTAR.TXT",4000);
	//TESTGetDirectoryEntryWithName("ALLSTAR.TXT",931);
	
	
	//test valid paths
	//TESTGetNestedDirectoryEntry("CRIM.TXT");
	//TESTGetNestedDirectoryEntry("TDIR2/TESTING/TD2TFIL.TXT");
	//TESTGetNestedDirectoryEntry("TESTING/ALLSTAR.TXT");
	//TESTGetNestedDirectoryEntry("TESTING/INNER3/EMPTY.TXT");
	//TESTGetNestedDirectoryEntry("TESTING/INNER4");
	//TESTGetNestedDirectoryEntry("INNER4");	
	//test starting at root
	//TESTGetNestedDirectoryEntry("/CRIM.TXT");
	//TESTGetNestedDirectoryEntry("/TDIR2/TESTING/TD2TFIL.TXT");
	//TESTGetNestedDirectoryEntry("/TESTING/ALLSTAR.TXT");
	//TESTGetNestedDirectoryEntry("/TESTING/INNER3/EMPTY.TXT");
	//TESTGetNestedDirectoryEntry("/TESTING/INNER4");
	//TESTGetNestedDirectoryEntry("/INNER4");
	//test the . and .. directories
	//TESTGetNestedDirectoryEntry("../TDIR2/CRIM.TXT");
	//TESTGetNestedDirectoryEntry("../TDIR2/./TESTING/TD2TFIL.TXT");
	//TESTGetNestedDirectoryEntry("./ALLSTAR.TXT");
	//TESTGetNestedDirectoryEntry("./../TDIR2/../TESTING/INNER3/EMPTY.TXT");
	//TESTGetNestedDirectoryEntry("../TESTING /./.././TESTING/INNER4");
	//test treating files as directories and vice versa
	//TESTGetNestedDirectoryEntry("TDIR2.ZIP/CRIM.TXT");
	//TESTGetNestedDirectoryEntry("TDIR2/./TESTING/TD2TFIL");
	//TESTGetNestedDirectoryEntry("ALLSTAR");
	//TESTGetNestedDirectoryEntry("TDIR2/../TESTING/INNER3.BIZ/EMPTY.TXT");
	//TESTGetNestedDirectoryEntry("TESTING/./../TESTING/INNER4");
	//TESTGetNestedDirectoryEntry("TESTING/CRIM.TXT/INNER4");
	
	
	//SetCurrentDirPath(arguments);
	//pwd(arguments);
	
	
	
	ConsoleWriteCharacter('\n');
}

//Display an error in the event of an unrecognised command
void incorrectFunction(char* arguments)
{
	ConsoleWriteString("Invalid command (Note: Commands must be UPPERCASE). Type \"HELP\" for a list of available commands.\n");
}