/*
 * The code is derived from cURL example and paster.c base code.
 * The cURL example is at URL:
 * https://curl.haxx.se/libcurl/c/getinmemory.html
 * Copyright (C) 1998 - 2018, Daniel Stenberg, <daniel@haxx.se>, et al..
 *
 * The paster.c code is 
 * Copyright 2013 Patrick Lam, <p23lam@uwaterloo.ca>.
 *
 * Modifications to the code are
 * Copyright 2018-2019, Yiqing Huang, <yqhuang@uwaterloo.ca>.
 * 
 * This software may be freely redistributed under the terms of the X11 license.
 */

/** 
 * @file main.c
 * @brief cURL write call back to save received data in a shared memory first
 *        and then write the data to a file for verification purpose.
 *        cURL header call back extracts data sequence number from header.
 * @see https://curl.haxx.se/libcurl/c/getinmemory.html
 * @see https://curl.haxx.se/libcurl/using/
 * @see https://ec.haxx.se/callback-write.html
 * NOTE: we assume each image segment from the server is less than 10K
 */ 


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
//#include "shm_stack.h"


#include <errno.h>    /* for errno                   */
#include "crc.h"      /* for crc()                   */
#include "zutil.h"    /* for mem_def() and mem_inf() */
#include "lab_png.h"  /* simple PNG data structures  */
#include <sys/queue.h>
#include <curl/curl.h>
#include <sys/types.h>



#define IMG_URL "http://ece252-1.uwaterloo.ca:2530/image?img=1&part=20"
#define DUM_URL "https://example.com/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 10240 /* 1024*10 = 10K */

/* This is a flattened structure, buf points to 
   the memory address immediately after 
   the last member field (i.e. seq) in the structure.
   Here is the memory layout. 
   Note that the memory is a chunk of continuous bytes.

   On a 64-bit machine, the memory layout is as follows:

   +================+
   | buf            | 8 bytes
   +----------------+
   | size           | 8 bytes
   +----------------+
   | max_size       | 8 bytes
   +----------------+
   | seq            | 4 bytes
   +----------------+
   | padding        | 4 bytes
   +----------------+
   | buf[0]         | 1 byte
   +----------------+
   | buf[1]         | 1 byte
   +----------------+
   | ...            | 1 byte
   +----------------+
   | buf[max_size-1]| 1 byte
   +================+
*/
typedef struct recv_buf_flat {
    U8 buf[10000];       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
//int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);


/**
 * @brief  cURL header call back function to extract image sequence number from 
 *         http header data. An example header for image part n (assume n = 2) is:
 *         X-Ece252-Fragment: 2
 * @param  char *p_recv: header data delivered by cURL
 * @param  size_t size size of each memb
 * @param  size_t nmemb number of memb
 * @param  void *userdata user defined data structurea
 * @return size of header data received.
 * @details this routine will be invoked multiple times by the libcurl until the full
 * header data are received.  we are only interested in the ECE252_HEADER line 
 * received so that we can extract the image sequence number from it. This
 * explains the if block in the code.
 */
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;
    
    if (realsize > strlen(ECE252_HEADER) &&
	strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0) {

        /* extract img sequence number */
	p->seq = atoi(p_recv + strlen(ECE252_HEADER));

    }
    return realsize;
}


/**
 * @brief write callback function to save a copy of received data in RAM.
 *        The received libcurl data are pointed by p_recv, 
 *        which is provided by libcurl and is not user allocated memory.
 *        The user allocated memory is at p_userdata. One needs to
 *        cast it to the proper struct to make good use of it.
 *        This function maybe invoked more than once by one invokation of
 *        curl_easy_perform().
 */

size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;
 
    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */ 
        fprintf(stderr, "User buffer is too small, abort...\n");
        abort();
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}

// int shm_recv_buf_init(RECV_BUF *ptr, size_t nbytes)
// {
//     if ( ptr == NULL ) {
//         return 1;
//     }
    
//     ptr->buf = (char *)ptr + sizeof(RECV_BUF);
//     ptr->size = 0;
//     ptr->max_size = nbytes;
//     ptr->seq = -1;              /* valid seq should be non-negative */
    
//     return 0;
// }

int recv_buf_init(RECV_BUF *ptr, size_t max_size)
{
    
    
    if (ptr == NULL) {
        return 1;
    }

   
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1;              /* valid seq should be non-negative */
    return 0;
}

int recv_buf_cleanup(RECV_BUF *ptr)
{
    if (ptr == NULL) {
	return 1;
    }
    
    free(ptr->buf);
    ptr->size = 0;
    ptr->max_size = 0;
    return 0;
}


/**
 * @brief output data in memory to a file
 * @param path const char *, output file path
 * @param in  void *, input data to be written to the file
 * @param len size_t, length of the input data in bytes
 */

int write_file(const char *path, const void *in, size_t len)
{
    FILE *fp = NULL;

    if (path == NULL) {
        fprintf(stderr, "write_file: file name is null!\n");
        return -1;
    }

    if (in == NULL) {
        fprintf(stderr, "write_file: input data is null!\n");
        return -1;
    }

    fp = fopen(path, "wb");
    if (fp == NULL) {
        perror("fopen");
        return -2;
    }

    if (fwrite(in, 1, len, fp) != len) {
        fprintf(stderr, "write_file: imcomplete write!\n");
        return -3; 
    }
    return fclose(fp);
}


typedef struct buf_stack
{
    int size;               /* the max capacity of the stack */
    int pos;                /* position of last item pushed onto the stack */
    RECV_BUF *items;             /* stack of stored buffers */
} ISTACK;

/**
 * @brief calculate the total memory that the struct int_stack needs and
 *        the items[size] needs.
 * @param int size maximum number of integers the stack can hold
 * @return return the sum of ISTACK size and the size of the data that
 *         items points to.
 */

int sizeof_shm_stack(int size)
{
    return (sizeof(ISTACK) + sizeof(RECV_BUF) * size);
}

/**
 * @brief initialize the ISTACK member fields.
 * @param ISTACK *p points to the starting addr. of an ISTACK struct
 * @param int stack_size max. number of items the stack can hold
 * @return 0 on success; non-zero on failure
 * NOTE:
 * The caller first calls sizeof_shm_stack() to allocate enough memory;
 * then calls the init_shm_stack to initialize the struct
 */
int init_shm_stack(ISTACK *p, int stack_size)
{
    if ( p == NULL || stack_size == 0 ) {
        return 1;
    }

    p->size = stack_size;
    p->pos  = -1;
    p->items = (RECV_BUF *) ((char *)p + sizeof(ISTACK));
    return 0;
}

/**
 * @brief create a stack to hold size number of integers and its associated
 *      ISTACK data structure. Put everything in one continous chunk of memory.
 * @param int size maximum number of integers the stack can hold
 * @return NULL if size is 0 or malloc fails
 */

ISTACK *create_stack(int size)
{
    int mem_size = 0;
    ISTACK *pstack = NULL;
    
    if ( size == 0 ) {
        return NULL;
    }

    mem_size = sizeof_shm_stack(size);
    pstack = malloc(mem_size);

    if ( pstack == NULL ) {
        perror("malloc");
    } else {
        char *p = (char *)pstack;
        pstack->items = (RECV_BUF *) (p + sizeof(ISTACK));
        pstack->size = size;
        pstack->pos  = -1;
    }

    return pstack;
}

/**
 * @brief release the memory
 * @param ISTACK *p the address of the ISTACK data structure
 */

void destroy_stack(ISTACK *p)
{
    if ( p != NULL ) {
        free(p);
    }
}

/**
 * @brief check if the stack is full
 * @param ISTACK *p the address of the ISTACK data structure
 * @return non-zero if the stack is full; zero otherwise
 */

int is_full(ISTACK *p)
{
    if ( p == NULL ) {
        return 0;
    }
    return ( p->pos == (p->size -1) );
}

/**
 * @brief check if the stack is empty 
 * @param ISTACK *p the address of the ISTACK data structure
 * @return non-zero if the stack is empty; zero otherwise
 */

int is_empty(ISTACK *p)
{
    if ( p == NULL ) {
        return 0;
    }
    return ( p->pos == -1 );
}

/**
 * @brief push one integer onto the stack 
 * @param ISTACK *p the address of the ISTACK data structure
 * @param int item the integer to be pushed onto the stack 
 * @return 0 on success; non-zero otherwise
 */

int push(ISTACK *p, RECV_BUF item)
{
    if ( p == NULL ) {
        return -1;
    }

    if ( !is_full(p) ) {
        ++(p->pos);
        p->items[p->pos] = item;
        return 0;
    } else {
        return -1;
    }
}

/**
 * @brief push one integer onto the stack 
 * @param ISTACK *p the address of the ISTACK data structure
 * @param int *item output parameter to save the integer value 
 *        that pops off the stack 
 * @return 0 on success; non-zero otherwise
 */

int pop(ISTACK *p, RECV_BUF *p_item)
{
    if ( p == NULL ) {
        return -1;
    }

    if ( !is_empty(p) ) {
        *p_item = p->items[p->pos];
        (p->pos)--;
        return 0;
    } else {
        return 1;
    }
}
typedef struct INF_BUF {
    U8 buf[9606];       /* memory to hold a copy of received data */
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} INF_BUF;
 


int main( int argc, char** argv ) 
{

    if (argc < 5){
        printf("invalid input");
        return 0;
    }
    int B = atoi(argv[1]);
    int P = atoi(argv[2]);
    int C = atoi(argv[3]);
    int X = atoi(argv[4]);
    int N = atoi(argv[5]);

    ISTACK *strip_buffer;
    int strip_shmid = shmget(IPC_PRIVATE, sizeof_shm_stack(B), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (strip_shmid == -1){
        perror("shmget");
        abort();
    }
    strip_buffer = shmat(strip_shmid, NULL, 0);
    init_shm_stack(strip_buffer, (B));
    

    INF_BUF *inflated_buffer;
    int INF_shmid = shmget(IPC_PRIVATE, sizeof(INF_BUF) * 50, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (INF_shmid == -1){
        perror("shmget");
        abort();
    }
    inflated_buffer = shmat(INF_shmid, NULL, 0);

    int *total_height ;
    int *width ;

    int shm_total_height_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int shm_width_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    total_height = shmat(shm_total_height_id, NULL, 0);
    width = shmat(shm_width_id, NULL, 0);

    int *prod_count ;
    int *cons_count ;

    int shm_prod_cons_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int shm_cons_count_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    prod_count = shmat(shm_prod_cons_id, NULL, 0);
    cons_count = shmat(shm_cons_count_id, NULL, 0);

    //semaphore memory

    sem_t *sem_buff;
    sem_t *sem_empty;
    sem_t *sem_filled;
    sem_t *sem_prod_count;
    sem_t *sem_cons_count;


    int sem_buff_shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int sem_empty_shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int sem_filled_shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int sem_prod_count_shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int sem_cons_count_shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    sem_buff = shmat(sem_buff_shmid, NULL, 0);
    sem_empty = shmat(sem_empty_shmid, NULL, 0);
    sem_filled = shmat(sem_filled_shmid, NULL, 0);
    sem_prod_count = shmat(sem_prod_count_shmid, NULL, 0);
    sem_cons_count = shmat(sem_cons_count_shmid, NULL, 0);

    sem_init(sem_buff, 1, 1);
    sem_init(sem_empty, 1, B);
    sem_init(sem_filled, 1, 0);
    sem_init(sem_prod_count, 1,1);
    sem_init(sem_cons_count, 1, 1);

        


    RECV_BUF recv_buf;
    pid_t prod[P];
    pid_t cons[C];

  double times[2];
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;

    for (int i =0 ; i < P; i++){
        pid_t pid = fork();
        if(pid >0) {
            //store the PID
            prod[i]= pid;

        }
        else if ( pid == 0){

                 
                while(1){
                    int sequence_num =0;
                    sem_wait(sem_prod_count);

                    if(*prod_count == 50){
                        sem_post(sem_prod_count);
                        exit(0);
                    }
                    sequence_num = *prod_count;
                    *prod_count +=1;
                
                    sem_post(sem_prod_count);

                    CURL *curl_handle;
                    CURLcode res;
                    char url[256];
                    
                    //RECV_BUF *p_shm_recv_buf;

                    //int shmid;
                    //int shm_size = sizeof_shm_recv_buf(BUF_SIZE);
                    char fname[256];
                    pid_t pid =getpid();
                   // printf("aaa\n");

                    //shm_recv_buf_init(&recv_buf, BUF_SIZE);
                    recv_buf_init(&recv_buf, BUF_SIZE);

                    //printf("ggg\n");
                    
                    
                    sprintf(url, "http://ece252-%d.uwaterloo.ca:2530/image?img=%d&part=%d",sequence_num%3 +1 , N, sequence_num);
                    //printf("ggg\n");
                    //printf("URL is %s\n", url);
                    
                    
                    curl_global_init(CURL_GLOBAL_DEFAULT);

                    /* init a curl session */
                    curl_handle = curl_easy_init();

                    if (curl_handle == NULL) {
                        fprintf(stderr, "curl_easy_init: returned NULL\n");
                        return 1;
                    }

                    /* specify URL to get */
                    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

                    /* register write call back function to process received data */
                    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl); 
                    /* user defined data structure passed to the call back function */
                    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);

                    /* register header call back function to process received header data */
                    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl); 
                    /* user defined data structure passed to the call back function */
                    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);

                    /* some servers requires a user-agent field */
                    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

                    
                    /* get it! */
                    res = curl_easy_perform(curl_handle);

                    if( res != CURLE_OK) {
                        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                    } 
                    sem_wait(sem_empty);
                    sem_wait(sem_buff);
                    push(strip_buffer, recv_buf);
                    sem_post(sem_buff);
                    sem_post(sem_filled);
                    //printf("%i\n", strip_buffer->items[i].seq);
                    
                    // int *t = shmat(data_shmid, NULL, 0);
                    // memcpy(t + offset, recv_buf.buf, recv_buf.size);
                    // offset+=recv_buf.size;
                    

                
                    //write_file(fname, recv_buf.buf, recv_buf.size);

                    /* cleaning up */
                    curl_easy_cleanup(curl_handle);
                    curl_global_cleanup();
                    //recv_buf_cleanup(&recv_buf);

                }
                
                
            
        }
        else{
            perror("forking issue");
            abort();
        }
    }

    int offset = 0;
    for (int i =0 ; i < C; i++){
        pid_t pid = fork();
        if(pid>0){
            cons[i]=pid;
        }
        else if (pid == 0){

            while(1){

                
                sem_wait(sem_cons_count);

                if(*cons_count == 50){
                        sem_post(sem_cons_count);
                        exit(0);
                }
                *cons_count +=1;
      

                sem_post(sem_cons_count);

                RECV_BUF *cons_buf = malloc(sizeof(RECV_BUF));

                sem_wait(sem_filled);
                sem_wait(sem_buff);
                pop(strip_buffer, cons_buf);
                sem_post(sem_buff);
                sem_post(sem_empty);

             
                data_IHDR_p data_IHDR = malloc(DATA_IHDR_SIZE);
                // printf("fdsf3\n");
                memcpy(data_IHDR, cons_buf->buf + 16, 13);
                // printf("fdsf4\n");
                *total_height += ntohl(data_IHDR -> height);

                *width = ntohl(data_IHDR->width);
                free(data_IHDR);



                //write_file("22.png", cons_buf->buf, cons_buf->size);
                
                //memcpy(cons_buf.buf ,strip_buffer->items[i].buf, strip_buffer->items[i].size);
                // cons_buf.seq = strip_buffer->items[i].seq;
                // cons_buf.size = strip_buffer->items[i].size;
                U64 inf_data_length = 0;
                
                //mem_inf(inflated_data, &inflated_data_length, data_buffer, ntohl(chunk_IDAT->length) );
                INF_BUF temp_inf_buf;
                temp_inf_buf.seq = cons_buf->seq;

                int idat_data_length;
                memcpy (&idat_data_length, cons_buf-> buf + 33 ,4);
                
                mem_inf(&temp_inf_buf.buf, &inf_data_length, cons_buf->buf + 41, ntohl(idat_data_length) );

                usleep(X*1000);
                //memcpy(inflated_buffer + offset, &temp_inf_buf, inf_data_length );

                //offset += sizeof(temp_inf_buf);
                inflated_buffer[cons_buf->seq] = temp_inf_buf;
                free(cons_buf);

                //printf("%i\n" , offset);
                //write_file("22.png", cons_buf.buf, cons_buf.size);

            }
        }

        else{
            perror("fork error");
            abort();
        }
    }
    

    for(int i = 0; i<P;i++){
        wait(prod[i]);
    }
      for(int i = 0; i<C;i++){
        wait(cons[i]);
    }
    
  

    //char *inflated_buffer = malloc(9000000);


    U8 * deflated_data=malloc(2000000);
     U64 deflated_data_length = 0;
    U8 * inflated_data = malloc(9000000);
    int offset2 = 0;
    for (int i = 0; i < 50; i++){
       
        memcpy(inflated_data + offset2,  inflated_buffer[i].buf, 9606);
        offset2 += 9606;
    }


    mem_def(deflated_data, &deflated_data_length, inflated_data, 9606 * 50, Z_DEFAULT_COMPRESSION);

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

    int new_width = htonl(*width);;
    int new_total_height = htonl(*total_height);;

    fwrite(&new_width, 4, 1, fp);
    fwrite(&new_total_height, 4, 1, fp);

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

    free(inflated_data);
    free(deflated_data);
    fclose(fp);

     if (gettimeofday(&tv, NULL) != 0) {
            perror("gettimeofday");
            abort();
        }
        times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
        printf("paster2 execution time: %.6lf seconds\n",  times[1] - times[0]);

    sem_destroy(sem_buff);
    sem_destroy(sem_empty);
    sem_destroy(sem_filled);
    sem_destroy(sem_prod_count);
    sem_destroy(sem_cons_count);
    
    shmdt(strip_buffer);
    shmdt(inflated_buffer);
    shmdt(total_height);
    shmdt(width);
    shmdt(prod_count);
    shmdt(cons_count);
    shmdt(sem_buff);
    shmdt(sem_empty);
    shmdt(sem_filled);
    shmdt(sem_prod_count);
    shmdt(sem_cons_count);

    shmctl(strip_shmid, IPC_RMID, NULL);
    shmctl(INF_shmid, IPC_RMID, NULL);
    shmctl(shm_total_height_id, IPC_RMID, NULL);
    shmctl(shm_width_id, IPC_RMID, NULL);
    shmctl(shm_prod_cons_id, IPC_RMID, NULL);
    shmctl(shm_cons_count_id, IPC_RMID, NULL);
    shmctl(sem_buff_shmid, IPC_RMID, NULL);
    shmctl(sem_empty_shmid, IPC_RMID, NULL);
    shmctl(sem_filled_shmid, IPC_RMID, NULL);
    shmctl(sem_prod_count_shmid, IPC_RMID, NULL);
    shmctl(sem_cons_count_shmid, IPC_RMID, NULL);

    return 0;
}
