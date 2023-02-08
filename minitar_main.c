#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "file_list.h"
#include "minitar.h"

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: %s -c|a|t|u|x -f ARCHIVE [FILE...]\n", argv[0]);
        return 0;
    }

    file_list_t files;
    file_list_init(&files);

    // Grab archive name
    const char *archive_name = argv[3];

    if (!strcmp(argv[1], "-c")) {
        // create

        // start at 4, first 3 args are non-file names
        for (int i = 4; i<argc; i++) {
            if(file_list_add(&files, argv[i])) {
                perror("Error adding file to file list");
                goto failure;
            }
        }

        // Error checking create
        if (create_archive(archive_name, &files)) {
            perror("Error in creating archive");
            goto failure;
        }

    } 
    
    else if (!strcmp(argv[1], "-a")) {
        // append

        // start at 4, first 3 args are non-file names
        for (int i = 4; i<argc; i++) {
            if(file_list_add(&files, argv[i])) {
                perror("Error adding file to file list");
                goto failure;
            }
        }

        // Error checking append
        if (append_files_to_archive(archive_name, &files)) {
            perror("Error in appending files to archive");
            goto failure;
        }

    } 
    
    else if (!strcmp(argv[1], "-t")) {
        // list

        // Error checking get file list
        if (get_archive_file_list(archive_name, &files)) {
            perror("Error in getting the list of files");
            goto failure;
        }

        // Print out file list
        node_t * currNode = files.head;
        while(currNode != NULL) {
            printf("%s\n", currNode->name);
            currNode = currNode->next;
        }
    } 
    
    else if (!strcmp(argv[1], "-u")) {
        // update

        // Checks that archive exists
        if (access(archive_name, F_OK) != 0) {
            printf("Archive %s doesn't exist\n", archive_name);
            goto failure;
        }

        // Get the list of files in the archive
        file_list_t archive_files;
        file_list_init(&archive_files);

        // Grab file list from archive
        if (get_archive_file_list(archive_name, &archive_files)) {
            perror("Error in getting the list of files");
            file_list_clear(&archive_files);
            goto failure;
        }

        // start at 4, first 3 args are non-file names
        // make list of files specified by user
        for (int i = 4; i<argc; i++) {
            if(file_list_add(&files, argv[i])) {
                perror("Error adding file to file list");
                file_list_clear(&archive_files);
                goto failure;
            }
        }

        // Check that all specified update files are within the archive already
        if (file_list_is_subset(&files, &archive_files)) {
            // Error checking append
            if (append_files_to_archive(archive_name, &files)) {
                perror("Error in appending files to archive");
                file_list_clear(&archive_files);
                goto failure;
            }
        } else {
            printf("Error: One or more of the specified files is not already present in archive\n");
            file_list_clear(&archive_files);
            goto failure;
        }
        file_list_clear(&archive_files);

    } 
    
    else if (!strcmp(argv[1], "-x")) {
        // extract

        // Checks that archive exists
        if (access(archive_name, F_OK) != 0) {
            printf("Archive %s doesn't exist\n", archive_name);
            goto failure;
        }

        // Extract files and error check
        if(extract_files_from_archive(archive_name)) {
            perror("Error extracting files from archive");
            goto failure;
        }
    } 
    
    else {
        // incorrect operation code
        printf("Incorrect operation code\n");
        printf("Usage: %s -c|a|t|u|x -f ARCHIVE [FILE...]\n", argv[0]);
        goto failure;
    }
    
    file_list_clear(&files);
    return 0;

// Will only reach here if called in error catching
failure:
    file_list_clear(&files);
    return 1;
}
