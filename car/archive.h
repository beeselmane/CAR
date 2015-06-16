#ifndef __CAR_ARCHIVE__
#define __CAR_ARCHIVE__ 1

#include "syscalls.h"
#include "lists.h"

#define kCAMagic      {'C', 'A', 'R', 0x0}
#define kCAVersion4   {'4', '.', '0'}
#define kCAHeaderSize 32
#define kCAEntrySize  sizeof(CAArchiveEntry)

typedef struct {
    char magic[4];
    char version[3];
    UInt16 flags;
    UInt32 stringOffset;
    UInt64 dataOffset;
    UInt32 checksum;
    UInt32 headerChecksum;
} CAArchiveHeader;

typedef struct {
    UInt32 nameOffset;
    UInt8 type;
    UInt64 dataOffset;
    UInt64 size;
} CAArchiveEntry;

extern bool CAArchiveCreate(Path archive, Path rootdir);
extern bool CAArchiveExtractItem(Path archive, String item, Path output);
extern bool CAArchiveExtractAll(Path archive, Path outdir);
extern String *CAArchiveListContents(Path archive, Size *count);
extern bool CAArchiveCheckValidity(Path archive);
extern void CAArchivePrintInfo(Path archive, bool entries);

#endif /* !defined(__CAR_ARCHIVE__) */
