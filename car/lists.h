#ifndef __CAR_LISTS__
#define __CAR_LISTS__ 1

#include "syscalls.h"

#pragma mark - Generic

#define kEntryTypeRegular   0
#define kEntryTypeDirectory 1
#define kEntryTypeSymlink   2

typedef struct FileListEntry {
    struct FileListEntry *next, *prev;

    Path path;
    UInt8 type;
    Size size;
} FileListEntry;

typedef struct {
    Size namesize;
    Size datasize;
    Size listsize;
} FileListData;

#pragma mark - Linked FileList

typedef struct {
    FileListEntry *head, *tail;
    FileListData data;
} FileListLinked;

extern FileListLinked *FileListLinkedCreate(void);
extern bool FileListLinkedAddDirectory(FileListLinked *list, Path directory);
extern void FileListLinkedAddFile(FileListLinked *list, Path path, UInt8 type, Size size);
extern void FileListLinkedDestory(FileListLinked *list);

#pragma mark - Array FileList

typedef struct {
    FileListEntry **entries;
    FileListData data;
} FileListArray;

extern FileListArray *FileListArrayCreate(void);
extern void FileListArraySort(FileListArray *list);
extern void FileListArrayDestroy(FileListArray *list);

#pragma mark - Conversion

extern FileListLinked *FileListToLinked(FileListArray *arrayList);
extern FileListArray  *FileListToArray(FileListLinked *linkedList);

#endif /* !defined(__CAR_LISTS__) */
