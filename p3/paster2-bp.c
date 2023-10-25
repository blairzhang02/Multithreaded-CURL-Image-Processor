#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h> // shmget, shmat, shmdt, shmctl
#include <sys/wait.h> // wait
#include <unistd.h> // fork
#include <errno.h>    /* for errno                   */
#include "crc.h"      /* for crc()                   */
#include "zutil.h"    /* for mem_def() and mem_inf() */
#include "lab_png.h"  /* simple PNG data structures  */
#include <libgen.h>
#include <sys/queue.h>
#include <curl/curl.h>
#include <sys/types.h>

sem_t X;
sem_t Y;

 unsigned int total_height = 0;
    unsigned int height = 0;
    unsigned int width = 0;



#define IMG_URL "http://ece252-1.uwaterloo.ca:2530/image?img=1&part=1"
#define DUM_URL "https://example.com/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */
#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
    

typedef struct recv_buf2 {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;


size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
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

size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;
 
    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */ 
        /* received data is not 0 terminated, add one byte for terminating 0 */
        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);   
        char *q = realloc(p->buf, new_size);
        if (q == NULL) {
            perror("realloc"); /* out of memory */
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}


int recv_buf_init(RECV_BUF *ptr, size_t max_size)
{
    void *p = NULL;
    
    if (ptr == NULL) {
        return 1;
    }

    p = malloc(max_size);
    if (p == NULL) {
	return 2;
    }
    
    ptr->buf = p;
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

    if (fwrite(in, 1, len, fp) != len) {x
        fprintf(stderr, "write_file: imcomplete write!\n");
        return -3; 
    }
    return fclose(fp);

}
int produce(int id) {
	static int count;
	int oldCount = __sync_fetch_and_add(&count, 1); // atomic increment

    CURL *curl_handle;
    CURLcode res;
    char url[256];
    RECV_BUF recv_buf;
    char fname[256];
    pid_t pid =getpid();
    
    recv_buf_init(&recv_buf, BUF_SIZE);
    
    
    //REQUEST STRIPS, FIX TO REQUEST FROM SERVERS IN ROUND ROBIN
  
    printf("%s: URL is %s\n", argv[0], url);

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
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3); 
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
    } else {
	printf("%lu bytes received in memory %p, seq=%d.\n", \
               recv_buf.size, recv_buf.buf, recv_buf.seq);
    }

    sprintf(fname, "./output_%d_%d.png", recv_buf.seq, pid);
    write_file(fname, recv_buf.buf, recv_buf.size);

    /* cleaning up */
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
    recv_buf_cleanup(&recv_buf);
    return 0;
    

    
    return oldCount;
}

void consume(int id, int number) {
    printf("Consumer %d consumed %d.\n", id, number);


   
   
    data_IHDR_p data_IHDR = malloc(DATA_IHDR_SIZE);
       // printf("fdsf3\n");
    //CHANGE PNGBUFFER TO SHARED MEM
    memcpy(data_IHDR, png_buffer[i].buf + 16, 13);
    // printf("fdsf4\n");
    total_height += ntohl(data_IHDR -> height);
    height = ntohl(data_IHDR -> height);

    width = ntohl(data_IHDR->width);
            // printf("height: %i\n", height);

    //printf("total_height: %i\n", total_height);


    //get idat
    chunk_p idat_chunk_length = malloc(4);
    memcpy(idat_chunk_length, png_buffer[i].buf + 33, 4);
    //printf("fdsf1\n");
    

    chunk_p chunk_IDAT = malloc(12 + ntohl(idat_chunk_length->length));
    memcpy(chunk_IDAT, png_buffer[i].buf +33, 12 + ntohl(idat_chunk_length->length));
    

    U8 *data_buffer = malloc(ntohl(chunk_IDAT->length)); 
    memcpy(data_buffer, png_buffer[i].buf +41,  ntohl(chunk_IDAT->length));       
                    
    
    int inflated_data_length = (height * (width *4 +1));
    
    
    U64 *inflated_data = malloc((height * (width *4 +1)));

    mem_inf(inflated_data, &inflated_data_length, data_buffer, ntohl(chunk_IDAT->length) );
    
    //CHANGE TO SHARED MEM
    //copy to shared mem
    memcpy(inflated_buffer + offset, inflated_data, inflated_data_length);
    
    offset += inflated_data_length;
    //free(inflated_data);
    
    
    free(data_IHDR);
    free(idat_chunk_length);
    free(data_buffer);
    free(chunk_IDAT);
    free(inflated_data);



}


// void* producer(void* arg) {
//     int* id = (int*) arg;

//     for(int i = 0; i < 10; ++i) {
// 		// produce
//         int num = produce(*id); 

// 		// add to bounded buffer
//  		sem_wait(&empty);       
//         pthread_mutex_lock(&mutex);
//         buffer[pIndex] = num;
//         pIndex = ++pIndex % BUFFER_SIZE;
//         pthread_mutex_unlock(&mutex);
//         sem_post(&filled);
//     }

//     pthread_exit(NULL);
// }

// void* consumer(void* arg) {
//     int* id = (int*) arg;

//     for(int i = 0; i < 10; ++i) {
// 		int num;
// 		// get from bounded buffer
//  		sem_wait(&filled);       
//         pthread_mutex_lock(&mutex);
//         num = buffer[cIndex];
//         cIndex = ++cIndex % BUFFER_SIZE;
//         pthread_mutex_unlock(&mutex);
//         sem_post(&empty);

// 		// consume
//         consume(*id, num);
//     }

//     pthread_exit(NULL);
// }

int main( int argc, char** argv ) 
{
    if (argc != 5){
        printf("invalid input");
        return 0;
    }
    int B = atoi(argv[1]);
    int P = atoi(argv[2]);
    int C = atoi(argv[3]);
    int X = atoi(argv[4]);
    int N = atoi(argv[5]);

    int shmid = shmget(IPC_PRIVATE, B, IPC_CREAT | 0600);
    

    












    //DEFLATION + PRODUCTION OF ALL.PNG
    U8 * deflated_data=malloc(2000000);
    unsigned int deflated_data_length = 0;
    
    //CHANGE TO SHARED MEM 
    mem_def(deflated_data, &deflated_data_length, inflated_buffer, 9000000, Z_DEFAULT_COMPRESSION);

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