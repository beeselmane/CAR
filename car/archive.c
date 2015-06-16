#include "archive.h"

bool CAArchiveCreate(Path archive, Path rootdir)
{
    FileListLinked *list = FileListLinkedCreate();
    bool added = FileListLinkedAddDirectory(list, rootdir);
    Size nameshift = strlen(rootdir);

    if (!added)
    {
        FileListLinkedDestory(list);
        return false;
    }

    list->data.namesize -= (nameshift * list->data.listsize);
    list->data.namesize++;

    UInt16 flags = 0;
    // There aren't any flags...

    UInt32 strOff = kCAHeaderSize + (kCAEntrySize * (UInt32)list->data.listsize);
    UInt64 datOff = (UInt64)strOff + list->data.namesize;
    Size finalsize = datOff + list->data.datasize;

    CAArchiveHeader header = {
        .magic = kCAMagic,
        .version = kCAVersion4,
        .flags = flags,
        .stringOffset = strOff,
        .dataOffset = datOff,
        .checksum = 0,
        .headerChecksum = 0
    };

    if (!OSXCreateFile(archive) || !OSXZeroFileToSize(archive, finalsize))
    {
        FileListLinkedDestory(list);
        OSXUnlinkItemAt(archive);
        return false;
    }

    MemoryAddress mapaddr = OSXMapFileFully(archive, NULL, true);

    if (!mapaddr)
    {
        FileListLinkedDestory(list);
        OSXUnlinkItemAt(archive);
        return false;
    }

    memcpy(mapaddr, &header, sizeof(CAArchiveHeader));
    Offset stringOffset = 0, dataOffset = 0, tocOffset = kCAHeaderSize;
    FileListEntry *entry = list->head;

    // Fix to make root directory be entered as '/' in the archive
    bool freepath = false;

    if ((entry->path + nameshift)[0] != '/')
    {
        asprintf(&entry->path, "%s/", rootdir);
        freepath = true;
    }

    while (entry)
    {
        String entryName = entry->path + nameshift;
        Size entryNameSize = strlen(entryName) + 1;
        printf("A %s\n", entryName);

        CAArchiveEntry fileEntry = {
            .nameOffset = (UInt32)stringOffset,
            .type = entry->type,
            .dataOffset = dataOffset,
            .size = entry->size
        };

        memcpy(mapaddr + tocOffset, &fileEntry, sizeof(CAArchiveEntry));
        memcpy(mapaddr + (header.stringOffset + stringOffset), entryName, entryNameSize);

        #define CACleanupAndReturnFalse()                \
            do {                                    \
                FileListLinkedDestory(list);        \
                OSXUnmapFile(mapaddr, finalsize);   \
                OSXUnlinkItemAt(archive);           \
                return false;                       \
            } while (0)

        switch (entry->type)
        {
            case kEntryTypeRegular: {
                if (!OSXWriteFileTo(entry->path, mapaddr + (header.dataOffset + dataOffset)))
                    CACleanupAndReturnFalse();
            } break;
            case kEntryTypeDirectory: {
                // No data to store...
            } break;
            case kEntryTypeSymlink: {
                String link = OSXReadLink(entry->path, NULL);
                if (!link) CACleanupAndReturnFalse();

                memcpy(mapaddr + (header.dataOffset + dataOffset), link, entry->size);
                free(link);
            } break;
            default:
                fprintf(stderr, "Error: Invalid entry type\n");
                FileListLinkedDestory(list);
                OSXUnmapFile(mapaddr, finalsize);
                OSXUnlinkItemAt(archive);
                return false;
        }

        #undef CACleanupAndReturnFalse
        stringOffset += entryNameSize;
        dataOffset += entry->size;
        tocOffset += kCAEntrySize;

        entry = entry->next;
    }

    if (freepath) free(list->head->path);
    FileListLinkedDestory(list);

    header.checksum = OSXCalculateChecksum(mapaddr + kCAHeaderSize, finalsize - kCAHeaderSize);
    header.headerChecksum = OSXCalculateChecksum(mapaddr, sizeof(CAArchiveHeader) - (2 * sizeof(UInt32)));
    memcpy(mapaddr, &header, sizeof(CAArchiveHeader));

    OSXUnmapFile(mapaddr, finalsize);
    return true;
}

bool CAArchiveExtractItem(Path archive, String item, Path output)
{
    Size mapsize = -1;
    MemoryAddress mapaddr = OSXMapFileFully(archive, &mapsize, false);
    CAArchiveHeader *header = (CAArchiveHeader *)mapaddr;
    Offset currentOffset = kCAHeaderSize;
    if (!mapaddr) return NULL;
    
    while (currentOffset < header->stringOffset)
    {
        CAArchiveEntry *entry = (CAArchiveEntry *)(mapaddr + currentOffset);
        String name = mapaddr + (header->stringOffset + entry->nameOffset);
        currentOffset += sizeof(CAArchiveEntry);
        if (strcmp(name, item)) continue;
        printf("X %s\n", name);

        MemoryAddress data = mapaddr + (header->dataOffset + entry->dataOffset);

        #define CACleanupAndReturnFalse()       \
            do {                                \
                OSXUnmapFile(mapaddr, mapsize); \
                return false;                   \
            } while (0)

        switch (entry->type)
        {
            case kEntryTypeRegular: {
                if (!OSXWriteDataToFile(data, entry->size, output))
                    CACleanupAndReturnFalse();
            } break;
            case kEntryTypeDirectory: {
                if (!OSXCreateDirectoryAt(output))
                    CACleanupAndReturnFalse();
            } break;
            case kEntryTypeSymlink: {
                if (OSXFileExists(output))
                {
                    fprintf(stderr, "Warning: file '%s' already exists. Will ignore\n", output);
                    break;
                }

                if (!OSXCreateSymlink(data, output)) CACleanupAndReturnFalse();
            } break;
            default:
                fprintf(stderr, "Error: Invalid Entry type 0x%02X", entry->type);
                CACleanupAndReturnFalse();
        }
        
        #undef CACleanupAndReturnFalse
    }
    
    OSXUnmapFile(mapaddr, mapsize);
    return true;
}

bool CAArchiveExtractAll(Path archive, Path outdir)
{
    Size mapsize = -1;
    MemoryAddress mapaddr = OSXMapFileFully(archive, &mapsize, false);
    CAArchiveHeader *header = (CAArchiveHeader *)mapaddr;
    Offset currentOffset = kCAHeaderSize;
    if (!mapaddr) return NULL;
    
    while (currentOffset < header->stringOffset)
    {
        CAArchiveEntry *entry = (CAArchiveEntry *)(mapaddr + currentOffset);
        String name = mapaddr + (header->stringOffset + entry->nameOffset);
        printf("X %s\n", name);

        MemoryAddress data = mapaddr + (header->dataOffset + entry->dataOffset);
        String outfile; asprintf(&outfile, "%s%s", outdir, name);

        #define CACleanupAndReturnFalse()       \
            do {                                \
                OSXUnmapFile(mapaddr, mapsize); \
                return false;                   \
            } while (0)

        switch (entry->type)
        {
            case kEntryTypeRegular: {
                if (!OSXWriteDataToFile(data, entry->size, outfile))
                    CACleanupAndReturnFalse();
            } break;
            case kEntryTypeDirectory: {
                if (!OSXCreateDirectoryAt(outfile))
                    CACleanupAndReturnFalse();
            } break;
            case kEntryTypeSymlink: {
                if (OSXFileExists(outfile))
                {
                    fprintf(stderr, "Warning: file '%s' already exists. Will ignore\n", outfile);
                    break;
                }

                if (!OSXCreateSymlink(data, outfile)) CACleanupAndReturnFalse();
            } break;
            default:
                fprintf(stderr, "Error: Invalid Entry type 0x%02X", entry->type);
                CACleanupAndReturnFalse();
        }

        #undef CACleanupAndReturnFalse
        currentOffset += sizeof(CAArchiveEntry);
    }

    OSXUnmapFile(mapaddr, mapsize);
    return true;
}

String *CAArchiveListContents(Path archive, Size *count)
{
    CAArchiveHeader *header = (CAArchiveHeader *)OSXMapFile(archive, kCAHeaderSize, 0, false);
    if (!header) return NULL;

    UInt64 dataOffset = header->dataOffset;
    OSXUnmapFile(header, kCAHeaderSize);

    MemoryAddress mapaddr = OSXMapFile(archive, dataOffset, 0, false);
    header = (CAArchiveHeader *)mapaddr;
    if (!mapaddr) return NULL;

    UInt64 filecount = (header->stringOffset - kCAHeaderSize) / kCAEntrySize;
    String *entries = calloc(filecount, sizeof(String));
    Offset currentOffset = kCAHeaderSize;
    UInt64 i = 0;

    while (currentOffset < header->stringOffset)
    {
        CAArchiveEntry *entry = (CAArchiveEntry *)(mapaddr + currentOffset);
        String name = mapaddr + (header->stringOffset + entry->nameOffset);
        Size nameSize = strlen(name) + 1;

        entries[i] = malloc(nameSize);
        memcpy(entries[i], name, nameSize);

        printf("L %s\n", name);
        currentOffset += sizeof(CAArchiveEntry);
    }

    OSXUnmapFile(mapaddr, dataOffset);
    if (count) *count = filecount;
    return entries;
}

bool CAArchiveCheckValidity(Path archive)
{
    CAArchiveHeader *header = (CAArchiveHeader *)OSXMapFile(archive, kCAHeaderSize, 0, false);
    if (!header) return false;

    #define CACleanupAndReturnFalse()               \
        do {                                        \
            OSXUnmapFile(header, kCAHeaderSize);    \
            return false;                           \
        } while (0)

    char magic[4] = kCAMagic;
    if (memcmp(header->magic, magic, 4)) CACleanupAndReturnFalse();

    char version[3] = kCAVersion4;
    if (memcmp(header->version, version, 3)) CACleanupAndReturnFalse();

    UInt32 hcheck = OSXCalculateChecksum((UInt8 *)header, kCAHeaderSize - (2 * sizeof(UInt32)));
    if (hcheck != header->headerChecksum) CACleanupAndReturnFalse();

    OSXUnmapFile(header, kCAHeaderSize); Size mapsize = -1;
    MemoryAddress data = OSXMapFileFully(archive, &mapsize, false);
    header = (CAArchiveHeader *)data;
    if (!data) return false;

    UInt32 checksum = OSXCalculateChecksum(data + kCAHeaderSize, mapsize - kCAHeaderSize);
    bool valid = checksum == header->checksum;
    OSXUnmapFile(data, mapsize);

    #undef CACleanupAndReturnFalse
    return valid;
}

// Note yet...
void CAArchivePrintInfo(Path archive, bool entries);
