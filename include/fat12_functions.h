#ifndef _F12Funcs_H
#define _F12Funcs_H

#include <filesystem.h>

void FsFat12_Initialise();

//Functions related to reading data from disk
uint16_t FsFat12_GetFATEntry(int sectorNum);
uint8_t* FsFat12_ReadCluster(int cluster);
uint8_t* FsFat12_GetNextClusterOfCurrentFile(int firstSector, int sectorOffset);

//functions used when interpreting filepaths
char* ExtractFileName(const char* nameAndExt);
char* ExtractFileExtension(const char* nameAndExt);

//Functions related to extracting specific directory entries
DirectoryEntry FsFat12_GetDirectoryEntryByIndex(int dirIndex, int sourceDirCluster);
DirectoryEntry FsFat12_GetDirectoryEntryWithName(const char* directoryEntry, int sourceDirCluster);
DirectoryEntry FsFat12_GetNestedDirectoryEntry(const char* nestedDirPath);

//Functions to update the current directory path displayed when PWD is entered
void UpdateCurrentDirName(const char* newFilepath);
void SetCurrentDirPath(const char* newPath);


//Functions for opening, reading, and closing files.
FILE FsFat12_Open(const char* filename);
unsigned int FsFat12_Read(PFILE file, unsigned char* buffer, unsigned int length);
void FsFat12_Close(PFILE file);


//Functions that match various input commands (FILE, PWD, DIR/LS, CD)
void FsFat12_GetEntryInfo(const char* entry);
char* FsFat12_GetCurrentDirectoryName();
void FsFat12_DisplayAllCurrentDirectoryEntries();
void FsFat12_ChangeDirectory(const char* newDirectory);



//Test functions
void TESTGetFatEntry(int initialCluster);
void TESTReadCluster(int cluster);
void TESTGetNextClusterOfCurrentFile(int initialCluster, int destClustOffset);
void TESTExtractFileNameAndExtension(char* filepath);
void TESTGetDirectoryEntryByIndex(int dirIndex, int sourceDirCluster);
void TESTGetDirectoryEntryWithName(const char* directoryEntry, int sourceDirCluster);
void TESTGetNestedDirectoryEntry(const char* nestedDirPath);

#endif