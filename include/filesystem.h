// Filesystem Definitions

#ifndef _FSYS_H
#define _FSYS_H

#include <stdint.h>

//	File flags

#define FS_FILE       0
#define FS_DIRECTORY  1
#define FS_INVALID    2

// File

typedef struct _File 
{
	char 			Name[256];
	uint32_t 	Flags;
	uint32_t 	FileLength;
	uint32_t 	Id;
	uint32_t 	Eof;
	uint32_t 	Position;
	uint32_t 	CurrentCluster;
} FILE;

typedef FILE * PFILE;

//	Directory Entry Attributes

#define DE_READONLY 0x01
#define DE_HIDDEN	0x02
#define DE_SYSTEM	0x04
#define DE_VOL_LAB	0x08
#define DE_SUBDIR	0x10
#define DE_ARCHIVE	0x20

//	Directory Entry

typedef struct _DirectoryEntry 
{
	uint8_t   Filename[8];
	uint8_t   Ext[3];
	uint8_t   Attrib;
	uint8_t   Reserved;
	uint8_t   TimeCreatedMs;
	uint16_t  TimeCreated;
	uint16_t  DateCreated;
	uint16_t  DateLastAccessed;
	uint16_t  FirstClusterHiBytes;
	uint16_t  LastModTime;
	uint16_t  LastModDate;
	uint16_t  FirstCluster;
	uint32_t  FileSize;

} __attribute__((packed)) DirectoryEntry;

typedef DirectoryEntry * pDirectoryEntry;



#endif
