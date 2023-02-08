#include <fcntl.h>
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

#include "minitar.h"

#define NUM_TRAILING_BLOCKS 2
#define MAX_MSG_LEN 512

/*
 * Helper function to compute the checksum of a tar header block
 * Performs a simple sum over all bytes in the header in accordance with POSIX
 * standard for tar file structure.
 */
void compute_checksum(tar_header *header) {
    // Have to initially set header's checksum to "all blanks"
    memset(header->chksum, ' ', 8);
    unsigned sum = 0;
    char *bytes = (char *)header;
    for (int i = 0; i < sizeof(tar_header); i++) {
        sum += bytes[i];
    }
    snprintf(header->chksum, 8, "%07o", sum);
}

/*
 * Populates a tar header block pointed to by 'header' with metadata about
 * the file identified by 'file_name'.
 * Returns 0 on success or -1 if an error occurs
 */
int fill_tar_header(tar_header *header, const char *file_name) {
    memset(header, 0, sizeof(tar_header));
    char err_msg[MAX_MSG_LEN];
    struct stat stat_buf;
    // stat is a system call to inspect file metadata
    if (stat(file_name, &stat_buf) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to stat file %s", file_name);
        perror(err_msg);
        return -1;
    }

    strncpy(header->name, file_name, 100); // Name of the file, null-terminated string
    snprintf(header->mode, 8, "%07o", stat_buf.st_mode & 07777); // Permissions for file, 0-padded octal

    snprintf(header->uid, 8, "%07o", stat_buf.st_uid); // Owner ID of the file, 0-padded octal
    struct passwd *pwd = getpwuid(stat_buf.st_uid); // Look up name corresponding to owner ID
    if (pwd == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up owner name of file %s", file_name);
        perror(err_msg);
        return -1;
    }
    strncpy(header->uname, pwd->pw_name, 32); // Owner  name of the file, null-terminated string

    snprintf(header->gid, 8, "%07o", stat_buf.st_gid); // Group ID of the file, 0-padded octal
    struct group *grp = getgrgid(stat_buf.st_gid); // Look up name corresponding to group ID
    if (grp == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up group name of file %s", file_name);
        perror(err_msg);
        return -1;
    }
    strncpy(header->gname, grp->gr_name, 32); // Group name of the file, null-terminated string

    snprintf(header->size, 12, "%011o", (unsigned)stat_buf.st_size); // File size, 0-padded octal
    snprintf(header->mtime, 12, "%011o", (unsigned)stat_buf.st_mtime); // Modification time, 0-padded octal
    header->typeflag = REGTYPE; // File type, always regular file in this project
    strncpy(header->magic, MAGIC, 6); // Special, standardized sequence of bytes
    memcpy(header->version, "00", 2); // A bit weird, sidesteps null termination
    snprintf(header->devmajor, 8, "%07o", major(stat_buf.st_dev)); // Major device number, 0-padded octal
    snprintf(header->devminor, 8, "%07o", minor(stat_buf.st_dev)); // Minor device number, 0-padded octal

    compute_checksum(header);
    return 0;
}

/*
 * Removes 'nbytes' bytes from the file identified by 'file_name'
 * Returns 0 upon success, -1 upon error
 * Note: This function uses lower-level I/O syscalls (not stdio), which we'll learn about later
 */
int remove_trailing_bytes(const char *file_name, size_t nbytes) {
    char err_msg[MAX_MSG_LEN];
    // Note: ftruncate does not work with O_APPEND
    int fd = open(file_name, O_WRONLY);
    if (fd == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to open file %s", file_name);
        perror(err_msg);
        return -1;
    }
    //  Seek to end of file - nbytes
    off_t current_pos = lseek(fd, -1 * nbytes, SEEK_END);
    if (current_pos == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to seek in file %s", file_name);
        perror(err_msg);
        close(fd);
        return -1;
    }
    // Remove all contents of file past current position
    if (ftruncate(fd, current_pos) == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to truncate file %s", file_name);
        perror(err_msg);
        close(fd);
        return -1;
    }
    if (close(fd) == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to close file %s", file_name);
        perror(err_msg);
        return -1;
    }
    return 0;
}


int create_archive(const char *archive_name, const file_list_t *files) {

    // Open/Create tar file
    FILE* tar_file = fopen(archive_name, "w");

    if (tar_file == NULL) {
        perror("Error");
        return -1;
    }

    
    node_t *curfile = files->head;
    for (int i = 0; i<files->size; i++) {

        // Create and populate file header
        tar_header header;

        if(fill_tar_header(&header, curfile->name)) {
            perror("Error in creating file header");
            if (fclose(tar_file)) {
                perror("Error closing tar file");
            }
            return -1;
        }

        // Write file header to tar file
        if (fwrite(&header, sizeof(char), sizeof(tar_header), tar_file) < sizeof(tar_header)) {
            perror("Error writing header to tar file");
            if (fclose(tar_file)) {
                perror("Error closing tar file");
            }
            return -1;
        }
        
        // Open current file to be written to tar file
        FILE* f = fopen(curfile->name, "r");

        if (f == NULL) {
            perror("Error");
            if (fclose(tar_file)) {
                perror("Error closing tar file");
            }
            return -1;
        }

        int bytes_read = 1;
        char buf[512];

        // Initially fill the buffer with all zero bytes
        memset(&buf, 0, 512);
        
        while (bytes_read) {
            // Read into buf, which up to this point, will be filled with 0's
            bytes_read = fread(&buf, sizeof(char), sizeof(buf), f);

            // check that some amount of bytes were read
            if (bytes_read) {
                // Write to tar file and error check
                if (fwrite(&buf, sizeof(char), sizeof(buf), tar_file) < sizeof(buf)) {
                    perror("Error writing file data to tar file");
                    if (fclose(tar_file) || fclose(f)) {
                        perror("Error closing files");
                    }
                    return -1;
                }
            } else if (ferror(f)) {
                perror("Error reading data file");
                if (fclose(tar_file) || fclose(f)) {
                    perror("Error closing files");
                }
                return -1;
            }

            // Set buffer to 0 bytes
            memset(&buf, 0, 512);
        }

        // Close data file and error check
        if (fclose(f)) {
            perror("Error closing data file");
            if (fclose(tar_file)) {
                perror("Error closing tar file");
            }
            return -1;
        }

        curfile = curfile->next;
    }

    // Write the 2 512-byte zero blocks that act as a footer
    char footer[1024];
    memset(&footer, 0, sizeof(footer));
    
    // Write footer and error check
    if (fwrite(&footer, sizeof(char), sizeof(footer), tar_file) < sizeof(footer)) {
            perror("Error writing footer to tar file");
            if (fclose(tar_file)) {
                perror("Error closing tar file");
            }
            return -1;
    }

    // Close tar file and error check
    if (fclose(tar_file)) {
        perror("Error closing tar file");
        return -1;
    }

    return 0;
}

int append_files_to_archive(const char *archive_name, const file_list_t *files) {

    // Check that the tar file exists
    if (access(archive_name, F_OK) != 0) {
        printf("Archive %s doesn't exist\n", archive_name);
        return -1;
    }
    
    // Remove the footer from the tar file
    int ret = remove_trailing_bytes(archive_name, 1024);
    if(ret == -1) {
        perror("Error removing trailing bytes");
        return -1;
    }

    // Open tar file and error check
    FILE* tar_file = fopen(archive_name, "a");
    if(tar_file == NULL) {
        perror("Error opening tar file");
        return -1;
    }

    node_t *curfile = files->head;
    for (int i = 0; i<files->size; i++) {

        // Create and populate file header
        tar_header header;

        // Error check tar header
        if(fill_tar_header(&header, curfile->name)) {
            perror("Error in creating file header");
            if (fclose(tar_file)) {
                perror("Error closing tar file");
            }
            return -1;
        }

        // Write file header to tar file
        if (fwrite(&header, sizeof(char), sizeof(tar_header), tar_file) < sizeof(tar_header)) {
            perror("Error writing header to tar file");
            if (fclose(tar_file)) {
                perror("Error closing tar file");
            }
            return -1;
        }
        
        // Open current file to be written to tar file
        FILE* f = fopen(curfile->name, "r");

        if (f == NULL) {
            perror("Error");
            if (fclose(tar_file)) {
                perror("Error closing tar file");
            }
            return -1;
        }

        int bytes_read = 1;
        char buf[512];

        // Initially fill the buffer with all zero bytes
        memset(&buf, 0, 512);
        
        while (bytes_read) {
            // Read into buf, which up to this point, will be filled with 0's
            bytes_read = fread(&buf, sizeof(char), sizeof(buf), f);

            // Check that some amount of bytes were read
            if (bytes_read) {
                // Write to tar file and error check
                if (fwrite(&buf, sizeof(char), sizeof(buf), tar_file) < sizeof(buf)) {
                    perror("Error writing file data to tar file");
                    if (fclose(tar_file) || fclose(f)) {
                        perror("Error closing files");
                    }
                    return -1;
                }
            } else if (ferror(f)) {
                perror("Error reading data file");
                if (fclose(tar_file) || fclose(f)) {
                    perror("Error closing files");
                }
                return -1;
            }

            // Refill buffer with 0 bytes
            memset(&buf, 0, 512);
        }

        // Close file and error check
        if (fclose(f)) {
            perror("Error closing data file");
            if (fclose(tar_file)) {
                perror("Error closing tar file");
            }
            return -1;
        }

        curfile = curfile->next;
    }

    // Write the 2 512-byte zero blocks that act as a footer
    char footer[1024];
    memset(&footer, 0, sizeof(footer));
    
    if (fwrite(&footer, sizeof(char), sizeof(footer), tar_file) < sizeof(footer)) {
            perror("Error writing footer to tar file");
            if (fclose(tar_file)) {
                perror("Error closing tar file");
            }
            return -1;
    }

    // Close file and error check
    if (fclose(tar_file)) {
        perror("Error closing tar file");
        return -1;
    }

    return 0;
}

int get_archive_file_list(const char *archive_name, file_list_t *files) {

    // Open tar file and error check
    FILE* tar_file = fopen(archive_name, "r");
    if(tar_file == NULL) {
        perror("Error opening tar file");
        return -1;
    }


    int convertedSize = 1;

    while(convertedSize != 0) {
        tar_header header;

        // Read tar data into header object and error check
        fread(&header, 1, sizeof(tar_header), tar_file);
        if(ferror(tar_file)) {
            perror("Error reading");
            return -1;
        }

        // Convert read header data into integer representing file size
        convertedSize = 0;
        int num_scanned = sscanf(header.size, "%o", &convertedSize);
        if(convertedSize < 1) {
            break;
        }

        // Error check sscanf
        if(num_scanned < 1) {
            perror("Error reading header size");
            return -1;
        }

        // Add file name to linked list and error check
        if (file_list_add(files, header.name)) {
            perror("Error adding file to file list when reading existing archive");
            if (fclose(tar_file)) {
                perror("Error closing tar file");
            }
            return -1;
        }

        // Seek to next header in tar file
        fseek(tar_file, convertedSize + (512 - (convertedSize % 512)), SEEK_CUR);

    }

    // Close tar file and error check
    if (fclose(tar_file)) {
        perror("Error closing tar file");
        return -1;
    }
    
    return 0;
}

int extract_files_from_archive(const char *archive_name) {
    // TODO: Not yet implemented
    return 0;
}
