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
    unsigned int total_height = 0;
    unsigned int height = 0;
    unsigned int width = 0;

    char *inflated_buffer = malloc(50000000);

    U64 offset = 0;

    for (int i =1 ; i < argc; i++ ){
        FILE * fp = fopen(argv[i], "rb+");


        //get header
        U8 header[8];
        fread(header, 8, 1, fp);

        //get ihdr data
        fseek(fp, 16, 0);
        data_IHDR_p data_IHDR = malloc(DATA_IHDR_SIZE);
        fread(data_IHDR, 13, 1, fp);
        //get height and width
        total_height += ntohl(data_IHDR -> height);
        height = ntohl(data_IHDR -> height);

        width = ntohl(data_IHDR->width);


        //get idat
        chunk_p idat_chunk_length = malloc(4);
        fseek(fp, 33, 0);
        fread(idat_chunk_length, 4 ,1, fp);
        chunk_p chunk_IDAT = malloc(12 + ntohl(idat_chunk_length->length));
        fseek(fp, 33, 0);
        fread(chunk_IDAT, 12 + ntohl(idat_chunk_length->length) ,1, fp);

        U8 *data_buffer = malloc(ntohl(chunk_IDAT->length)); 
        fseek(fp, 41,0);
        fread(data_buffer,ntohl(chunk_IDAT->length), 1, fp);

        U64 inflated_data_length = (height * (width *4 +1));

        U64 * inflated_data = malloc(inflated_data_length+1);
        mem_inf(inflated_data, &inflated_data_length, data_buffer, ntohl(chunk_IDAT->length) );

        memcpy(inflated_buffer + offset, inflated_data, inflated_data_length);
        
        offset += inflated_data_length;

        fclose(fp);
        free(data_IHDR);
        free(idat_chunk_length);
        free(data_buffer);
        free(chunk_IDAT);
        free(inflated_data);
    }
    
    U8 * deflated_data=malloc(200000);
    unsigned int deflated_data_length = 0;
    

    mem_def(deflated_data, &deflated_data_length, inflated_buffer, 5000000, Z_DEFAULT_COMPRESSION);

    FILE * fp = fopen("all.png", "wb+");
    U8 header[8];
    header[0] = 0x89;
    header[1] = 0x50;
    header[2] = 0x4E;
    header[3] = 0x47;
    header[4] = 0x0D;
    header[5] = 0x0A;
    header[6] = 0x1A;
    header[7] = 0x0A;

    fwrite(header, 8, 1, fp);

    U8 ihdr_length_type[8];
    ihdr_length_type[0] = 0x00;
    ihdr_length_type[1] = 0x00;
    ihdr_length_type[2] = 0x00;
    ihdr_length_type[3] = 0x0D;
    ihdr_length_type[4] = 0x49;
    ihdr_length_type[5] = 0x48;
    ihdr_length_type[6] = 0x44;
    ihdr_length_type[7] = 0x52;
    fwrite(ihdr_length_type, 8, 1, fp);

    width = htonl(width);
    total_height = htonl(total_height);
    fwrite(&width, 4, 1, fp);
    fwrite(&total_height, 4, 1, fp);

    U8 data_IHDR_remaining[5];
    data_IHDR_remaining[0] = 0x08;
    data_IHDR_remaining[1] = 0x06;
    data_IHDR_remaining[2] = 0x00;
    data_IHDR_remaining[3] = 0x00;
    data_IHDR_remaining[4] = 0x00;
    fwrite(data_IHDR_remaining, 5, 1, fp);

    //read in ihdr type and data
    U8 type_data_IHDR[17];
    fseek(fp, 12, SEEK_SET);
    fread(type_data_IHDR ,17, 1, fp);
    U32 calculated_crc_IHDR = (htonl(crc((type_data_IHDR), 17)));
    fseek(fp, 29, 0);
    fwrite(&calculated_crc_IHDR, 4, 1, fp);

    //idat
    
    unsigned int d = (deflated_data_length);
    d = htonl(deflated_data_length);

    fseek(fp, 33, 0);
    fwrite((&d), 4,1,fp);
    fseek(fp,37,0);
    U8 idat_type[4];
    idat_type[0] = 0x49;
    idat_type[1] = 0x44;
    idat_type[2] = 0x41;
    idat_type[3] = 0x54;
    fwrite(idat_type, 4, 1, fp);
    
    fseek(fp, 41, SEEK_SET);
    fwrite(deflated_data, deflated_data_length, 1, fp);
    

    //calculate crc
    U8 type_data_IDAT[4+deflated_data_length];
    fseek(fp, 37, SEEK_SET);
    fread(type_data_IDAT ,4+deflated_data_length, 1, fp);
    U32 calculated_crc_IDAT = (htonl(crc(type_data_IDAT, 4+deflated_data_length)));
    fseek(fp,41+deflated_data_length , 0);
    fwrite(&calculated_crc_IDAT, 4, 1, fp);

    //iend
    U8 iend_length[4];
    iend_length[0] = 0x00;
    iend_length[1] = 0x00;
    iend_length[2] = 0x00;
    iend_length[3] = 0x00;
    fwrite(iend_length, 4,1,fp);

    U8 iend_type[4];
    iend_type[0] = 0x49;
    iend_type[1] = 0x45;
    iend_type[2] = 0x4E;
    iend_type[3] = 0x44;
    fwrite(iend_type, 4, 1, fp);


    unsigned int calculated_crc_IEND = (htonl(crc(iend_type, 4)));
    fseek(fp,53+deflated_data_length , 0);
    fwrite(&calculated_crc_IEND, 4, 1, fp);

    free(inflated_buffer);
    free(deflated_data);
    fclose(fp);
    return 0;
}
