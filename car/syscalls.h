#ifndef __CAR_SYSCALLS__
#define __CAR_SYSCALLS__ 1

#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define OSXIsDirectory(fs) S_ISDIR(fs->st_mode)
#define OSXIsRegular(fs)   S_ISREG(fs->st_mode)
#define OSXIsLink(fs)      S_ISLNK(fs->st_mode)

typedef uint64_t UInt64;
typedef uint32_t UInt32;
typedef uint16_t UInt16;
typedef uint8_t  UInt8;

typedef int64_t SInt64;
typedef int32_t SInt32;
typedef int16_t SInt16;
typedef int8_t  SInt8;

typedef struct dirent *DirectoryEntry;
typedef struct stat FileStats;
typedef void *MemoryAddress;
typedef mode_t FileMode;
typedef DIR *Directory;
typedef ssize_t SSize;
typedef off_t Offset;
typedef char *String;
typedef String Path;

#ifndef __MACTYPES__
    typedef size_t Size;
#endif

extern bool OSXRunBlockOnDirectoryContents(Path directory, bool (^block)(Path, DirectoryEntry, MemoryAddress), MemoryAddress userinfo);
extern MemoryAddress OSXMapFile(Path path, Size size, Offset startOffset, bool write);
extern bool OSXWriteDataToFile(MemoryAddress data, Size size, Path path);
extern MemoryAddress OSXMapFileFully(Path path, Size *size, bool write);
extern bool OSXWriteFileTo(Path file, MemoryAddress destination);
extern FileStats *OSXReadFileStats(Path path, bool followLinks);
extern bool OSXUnmapFile(MemoryAddress mapaddr, Size size);
extern UInt32 OSXCalculateChecksum(UInt8 *data, Size size);
extern bool OSXZeroFileToSize(Path path, Size size);
extern bool OSXCreateSymlink(Path from, Path to);
extern String OSXReadLink(Path path, Size *size);
extern bool OSXHaveSearchAccess(Path directory);
extern bool OSXCreateDirectoryAt(Path path);
extern bool OSXUnlinkItemAt(Path path);
extern bool OSXCanReadFile(Path file);
extern bool OSXCreateFile(Path path);
extern bool OSXFileExists(Path path);

// OSXMapFile          --> Call OSXUnmapFile
// OSXMapFileFully     --> Call OSXUnmapFile
// OSXReadFileStats    --> Call free
// OSXUnmapFile        --> N/A
// OSXHaveSearchAccess --> N/A
// OSXCanReadFile      --> N/A
// OSXCreateFile       --> N/A
// OSXReadLink         --> Call free
// OSXFileExists       --> N/A
// OSXZeroFileToSize   --> N/A

#endif /* !defined(__car__syscalls__) */
