#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>  /* for printf().  man 3 printf */
#include <stdlib.h> /* for exit().    man 3 exit   */
#include <string.h> /* for strcat().  man strcat   */
#include "lab_png.h"

void scan_directory(char *d_name, int *png_counter)
{
    DIR *p_dir;
    struct dirent *p_dirent;
    char str[64];
    struct stat buf;

    // Check if directory can be opened
    if ((p_dir = opendir(d_name)) == NULL)
    {
        fprintf(stderr, "cannot open directory: %s\n", d_name);
        return;
    }

    while ((p_dirent = readdir(p_dir)) != NULL)
    {

        char *str_path = p_dirent->d_name; /* relative path name! */

        if (str_path == NULL)
        {
            fprintf(stderr, "Null pointer found!");
            exit(3);
        }
        else if (p_dirent->d_type == 8)
        {
            // check for valid png
            char file_name[256];
            sprintf(file_name, "%s/%s", d_name, str_path);
            FILE *fp = fopen(file_name, "rb");
            U8 buffer[8];
            fread(buffer, 8, 1, fp);
            if ((buffer[1] == 0x50 && buffer[2] == 0x4E && buffer[3] == 0x47))
            {
                printf("%s/%s\n", d_name, str_path);
                (*png_counter)++;
                fclose(fp);
                continue;
            }
            fclose(fp);
        }
        else if (p_dirent->d_type == 4)
        {
            if (strcmp(str_path, ".") != 0 && strcmp(str_path, "..") != 0) 
            {
                char directory_name[256];
                sprintf(directory_name, "%s/%s", d_name, str_path);
                scan_directory(directory_name, png_counter);
            }
        }
    }

    if (closedir(p_dir) != 0)
    {
        perror("closedir");
        exit(3);
    }
}

int main(int argc, char *argv[])
{
    DIR *p_dir;
    struct dirent *p_dirent;
    char str[64];

    if (argc == 1)
    {
        fprintf(stderr, "Usage: %s <directory name>\n", argv[0]);
        exit(1);
    }
    int png_counter = 0;

    scan_directory(argv[1], &png_counter);

    if (!png_counter)
    {
        printf("findpng: No PNG file found\n");
    }

    return 0;
}