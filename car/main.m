#import <Foundation/Foundation.h>
#include "archive.h"

#define CFLAG_V @"-v"
#define CFLAG_C @"-c"
#define CFLAG_X @"-x"
#define CFLAG_L @"-l"
#define CFLAG_I @"-i"

static int stdout_dup = -1;

__attribute__((noreturn)) static void usage(NSString *name)
{
    // Restore Redirected Stream
    if (stdout_dup != -1) dup2(stdout_dup, STDOUT_FILENO);
    
    printf("Usage: %s -[c:x:l:i] [-v] <Options>\n", [name UTF8String]);
    exit(EXIT_FAILURE);
}

int main(int argc, const char *const *argv)
{
    @autoreleasepool
    {
        NSMutableArray *args = [[NSMutableArray alloc] init];
        for (int i = 0; i < argc; i++) [args addObject:[NSString stringWithUTF8String:argv[i]]];
        NSString *name = args[0]; [args removeObjectAtIndex:0];

        if (![args containsObject:CFLAG_V]) {
            // Redirect stdout to /dev/null
            stdout_dup = dup(STDOUT_FILENO);

            int nul_out = open("/dev/null", O_WRONLY);
            dup2(nul_out, STDOUT_FILENO);
            dup2(nul_out, STDERR_FILENO);
        } else {
            [args removeObject:CFLAG_V];
        }
        
        if ([args containsObject:CFLAG_C]) {
            if ([args count] != 3) usage(name);
            [args removeObject:CFLAG_C];
            NSString *archive = args[0];
            NSString *rootdir = args[1];

            bool created = CAArchiveCreate((char *)[archive UTF8String], (char *)[rootdir UTF8String]);
            printf((created ? "C %s\n" : "F %s\n"), [archive UTF8String]);
            exit(created);
        } else if ([args containsObject:CFLAG_L]) {
            if ([args count] != 2) usage(name);
            [args removeObject:CFLAG_L];
            NSString *archive = args[0];

            if (stdout_dup != -1) dup2(stdout_dup, STDOUT_FILENO);
            CAArchiveListContents((char *)[archive UTF8String], NULL);
        } else if ([args containsObject:CFLAG_X]) {
            [args removeObject:CFLAG_X];
            
            if ([args count] == 2) {
                bool success = CAArchiveExtractAll((char *)[args[0] UTF8String], (char *)[args[1] UTF8String]);
                printf((success ? "X %s\n" : "F %s\n"), [args[0] UTF8String]);
            } else if ([args count] == 3) {
                bool success = CAArchiveExtractItem((char *)[args[0] UTF8String], (char *)[args[1] UTF8String], (char *)[args[2] UTF8String]);
                printf((success ? "X %s\n" : "F %s\n"), [args[0] UTF8String]);
            } else {
                usage(name);
            }
        } else if ([args containsObject:CFLAG_I]) {
            [args removeObject:CFLAG_I];
            if ([args count] != 1) usage(name);

            if (stdout_dup != -1) dup2(stdout_dup, STDOUT_FILENO);
            printf("Archive %s is %s\n", [args[0] UTF8String], (CAArchiveCheckValidity((char *)[args[0] UTF8String]) ? "VALID" : "INVALID"));
        } else {
            usage(name);
        }
    }

    return(0);
}
