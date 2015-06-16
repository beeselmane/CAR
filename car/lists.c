#include "lists.h"

#pragma mark - Linked FileList

FileListLinked *FileListLinkedCreate(void)
{
    FileListLinked *list = malloc(sizeof(FileListLinked));
    memset(&list->data, 0, sizeof(FileListData));
    list->head = list->tail = NULL;
    return list;
}

bool FileListLinkedAddDirectory(FileListLinked *list, Path directory)
{
    printf("L %s\n", directory);

    if (!OSXHaveSearchAccess(directory))
    {
        fprintf(stderr, "Error: Permission denied to search directory\n");
        return false;
    }

    return OSXRunBlockOnDirectoryContents(directory, (bool (^)(Path, DirectoryEntry, MemoryAddress))^(Path directory, DirectoryEntry entry, FileListLinked *list) {
        if (entry->d_namlen == 2 && *((UInt16 *)entry->d_name) == 0x2E2E) return true; // '..'

        if (entry->d_namlen == 1 && entry->d_name[0] == '.') // '.'
        {
            FileListLinkedAddFile(list, directory, kEntryTypeDirectory, 0);
            return true;
        }

        Path realpath = NULL; asprintf(&realpath, "%s/%s", directory, entry->d_name);
        FileStats *stats = OSXReadFileStats(realpath, false);

        if (!OSXIsDirectory(stats)) {
            printf("L %s\n", realpath);

            if (OSXIsRegular(stats)) {
                FileListLinkedAddFile(list, realpath, kEntryTypeRegular, stats->st_size);
            } else if (OSXIsLink(stats)) {
                Size linksize = -1;
                String link = OSXReadLink(realpath, &linksize);

                if (!link)
                {
                    free(stats);
                    return false;
                }

                free(link);
                FileListLinkedAddFile(list, realpath, kEntryTypeSymlink, linksize + 1);
            } else {
                fprintf(stderr, "Warning: Item at '%s' is of unknown type\n", realpath);
                free(realpath);
            }
        } else {
            bool success = FileListLinkedAddDirectory(list, realpath);

            if (!success)
            {
                free(stats);
                return false;
            }
        }

        free(stats);
        return true;
    }, list);
}

void FileListLinkedAddFile(FileListLinked *list, Path path, UInt8 type, Size size)
{
    FileListEntry *entry = malloc(sizeof(FileListEntry));
    memset(entry, 0, sizeof(FileListEntry));

    entry->path = path;
    entry->type = type;
    entry->size = size;

    if (list->head) {
        entry->prev = list->tail;
        list->tail->next = entry;
        entry->next = NULL;
        list->tail = entry;
    } else {
        entry->next = entry->prev = NULL;
        list->head = list->tail = entry;
    }

    list->data.namesize += strlen(path) + 1;
    list->data.datasize += size;
    list->data.listsize++;
}

extern void FileListLinkedDestory(FileListLinked *list)
{
    FileListEntry *entry = list->head->next;
    free(entry->prev);

    while (entry)
    {
        free(entry->path);
        free(entry);

        entry = entry->next;
    }

    free(list);
}

#pragma mark - Array FileList

FileListArray *FileListArrayCreate(void)
{
    FileListArray *list = malloc(sizeof(FileListArray));
    memset(&list->data, 0, sizeof(FileListData));
    list->entries = NULL;
    return list;
}

extern void FileListArraySort(FileListArray *list);

void FileListArrayDestroy(FileListArray *list)
{
    for (UInt64 i = 0; i < list->data.listsize; i++)
    {
        if (i != 0) free(list->entries[i]->path);
        free(list->entries[i]);
    }

    free(list);
}

#pragma mark - Conversion

FileListLinked *FileListToLinked(FileListArray *arrayList)
{
    FileListLinked *linkedList = FileListLinkedCreate();
    memcpy(&linkedList->data, &arrayList->data, sizeof(FileListData));
    UInt64 finalIndex = arrayList->data.listsize - 1;

    for (UInt64 i = 1; i < finalIndex; i++)
    {
        arrayList->entries[i]->next = arrayList->entries[i + 1];
        arrayList->entries[i]->prev = arrayList->entries[i - 1];
    }

    arrayList->entries[finalIndex]->prev = arrayList->entries[finalIndex - 1];
    arrayList->entries[finalIndex]->next = NULL;

    arrayList->entries[0]->next = arrayList->entries[1];
    arrayList->entries[0]->prev = NULL;

    linkedList->tail = arrayList->entries[finalIndex];
    linkedList->head = arrayList->entries[0];

    return linkedList;
}

FileListArray *FileListToArray(FileListLinked *linkedList)
{
    FileListArray *arrayList = FileListArrayCreate();
    memcpy(&arrayList->data, &linkedList->data, sizeof(FileListData));
    arrayList->entries = calloc(linkedList->data.listsize, sizeof(FileListEntry));

    FileListEntry *entry = linkedList->head;
    UInt64 i = 0;

    while (entry)
    {
        arrayList->entries[i++] = entry;
        entry = entry->next;
    }

    return arrayList;
}
