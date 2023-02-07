#include <stdio.h>
#include <string.h>

#include "file_list.h"
#include "minitar.h"

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: %s -c|a|t|u|x -f ARCHIVE [FILE...]\n", argv[0]);
        return 0;
    }

    file_list_t files;
    file_list_init(&files);

    // TODO: Parse command-line arguments and invoke functions from 'minitar.h'
    // to execute archive operations

    if (!strcmp(argv[1], "-c")) {
        // create

        const char *archive_name = argv[3];

        // start at 4, first 3 args are non-file names
        for (int i = 4; i<argc; i++) {
            if(file_list_add(&files, argv[i])) {
                perror("Error adding file to file list");
                goto failure;
            }
        }

        if (create_archive(archive_name, &files)) {
            perror("Error in creating archive");
            goto failure;
        }

    } 
    
    else if (!strcmp(argv[1], "-a")) {
        printf("append called\n");
        // append
    } 
    
    else if (!strcmp(argv[1], "-t")) {
        printf("list called\n");
        // list
    } 
    
    else if (!strcmp(argv[1], "-u")) {
        printf("update called\n");
        // update
    } 
    
    else if (!strcmp(argv[1], "-x")) {
        printf("extract called\n");
        // extract
    } 
    
    else {
        // incorrect operation code
        printf("Incorrect operation code\n");
        printf("Usage: %s -c|a|t|u|x -f ARCHIVE [FILE...]\n", argv[0]);
        goto failure;
    }
    
    file_list_clear(&files);
    return 0;

failure:
    file_list_clear(&files);
    return 1;
}
