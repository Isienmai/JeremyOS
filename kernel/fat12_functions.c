#include <fat12_functions.h>
#include <bpb.h>
#include <console.h>
#include <floppydisk.h>
#include <_null.h>
#include <string.h>


// These macros are sets of boolean expressions used to determine the validity of certain values

#define INVALID_FILENAME(c) (c == 0x0)
#define DELETED_FILENAME(c) (c == 0xe5)

//treat special clusters differently (can't use their values as normal FAT values)
#define SPECIAL_CLUSTER(n) (n >= 0xFF0 || n == 0x0)
//any cluster number that is too low or too high cannot be properly handled by the FAT
#define INVALID_CLUSTER(n) ((n < 2) || (n >=(BIOSParamBlc.SectorsPerFat * BIOSParamBlc.BytesPerSector * 2)/3))


//These variables are set up in the initialise method to be used without modification by the remaining methods
BIOSParameterBlock BIOSParamBlc;
BIOSParameterBlockExt BIOSParamBlcExt;

int FATSector;
int rootSector;
int dataSector;

DirectoryEntry rootDirectory;
DirectoryEntry invalidDirectory;


//These variables keep track of data pertaining to the current directory
char currentDirectoryName[255];
int currentDirStrLen;
DirectoryEntry currentDirectory;


void FsFat12_Initialise()
{
	//Copy the BIOSParameter information into memory for future use
	pBootSector bootSectorStart = (pBootSector)FloppyDriveReadSector(0);
	BIOSParamBlc = bootSectorStart->Bpb;
	BIOSParamBlcExt = bootSectorStart->BpbExt;	
	
	
	//store the index of the FAT, root, and data sectors
	FATSector = BIOSParamBlc.ReservedSectors;
	rootSector = FATSector + BIOSParamBlc.SectorsPerFat*BIOSParamBlc.NumberOfFats;
		
	//calculate the number of sectors occupied by the root directory (rounding up)
	int numOfRootSectors = BIOSParamBlc.NumDirEntries * sizeof(DirectoryEntry);
	numOfRootSectors = (numOfRootSectors + BIOSParamBlc.BytesPerSector - 1)/BIOSParamBlc.BytesPerSector;
	
	dataSector = rootSector + numOfRootSectors;
	
	
			//DEBUG THE CALCULATED SECTOR POSITIONS
			/*ConsoleWriteString("FAT Sector: "); ConsoleWriteInt(FATSector, 10);
			ConsoleWriteString("\nroot Sector: "); ConsoleWriteInt(rootSector, 10);
			ConsoleWriteString("\ndata Sector: "); ConsoleWriteInt(dataSector, 10);
			ConsoleWriteCharacter('\n');*/
	
	
	//initialise the root directory entry
	strncpy(rootDirectory.Filename, "/       ", 8);
	strncpy(rootDirectory.Ext, "   ", 3);
	rootDirectory.Attrib = DE_SUBDIR;
	rootDirectory.Reserved = 0;
	rootDirectory.TimeCreatedMs = 0;
	rootDirectory.TimeCreated = 0;
	rootDirectory.DateCreated = 0;
	rootDirectory.DateLastAccessed = 0;
	rootDirectory.FirstClusterHiBytes = 0;
	rootDirectory.LastModTime = 0;
	rootDirectory.LastModDate = 0;
	rootDirectory.FirstCluster = 0;
	rootDirectory.FileSize = 0;
	
	
	//default to the root directory
	currentDirectoryName[0] = '/';
	currentDirectoryName[1] = 0;
	currentDirStrLen = 1;
	currentDirectory = rootDirectory;
	
	//a directory is invalide if it's filename begins with the null terminator
	invalidDirectory.Filename[0] = 0;
}

//return the FAT value associated with the input cluster
uint16_t FsFat12_GetFATEntry(int clusterNum)
{	
	if(INVALID_CLUSTER(clusterNum)) return NULL;
	
	//Get the indices of the two bytes that contain the requested FAT entry
	int firstByteIndex = ((clusterNum/2) * 3) + clusterNum%2;
	int secondByteIndex = firstByteIndex + 1;	
		
	//Find the sectors of the FAT which contains the two bytes being searched for
	int firstSector = firstByteIndex/BIOSParamBlc.BytesPerSector;
	int secondSector = secondByteIndex/BIOSParamBlc.BytesPerSector;
	
	//Read the two bytes from disk
	uint8_t* readInSector = FloppyDriveReadSector(FATSector + firstSector);	
	uint8_t firstByte = *(readInSector + firstByteIndex%BIOSParamBlc.BytesPerSector);	
	if(firstSector != secondSector) readInSector = FloppyDriveReadSector(FATSector + secondSector);
	uint8_t secondByte = *(readInSector + secondByteIndex%BIOSParamBlc.BytesPerSector);
		
	
	//extract the result from the two bytes
	uint16_t finalValue;
	if(clusterNum%2 == 0)
	{
		finalValue = firstByte & 0x00ff;
		finalValue |= ((uint16_t)secondByte & 0x000f) << 8;
	}
	else
	{			
		finalValue = ((uint16_t)firstByte & 0x00f0) >> 4;
		finalValue |= ((uint16_t)secondByte & 0xff) << 4;
	}
	
	return finalValue;
}

//read the requested cluster from the disk
//NOTE: This function assumes clusters and sectors are the same size
uint8_t* FsFat12_ReadCluster(int cluster)
{
	if(INVALID_CLUSTER(cluster)) return NULL;
	
	return FloppyDriveReadSector(dataSector + cluster - 2);
}


//return a file's n'th cluster (where n is the "clusterNumber" argument, and the file is determined by the firstCluster)
//NOTE: This function assumes clusters and sectors are the same size
uint8_t* FsFat12_GetNextClusterOfCurrentFile(int firstCluster, int clusterNumber)
{
	uint16_t currentCluster = firstCluster;
	
	//Loop through the sectors, starting at the input origin sector, until "clusterNumber" has been reached or the list of sectors ends
	for(int i = 0; i < clusterNumber; ++i)
	{
		currentCluster = FsFat12_GetFATEntry(currentCluster);
		
		if(SPECIAL_CLUSTER(currentCluster)) return NULL;
	}
	
	return FsFat12_ReadCluster(currentCluster);
}

//from a given file identifier (name.extension) extract the name
char temporaryNameBuffer[9];
char* ExtractFileName(const char* nameAndExt)
{	
	//reset the name buffer
	temporaryNameBuffer[0] = '\0';	
	int index = 0;
	
	//read the filename into the name buffer	
	if(nameAndExt[index] != '.')
	{
		while(	nameAndExt[index] != '/' && 
				nameAndExt[index] != 0 && 
				nameAndExt[index] != '.' && 
				nameAndExt[index] != ' ' &&
				index < 9)
		{
			temporaryNameBuffer[index] = nameAndExt[index];
			++index;
		}
		
		//if the filename was too long then it was invalid
		if(index == 9) return NULL;
	}
	//treat files beginning with a dot differently
	else
	{		
		//special case for . and .. files
		temporaryNameBuffer[index] = '.';
		++index;
		
		if(nameAndExt[index] == '.')
		{			
			temporaryNameBuffer[index] = '.';
			++index;
		}
		
		//all other files beginning with a dot are invalid
		if(nameAndExt[index] != '/' && nameAndExt[index] != 0) return NULL;
	}
	
	
	//pad the valid filename with spaces
	while(index < 8)
	{
		temporaryNameBuffer[index] = ' ';
		++index;
	}
	
	return temporaryNameBuffer;
}

//from a given file identifier (name.extension) extract the extension
char temporaryExtBuffer[4];
char* ExtractFileExtension(const char* nameAndExt)
{
	//clear the extension buffer
	temporaryExtBuffer[0] = '\0';
		
	
	//move to the end of the name (the start of the extension)
	int extensionStart = 0;
	while(nameAndExt[extensionStart] == '.' && extensionStart < 9){++extensionStart;} //treat initial dots differently to the separating dot
	 
	while(	nameAndExt[extensionStart] != '.' &&
			nameAndExt[extensionStart] != '/' &&
			nameAndExt[extensionStart] != 0 &&
			extensionStart < 9)
	{
		++extensionStart;
	}
	
	
	//if extension beginning is past its max index (8) return error
	if(extensionStart >= 9) return NULL;
	
	
	//if there is no separating dot, return a blank file extension
	if(nameAndExt[extensionStart] != '.')
	{
		strncpy(temporaryExtBuffer, "   ", 3);
		return temporaryExtBuffer;
	}
	
	++extensionStart;
	
	//if there is no file extension, in spite of the separating dot, return error
	if(	nameAndExt[extensionStart] == 0 ||
		nameAndExt[extensionStart] == '/' ||
		nameAndExt[extensionStart] == ' ') return NULL;
	
	//copy the extension to the buffer
	int index = 0;
	while(	nameAndExt[extensionStart] != 0 &&
				nameAndExt[extensionStart] != '/' &&
				index < 4)
	{
		temporaryExtBuffer[index] = nameAndExt[extensionStart];
		++index;
		++extensionStart;
	}
	
	//if the extension was too long, error
	if(index == 4) return NULL;
	
	//pad with spaces before returning
	while( index < 3)
	{
		temporaryExtBuffer[index] = ' ';
		++index;
	}
	return temporaryExtBuffer;
}


//return the n'th directoryEntry (specified by dirIndex) in the directory specified by sourceDirInitialCluster
DirectoryEntry FsFat12_GetDirectoryEntryByIndex(int dirIndex, int sourceDirInitialCluster)
{	
	//NOTE: It has been assumed that each DirectoryEntry will always be entirely contained within a single sector.
	//		This is because each entry is 32 bytes, whilst sectors are 512. Hence each sector contains exactly 16 entries.


	//calculate the first byte and sector of the intended entry
	int byteOffset = dirIndex * sizeof(DirectoryEntry);	
	int clusterOffset = byteOffset/BIOSParamBlc.BytesPerSector;
		
	//read in the relevant sector from the floppy, treating the root directory differently to sub directories
	uint8_t* sector;
	if(sourceDirInitialCluster != 0) 
		sector = FsFat12_GetNextClusterOfCurrentFile(sourceDirInitialCluster, clusterOffset);
	else 
		sector = FloppyDriveReadSector(rootSector + clusterOffset);
	
	if(sector == NULL) return invalidDirectory;
	
	//calculate the offset within the current sector
	byteOffset = byteOffset % BIOSParamBlc.BytesPerSector;
	
	//Move the pointer to the requested index
	sector += byteOffset;
	
	return *((DirectoryEntry*)sector);
}

//return the directory entry with the specified name and extension from the directory starting in sector "sourceDirInitialSector"
DirectoryEntry FsFat12_GetDirectoryEntryWithName(const char* directoryEntry, int sourceDirInitialSector)
{				
	char* entryName;
	char* extension;	

	//split into name and extension
	entryName = ExtractFileName(directoryEntry);
	if(entryName == NULL) return invalidDirectory;
	
	extension = ExtractFileExtension(directoryEntry);
	if(extension == NULL) return invalidDirectory;

	DirectoryEntry tempDirEntry;
	
	//loop until the directory entry matches the requested name AND extension, or until the entry is invalid
	int index = 0;	
	do
	{
		tempDirEntry = FsFat12_GetDirectoryEntryByIndex(index++, sourceDirInitialSector);				
		
	}while((strncmp(entryName, tempDirEntry.Filename, 8) != 0 ||
			strncmp(extension, tempDirEntry.Ext, 3) != 0) &&
			!INVALID_FILENAME(tempDirEntry.Filename[0]));
	
	return tempDirEntry;
}


//traverse the directory structure to the location specified in the path
//return the specified directory entry at that location
DirectoryEntry FsFat12_GetNestedDirectoryEntry(const char* nestedDirPath)
{
	//start in the currentDirectory
	DirectoryEntry toReturn = currentDirectory;		
	int pathCharIndex = 0;
	
	
	//special case if the filepath starts in ROOT
	if(nestedDirPath[pathCharIndex] == '/')
	{
		toReturn = rootDirectory;
		++pathCharIndex;		
	}
	
	
	char tempName[20];
	
	//Look at the filenames in the full filepath one by one, and traverse them if they are valid
	while(nestedDirPath[pathCharIndex] != '\0')
	{
		//extract the next directory entry name from the full path
		int index = 0;
		while(	nestedDirPath[pathCharIndex] != '/' &&
				nestedDirPath[pathCharIndex] != 0 &&	
				index < 19)
		{
			tempName[index++] = nestedDirPath[pathCharIndex++];
		}		
		tempName[index] = 0;	
		
		
		//get the next directory specified in the filepath
		toReturn = FsFat12_GetDirectoryEntryWithName(tempName, toReturn.FirstCluster);
		if(INVALID_FILENAME(toReturn.Filename[0])) return invalidDirectory;
		
		
		//move the index past the filename separator
		if(nestedDirPath[pathCharIndex] != 0) ++pathCharIndex;
	}
		
	return toReturn;
}


//given a local directory identifier, update the current working directory string to match this new directory
//ASSUMES "newFilepath" IS A STANDARD DIRECTORY ENTRY FILENAME (8 chars, padded with spaces)
void UpdateCurrentDirName(const char* newFilepath)
{
	//current directory means no change required
	if(strncmp(newFilepath, ".       ", 8) == 0) return;
	
	
	//root directory should reset the path
	if(newFilepath[0] == '/')
	{
		currentDirectoryName[0] = '/';
		currentDirectoryName[1] = 0;
		currentDirStrLen = 1;
		return;
	}
	
	//parent directory should erase the last file identifier in the path, then return early
	if(strncmp(newFilepath, "..      ", 8) == 0)
	{
		//work back from the last character and insert a null terminator at the start of the last subdir name
		int i = currentDirStrLen;		
		while(currentDirectoryName[i] != '/' && i > 1)
		{
			--i;
		}		
		currentDirectoryName[i] = 0;
		currentDirStrLen = i;
		return;
	}
		
	//insert the separator at the end of the current filepath if it isn't already there
	if(currentDirectoryName[currentDirStrLen - 1] != '/') currentDirectoryName[currentDirStrLen++] = '/';
	
	//Append the provided filename (without spaces) to the current working directory string
	int i = 0;		
	while(newFilepath[i] != ' ' && i < 8)
	{
		currentDirectoryName[currentDirStrLen + i] = newFilepath[i];
		++i;
	}
	currentDirectoryName[currentDirStrLen + i] = 0;
	
	currentDirStrLen += i;
}

//given a full filepath, update the currentWorkingDirectory string
void SetCurrentDirPath(const char* newPath)
{
	int index = 0;
	char temp[8];
	
	//if the filepath starts in the root directory, reset to the root directory
	if(newPath[index] == '/')
	{
		UpdateCurrentDirName("/");
		++index;
	}		
	
	//loop through each file specifier, updating the current dir string for each one
	while(newPath[index] != 0)
	{
		//extract the full 8 character subdir name from the filepath
		for(int i = 0; i < 8; ++i)
		{
			if(newPath[index] != '/' && newPath[index] != 0)
			{
				temp[i] = newPath[index];
				++index;
			}
			else
			{
				temp[i] = ' ';
			}
		}
		
		//append it to the current directory string
		UpdateCurrentDirName(temp);
		
		//skip over the filename separator
		if(newPath[index] == '/') ++index;
	}
}


//given a filepath, retreive the directory entry it refers to and convert it into a FILE that is returned
FILE FsFat12_Open(const char* filename)
{
	// GET THE DIRECTORY ENTRY
	
	DirectoryEntry resultingEntry = FsFat12_GetNestedDirectoryEntry(filename);
		
	FILE toReturn;	
	if(INVALID_FILENAME(resultingEntry.Filename[0]))
	{
		toReturn.Flags = FS_INVALID;
		return toReturn;
	}	
	
	
	// SET UP THE FILE STRUCTURE
	
	//copy the name across, until the first space
	int i = 0;
	for(; i < 8; ++i)
	{
		if(resultingEntry.Filename[i] == ' ') break;	
		
		toReturn.Name[i] = resultingEntry.Filename[i];
	}	
	toReturn.Name[i] = 0;
		
	//handle subdirectories and files differently
	if((resultingEntry.Attrib & DE_SUBDIR) == DE_SUBDIR)
	{
		toReturn.Flags = FS_DIRECTORY;
	}
	else
	{
		toReturn.Flags = FS_FILE;
		
		//copy across the file extension, up to the first space
		toReturn.Name[i++] = '.';
		int j = 0;
		for(; j < 3; ++j)
		{
			if(resultingEntry.Ext[j] == ' ') break;	
			
			toReturn.Name[i + j] = resultingEntry.Ext[j];
		}		
		toReturn.Name[i + j] = '\0';		
	}
		
	toReturn.FileLength = resultingEntry.FileSize;	
	toReturn.CurrentCluster = resultingEntry.FirstCluster;
	
	toReturn.Eof = 0;
	toReturn.Position = 0;
	
	return toReturn;
}

unsigned int FsFat12_Read(PFILE file, unsigned char* buffer, unsigned int length)
{	
	//ASSUMPTIONS:
	//				1 cluster = 1 sector
	//				length is never larger than buffer

	//store the last read sector
	uint8_t* sector;
	int amountToRead;
	int totalRead = 0;
	int remainingDist;
	
	//loop through the number of sectors taken up by length
	while(totalRead < length)
	{
		sector = FsFat12_ReadCluster(file->CurrentCluster);
		
		//if the amount space left to fill is less than a sector, only copy across the remainder
		remainingDist = length - totalRead;
		if(file->Position + remainingDist < BIOSParamBlc.BytesPerSector)
		{
			amountToRead = remainingDist;
		}
		else
		{
			amountToRead = BIOSParamBlc.BytesPerSector - file->Position;	
		}
		//copy the appropriate amount of data from the read in sector to the end of the buffer		
		memcpy(buffer, sector + file->Position, amountToRead);
		
		//keep track of how much data has been read in
		file->Position += amountToRead;
		buffer += amountToRead;	
		totalRead += amountToRead;
		if(file->Position >= BIOSParamBlc.BytesPerSector)
		{			
			file->CurrentCluster = FsFat12_GetFATEntry(file->CurrentCluster);
			file->Position -= BIOSParamBlc.BytesPerSector;
		}		
		
		//if the final cluster has been reached then close the file, fill the rest of the buffer with null characters, and return
		if(SPECIAL_CLUSTER(file->CurrentCluster))
		{
			FsFat12_Close(file);			
			memset(buffer, 0, length - totalRead);			
			return totalRead;
		}
	}	
	
	return totalRead;
}

//close a given file
void FsFat12_Close(PFILE file)
{
	file->Eof = 1;
}



//given a full filepath, open the file and display various bits of information
void FsFat12_GetEntryInfo(const char* entry)
{	
	FILE resultingFile = FsFat12_Open(entry);
	
	if(resultingFile.Flags == FS_INVALID)
	{
		ConsoleWriteString("File does not exist\n");
		return;
	}	
	
	ConsoleWriteString("FileName: ");
	ConsoleWriteString(resultingFile.Name);
	ConsoleWriteCharacter('\n');
	
	
	ConsoleWriteString("File Type: ");
	if(resultingFile.Flags == FS_DIRECTORY) ConsoleWriteString("Directory");
	else ConsoleWriteString("File");
	ConsoleWriteCharacter('\n');
	
	ConsoleWriteString("File Size: ");
	ConsoleWriteInt(resultingFile.FileLength, 10);	
	ConsoleWriteCharacter('\n');
	
	ConsoleWriteString("First Cluster: ");
	ConsoleWriteInt(resultingFile.CurrentCluster, 10);	
	ConsoleWriteCharacter('\n');
}

//return the current working directory string
char* FsFat12_GetCurrentDirectoryName()
{
	return currentDirectoryName;
}

//loop through all directory entries in the current directory displaying their filenames and extensions
void FsFat12_DisplayAllCurrentDirectoryEntries()
{		
	//TODO: MODIFY TO SHOW HIDDEN FILES ONLY WHEN REQUESTED WITH AN ARGUMENT
	int clusterOffset = 0;
	DirectoryEntry temp;
	
	do
	{
		
		//get the next directory entry and print its name if it is valid
		temp = FsFat12_GetDirectoryEntryByIndex(clusterOffset++, currentDirectory.FirstCluster);		
		if(!INVALID_FILENAME(temp.Filename[0]))
		{
			for(int i = 0; i < 8; ++i)
			{
				if(temp.Filename[i] != ' ') ConsoleWriteCharacter(temp.Filename[i]);
				else i = 8;
			}
			
			if((temp.Attrib & DE_SUBDIR) != DE_SUBDIR)
			{				
				ConsoleWriteCharacter('.');
				ConsoleWriteCharacter(temp.Ext[0]);
				ConsoleWriteCharacter(temp.Ext[1]);
				ConsoleWriteCharacter(temp.Ext[2]);
			}
			
			ConsoleWriteCharacter('\n');			
		}	
		
	}while(!INVALID_FILENAME(temp.Filename[0]));
}

//change the current directory to the directory specified in the filepath
void FsFat12_ChangeDirectory(const char* newDirectory)
{	
	DirectoryEntry resultingEntry = FsFat12_GetNestedDirectoryEntry(newDirectory);
	
	if(INVALID_FILENAME(resultingEntry.Filename[0]))
	{		
			ConsoleWriteString("ERROR: Directory does not exist.\n");
			return;
	}	
	if((resultingEntry.Attrib & DE_SUBDIR) != DE_SUBDIR)
	{		
			ConsoleWriteString("ERROR: File specified is not a directory.\n");
			return;
	}
	
	currentDirectory = resultingEntry;
	SetCurrentDirPath(newDirectory);	
}




//Display all clusters linked to the provided cluster
void TESTGetFatEntry(int initialCluster)
{
	int tempCluster = initialCluster;
	ConsoleWriteInt(tempCluster, 10);
	ConsoleWriteCharacter(',');
	
	do
	{
		tempCluster = FsFat12_GetFATEntry(tempCluster);
		ConsoleWriteInt(tempCluster, 10);
		ConsoleWriteCharacter(',');
	}while(!SPECIAL_CLUSTER(tempCluster));
	
	ConsoleWriteString("Exited with: ");
	ConsoleWriteInt(tempCluster, 10);
	ConsoleWriteCharacter('\n');
}

//Display the contents of the chosen cluster to the screen
void TESTReadCluster(int cluster)
{
	uint8_t* readData = FsFat12_ReadCluster(cluster);
	if(readData == NULL) return;
	for(int i = 0; i < 512; ++i)
	{
		ConsoleWriteCharacter(readData[i]);
	}
}

//Display the second and third sectors of the specified file
void TESTGetNextClusterOfCurrentFile(int initialCluster, int destClustOffset)
{	
	//display the second cluster (cluster index 1)
	uint8_t* readData = FsFat12_GetNextClusterOfCurrentFile(initialCluster, destClustOffset);
	if(readData == NULL) return;
	for(int i = 0; i < 512; ++i)
	{
		ConsoleWriteCharacter(readData[i]);
	}
}

//Given a filepath display the extracted names and extensions
void TESTExtractFileNameAndExtension(char* filepath)
{	
	char tempName[20];
	char* entryName;
	char* extension;
	
	int pathCharIndex = 0;
	//loop through all files in the filepath
	while(filepath[pathCharIndex] != '\0')
	{
		//extract the next directory entry name from the full path
		int index = 0;
		while(	filepath[pathCharIndex] != '/' &&
				filepath[pathCharIndex] != 0 &&	
				index < 19)
		{
			tempName[index++] = filepath[pathCharIndex++];
		}		
		tempName[index] = 0;
		
		
		ConsoleWriteString("FilePath: ");
		ConsoleWriteString(tempName);
		ConsoleWriteCharacter('\n');
		
		
		//split it into name and extension
		entryName = ExtractFileName(tempName);
		extension = ExtractFileExtension(tempName);
		
		//display name and extension
		ConsoleWriteString("Name: ");
		if(entryName != NULL)
		{
			for(int i = 0; i < 8; ++i)
			{
				ConsoleWriteCharacter(entryName[i]);
			}
		}
		else
		{
			ConsoleWriteString("ERROR   ");
		}
		ConsoleWriteCharacter(';');
		
		ConsoleWriteString(" Extensiom: ");
		if(extension != NULL)
		{			
			for(int i = 0; i < 3; ++i)
			{
				ConsoleWriteCharacter(extension[i]);
			}
		}
		else
		{
			ConsoleWriteString("ERROR   ");
		}
		ConsoleWriteCharacter('\n');	
		
		//move the index past the filename separator
		if(filepath[pathCharIndex] != 0) ++pathCharIndex;
	}
}

//Given a directory and a directory entry index, retreive and display the directory entry at the provided index of the provided directory
void TESTGetDirectoryEntryByIndex(int dirIndex, int sourceDirCluster)
{
	DirectoryEntry result = FsFat12_GetDirectoryEntryByIndex(dirIndex, sourceDirCluster);
	
	//return early for invalid directory entries
	if(result.Filename[0] == 0)
	{
		ConsoleWriteString("Invalid directory entry.\n");
		return;
	}
	
	ConsoleWriteString(result.Filename);
	ConsoleWriteCharacter('\n');
}

//given a directory entry name.extension, split it into separate name and extension strings and find the directory entry with matching name and extension within the provided source directory
void TESTGetDirectoryEntryWithName(const char* directoryEntry, int sourceDirCluster)
{			
	//retreive requested directory entry
	DirectoryEntry result = FsFat12_GetDirectoryEntryWithName(directoryEntry, sourceDirCluster);
	
	//return early for invalid directory entries
	if(result.Filename[0] == 0)
	{
		ConsoleWriteString("Invalid directory entry.\n");
		return;
	}
	
	ConsoleWriteString(result.Filename);
	ConsoleWriteCharacter('\n');
}


void TESTGetNestedDirectoryEntry(const char* nestedDirPath)
{
	//retreive requested directory entry
	DirectoryEntry result = FsFat12_GetNestedDirectoryEntry(nestedDirPath);
	
	//return early for invalid directory entries
	if(result.Filename[0] == 0)
	{
		ConsoleWriteString("Invalid directory entry.\n");
		return;
	}
	
	ConsoleWriteString(result.Filename);
	ConsoleWriteCharacter('\n');
}