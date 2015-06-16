#include "syscalls.h"

bool OSXRunBlockOnDirectoryContents(Path path, bool (^block)(Path, DirectoryEntry, MemoryAddress), MemoryAddress userinfo)
{
    Directory directory = opendir(path);
    DirectoryEntry entry = NULL;
    bool result = true;

    if (!directory)
    {
        fprintf(stderr, "Error: Could not open directory at '%s'\n", path);
        perror("opendir");
        return false;
    }

    while ((entry = readdir(directory)))
    {
        bool cont = block(path, entry, userinfo);

        if (!cont)
        {
            result = false;
            break;
        }
    }

    if (closedir(directory))
    {
        fprintf(stderr, "Error: Could not close directory at '%s'\n", path);
        perror("closedir");
        return false;
    }

    return result;
}

MemoryAddress OSXMapFile(Path path, Size size, Offset startOffset, bool write)
{
    int fd = open(path, (write ? O_RDWR : O_RDONLY));

    if (fd < 0)
    {
        fprintf(stderr, "Error: Could not open file at '%s'\n", path);
        perror("open");
        return NULL;
    }

    int protection = write ? (PROT_READ | PROT_WRITE) : (PROT_READ);
    intptr_t result = (intptr_t)mmap(NULL, size, protection, MAP_FILE | MAP_SHARED, fd, startOffset);

    if (result < 0)
    {
        fprintf(stderr, "Error: Could not map file at '%s' into memory\n", path);
        perror("mmap");

        if (close(fd) < 0)
        {
            fprintf(stderr, "Error: Could not close file at '%s'\n", path);
            perror("close");
        }

        return NULL;
    }

    if (close(fd) < 0)
    {
        fprintf(stderr, "Error: Could not close file at '%s'\n", path);
        perror("close");
        return NULL;
    }

    return (MemoryAddress)result;
}

MemoryAddress OSXMapFileFully(Path path, Size *size, bool write)
{
    FileStats *stats = OSXReadFileStats(path, true);
    if (!stats) return NULL;

    if (size) *size = stats->st_size;
    return OSXMapFile(path, stats->st_size, 0, write);
}

#include "crc32_table.h"

UInt32 OSXCalculateChecksum(UInt8 *data, Size size)
{
    UInt32 checksum = 0xFFFFFFFF;

    for (Size i = 0; i < size; i++)
        checksum = (checksum >> 8) ^ crc32_table[(checksum & 0xFF) ^ *data++];

    return (checksum ^ 0xFFFFFFFF);
}

bool OSXWriteFileTo(Path file, MemoryAddress destination)
{
    FileStats *stats = OSXReadFileStats(file, true);
    if (!stats) return false;

    if (stats->st_size == 0)
    {
        free(stats);
        return true;
    }

    FILE *fp = fopen(file, "rb");

    if (!fp)
    {
        fprintf(stderr, "Error: Could not open file at '%s'\n", file);
        perror("open");
        return false;
    }

    Size written = fread(destination, stats->st_size, 1, fp);

    if (written != 1)
    {
        fprintf(stderr, "Error: Could not write file at '%s' to memory at 0x%016lX\n", file, (uintptr_t)destination);
        perror("write");

        if (fclose(fp))
        {
            fprintf(stderr, "Error: Could not close file at '%s'\n", file);
            perror("close");
        }

        return false;
    }

    if (fclose(fp))
    {
        fprintf(stderr, "Error: Could not close file at '%s'\n", file);
        perror("close");
        return false;
    }

    return true;
}

bool OSXUnmapFile(MemoryAddress mapaddr, Size size)
{
    if (munmap(mapaddr, size))
    {
        fprintf(stderr, "Error: Could not unmap %zu bytes of memory from 0x%016lX\n", size, (uintptr_t)mapaddr);
        perror("munmap");
        return false;
    }

    return true;
}

FileStats *OSXReadFileStats(Path path, bool followLinks)
{
    FileStats *stats = malloc(sizeof(FileStats));
    int (*statfunc)(const char *, FileStats *) = followLinks ? stat : lstat;

    if (statfunc(path, stats))
    {
        fprintf(stderr, "Error: Could not get file stats for file at '%s'", path);
        perror(followLinks ? "stat" : "lstat");
        free(stats);

        return NULL;
    }

    return stats;
}

bool OSXCreateFile(Path path)
{
    if (OSXFileExists(path)) return true;

    int fd = open(path, O_RDWR | O_CREAT);

    if (fd < 0)
    {
        fprintf(stderr, "Error: Could not create file at '%s'\n", path);
        perror("open");
        return false;
    }

    if (close(fd))
    {
        fprintf(stderr, "Error: Could not close new file at '%s'\n", path);
        perror("close");
        return false;
    }

    if (chmod(path, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))
    {
        fprintf(stderr, "Error: Could not set file permissions on new file at '%s'\n", path);
        perror("chmod");
        return false;
    }

    return true;
}

bool OSXHaveSearchAccess(Path directory)
{
    return (access(directory, R_OK | X_OK) ? false : true);
}

bool OSXCanReadFile(Path file)
{
    return (access(file, R_OK) ? false : true);
}

String OSXReadLink(Path path, Size *size)
{
    char buffer[2056]; memset(buffer, 0, sizeof(buffer));
    SSize linksize = readlink(path, buffer, sizeof(buffer));

    if (linksize < 0)
    {
        fprintf(stderr, "Error: Could not read link at '%s'\n", path);
        perror("readlink");
        return NULL;
    }

    String result = malloc(linksize + 1);
    SSize lr2 = readlink(path, result, linksize);
    result[linksize] = 0;

    if (lr2 != linksize)
    {
        fprintf(stderr, "Error: Link inconsistency at '%s'\n", path);
        perror("readlink");
        free(result);

        return NULL;
    }

    if (size) *size = linksize;
    return result;
}

bool OSXCreateDirectoryAt(Path path)
{
    if (OSXFileExists(path)) return true;

    if (mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH))
    {
        fprintf(stderr, "Error: Could not create directory at '%s'\n", path);
        perror("mkdir");
        return false;
    }

    return true;
}

bool OSXUnlinkItemAt(Path path)
{
    if (!OSXFileExists(path)) return true;

    if (unlink(path))
    {
        fprintf(stderr, "Error: Could not remove file at '%s'\n", path);
        perror("unlink");
        return false;
    }

    return true;
}

bool OSXZeroFileToSize(Path path, Size size)
{
    if (!OSXCreateFile(path)) return false;

    if (truncate(path, size))
    {
        fprintf(stderr, "Error: Could not extend file at '%s' to be '%lu' bytes\n", path, size);
        perror("truncate");
        return false;
    }

    MemoryAddress mapaddr = OSXMapFileFully(path, NULL, true);
    if (!mapaddr) return false;

    memset(mapaddr, 0, size);
    OSXUnmapFile(mapaddr, size);
    return true;
}

bool OSXWriteDataToFile(MemoryAddress data, Size size, Path path)
{
    FILE *fp = fopen(path, "wb");

    if (!fp)
    {
        fprintf(stderr, "Error: Could not open file at '%s'\n", path);
        perror("open");
        return false;
    }

    SSize written = fwrite(data, size, 1, fp);

    if (written != 1)
    {
        fprintf(stderr, "Error: Could not write %lu bytes of data at 0x%016lX to file at '%s'", size, (uintptr_t)data, path);
        perror("write");

        if (fclose(fp))
        {
            fprintf(stderr, "Error: Could not close file at '%s'\n", path);
            perror("close");
        }

        return false;
    }

    if (fclose(fp))
    {
        fprintf(stderr, "Error: Could not close file at '%s'\n", path);
        perror("close");
        return false;
    }

    return true;
}

bool OSXCreateSymlink(Path from, Path to)
{
    if (OSXFileExists(to)) return false;

    if (symlink(from, to))
    {
        fprintf(stderr, "Error: Could not create a symlink from '%s' to '%s'\n", from, to);
        perror("symlink");
        return false;
    }

    return true;
}

bool OSXFileExists(Path path)
{
    return (access(path, F_OK) ? false : true);
}
