 #include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include <errno.h>    /* for errno                   */
#include "crc.h"      /* for crc()                   */
#include "zutil.h"    /* for mem_def() and mem_inf() */
#include "lab_png.h"  /* simple PNG data structures  */
#include <libgen.h>
#define _GNU_SOURCE

 

int main (int argc, char *argv[])
{
    U8 buffer[8];
    U32 header_CRC[4];
    U32 length_IDAT[4];
    U32 length_IEND[4];
    FILE * fp = fopen(argv[1], "rb");

    //printing
    char *pfile;
    pfile = argv[1] + strlen(argv[1]);
    for (; pfile > argv[1]; pfile--)
    {
        if ((*pfile == '\\') || (*pfile == '/'))
        {
            pfile++;
            break;
        }
    }

    //read header
    fread(buffer, 8, 1, fp);

    fseek(fp, 16, 0);
     if (!(buffer[1] == 0x50 && buffer[2] == 0x4E && buffer[3] == 0x47)){
        printf("%s: Not a PNG file\n", pfile);
        return 0;
    }
    fseek(fp,8,0);

    //read in ihdr chunk
    chunk_p chunk_IHDR = malloc(25);
    fread(chunk_IHDR, 25, 1, fp);


    //read in ihdr crc
    unsigned int crc_IHDR;
    fseek(fp, 29, SEEK_SET);
    fread(&crc_IHDR, 4,1,fp);

    //read in ihdr type and data
    unsigned char type_data_IHDR[ntohl(chunk_IHDR -> length) + 4];
    fseek(fp, 12, 0);
    fread(type_data_IHDR ,ntohl(chunk_IHDR -> length) + 4, 1, fp);
    //printf("ihdr calculated crc = %u\n", ntohl(crc(type_data_IHDR, 4+ntohl(chunk_IHDR->length))));


    fseek(fp, 16, 0);

    //ihdr data
    data_IHDR_p data_IHDR = malloc(DATA_IHDR_SIZE);
    fread(data_IHDR, 13, 1, fp);  
    //printf("bit = %u\n", (data_IHDR->bit_depth)); 


    fseek(fp, 33, 0);

    ///IDAT chunk data length
    chunk_p idat_chunk_length = malloc(4);
    fread(idat_chunk_length, 4 ,1, fp);

    //IDAT chunk read
    chunk_p chunk_IDAT = malloc(12 + ntohl(idat_chunk_length->length));
    fseek(fp, 33, 0);
    fread(chunk_IDAT, 12 + ntohl(idat_chunk_length->length) ,1, fp);

    //IDAT expected crc
    unsigned int crc_IDAT;
    fseek(fp, 41+ntohl(chunk_IDAT->length), SEEK_SET);
    fread(&crc_IDAT, 4,1,fp);
   // printf("idat crc = %x\n", (crc_IDAT));

    //IDAT calculated crc
    unsigned char type_data_IDAT[ntohl(chunk_IDAT -> length) + 4];
    fseek(fp, 37, 0);
    fread(type_data_IDAT ,ntohl(chunk_IDAT -> length) + 4, 1, fp);
    //printf("idat calculated crc = %u\n", ntohl(crc(type_data_IDAT, 4+ntohl(chunk_IDAT->length))));
    fseek(fp, 37, 0);

    //IEND CALCULATIONS
    chunk_p chunk_IEND = malloc(12);
    fseek(fp, 45+ntohl(chunk_IDAT->length), 0);
    fread(chunk_IEND, 12  ,1, fp);

    //IEND expected crc
    unsigned int crc_IEND;
    fseek(fp, 53+ntohl(chunk_IDAT->length), SEEK_SET);
    fread(&crc_IEND, 4,1,fp);
    //printf("iend crc = %u\n", (crc_IEND));

    //IEND calculated crc
    unsigned char type_data_IEND[ntohl(chunk_IEND -> length) + 4];
    fseek(fp, 49+ntohl(chunk_IDAT->length), 0);
    fread(type_data_IEND ,ntohl(chunk_IEND -> length) + 4, 1, fp);
    //printf("iend calculated crc = %u\n", ntohl(crc(type_data_IEND, 4+ntohl(chunk_IEND->length))));


    if (buffer[1] == 0x50 && buffer[2] == 0x4E && buffer[3] == 0x47){
        printf("%s: %i x %i\n", pfile, ntohl(data_IHDR->width), ntohl(data_IHDR->height));
        if (crc_IHDR != ntohl(crc(type_data_IHDR, 4+ntohl(chunk_IHDR->length)))){
            printf("IHDR chunk CRC error: computed %x, expected %x\n", (crc(type_data_IHDR, 4+ntohl(chunk_IHDR->length))), ntohl(crc_IHDR ));
        }
         if (crc_IDAT != ntohl(crc(type_data_IDAT, 4+ntohl(chunk_IDAT->length)))){
            printf("IDAT chunk CRC error: computed %x, expected %x\n",  (crc(type_data_IDAT, 4+ntohl(chunk_IDAT->length))), ntohl(crc_IDAT) );
        }
         if (crc_IEND != ntohl(crc(type_data_IEND, 4+ntohl(chunk_IEND->length)))){
            printf("IEND chunk CRC error: computed %x, expected %x\n", (crc(type_data_IEND, 4+ntohl(chunk_IEND->length))), ntohl(crc_IEND ));
        }

    }else{
        printf("%s: Not a PNG file\n", pfile);
    }
    free(chunk_IHDR);
    free(chunk_IDAT);
    free(chunk_IEND);
    free(data_IHDR);
    free(idat_chunk_length);
    fclose(fp);
    return 0; 
}


  