#define _GNU_SOURCE
/*
 * The code is derived from cURL example and paster.c base code.
 * The cURL example is at URL:
 * https://curl.haxx.se/libcurl/c/getinmemory.html
 * Copyright (C) 1998 - 2018, Daniel Stenberg, <daniel@haxx.se>, et al..
 *
 * The xml example code is 
 * http://www.xmlsoft.org/tutorial/ape.html
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
 * @file main_wirte_read_cb.c
 * @brief cURL write call back to save received data in a user defined memory first
 *        and then write the data to a file for verification purpose.
 *        cURL header call back extracts data sequence number from header if there is a sequence number.
 * @see https://curl.haxx.se/libcurl/c/getinmemory.html
 * @see https://curl.haxx.se/libcurl/using/
 * @see https://ec.haxx.se/callback-write.html
 */ 


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include <search.h>
#include <pthread.h>

#define SEED_URL "http://ece252-1.uwaterloo.ca/lab4/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */

#define CT_PNG  "image/png"
#define CT_HTML "text/html"
#define CT_PNG_LEN  9
#define CT_HTML_LEN 9
pthread_mutex_t frontier_mutex= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t png_URL_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t hashmap_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;
int wait_thread =1;
int glob_counter = 0;
int collection_index = 0;
struct hsearch_data *visited;
char *key_collection[1000];

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })



typedef struct int_stack
{
    int size;               /* the max capacity of the stack */
    int pos;                /* position of last item pushed onto the stack */
    char items[5000][256];             /* stack of stored integers */
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
    return (sizeof(ISTACK) + sizeof(char) * size);
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
    strcpy(p->items,(char *) ((char *)p + sizeof(ISTACK)) ); 
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
        strcpy(pstack->items,(char *) (p + sizeof(ISTACK)) );
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

int push(ISTACK *p, char *item)
{
    //printf("ldfsfdsf: %s\n", item);
    if ( p == NULL ) {
        
        return -1;
    }

    if ( !is_full(p) ) {
        
        ++(p->pos);
        strcpy(p->items[p->pos], item);
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

int pop(ISTACK *p, char *p_item)
{
    if ( p == NULL ) {
        return -1;
    }

    if ( !is_empty(p) ) {
        //printf("sdfds: %s\n", p->items[p->pos]);
        strcpy(p_item, p->items[p->pos]);
        (p->pos)--;
        return 0;
    } else {
        return 1;
    }
}

ISTACK *frontier;
ISTACK *png_URL;

typedef struct recv_buf2 {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;


htmlDocPtr mem_getdoc(char *buf, int size, const char *url);
xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath);
int find_http(char *fname, int size, int follow_relative_links, const char *base_url);
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
void cleanup(CURL *curl, RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);
CURL *easy_handle_init(RECV_BUF *ptr, const char *url);
int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf);


htmlDocPtr mem_getdoc(char *buf, int size, const char *url)
{
    int opts = HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR | \
               HTML_PARSE_NOWARNING | HTML_PARSE_NONET;
    htmlDocPtr doc = htmlReadMemory(buf, size, url, NULL, opts);
    
    if ( doc == NULL ) {
        //fprintf(stderr, "Document not parsed successfully.\n");
        return NULL;
    }
    return doc;
}

xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath)
{
	
    xmlXPathContextPtr context;
    xmlXPathObjectPtr result;

    context = xmlXPathNewContext(doc);
    if (context == NULL) {
       // printf("Error in xmlXPathNewContext\n");
        return NULL;
    }
    result = xmlXPathEvalExpression(xpath, context);
    xmlXPathFreeContext(context);
    if (result == NULL) {
       // printf("Error in xmlXPathEvalExpression\n");
        return NULL;
    }
    if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
        xmlXPathFreeObject(result);
       // printf("No result\n");
        return NULL;
    }
    return result;
}

int find_http(char *buf, int size, int follow_relative_links, const char *base_url)
{
     ENTRY e, *ep;
    int i;
    htmlDocPtr doc;
    xmlChar *xpath = (xmlChar*) "//a/@href";
    xmlNodeSetPtr nodeset;
    xmlXPathObjectPtr result;
    xmlChar *href;
		
    if (buf == NULL) {
        return 1;
    }

    doc = mem_getdoc(buf, size, base_url);
    result = getnodeset (doc, xpath);
    if (result) {
        nodeset = result->nodesetval;
        for (i=0; i < nodeset->nodeNr; i++) {
            href = xmlNodeListGetString(doc, nodeset->nodeTab[i]->xmlChildrenNode, 1);
            if ( follow_relative_links ) {
                xmlChar *old = href;
                href = xmlBuildURI(href, (xmlChar *) base_url);
                xmlFree(old);
            }
            if ( href != NULL && !strncmp((const char *)href, "http", 4) ) {
               
                                    e.key = (char*) href;


                pthread_mutex_lock(&hashmap_mutex);
                 
            
                //
                //strcpy(e.key,(char*) href);
               

                hsearch_r(e, FIND, &ep, visited);
               
                if (ep != 0){
                    pthread_mutex_unlock(&hashmap_mutex);

                }else{
                    //printf("e.key: %s\n", e.key);
                    e.key = malloc(256);
                     strcpy(e.key, (char *)href);
                    key_collection[collection_index] = e.key;
                    collection_index++;
                                    e.data = NULL;

                    hsearch_r(e, ENTER, &ep, visited);
                    pthread_mutex_unlock(&hashmap_mutex);

                    pthread_mutex_lock(&frontier_mutex);
                    //printf("href: %s\n", href);
                    push(frontier,  (char*) href);
                    //printf("\npushed: %s\n", (char*) href);
                    //printf("pushed and %d\n",is_empty(frontier));
                    pthread_mutex_unlock(&frontier_mutex);

                    
                    //printf("signaled\n");
                
                    pthread_cond_signal(&cond_var);
                    
        
                    //free(e.key);
                }
                //free(e.key);
                
        }
            xmlFree(href);
            
        }
        xmlXPathFreeObject (result);
    }
    xmlFreeDoc(doc);
    //xmlCleanupParser();
    return 0;
}
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

#ifdef DEBUG1_
   // printf("%s", p_recv);
#endif /* DEBUG1_ */
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
    ptr->seq = -1;              /* valid seq should be positive */
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

void cleanup(CURL *curl, RECV_BUF *ptr)
{
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        recv_buf_cleanup(ptr);
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

/**
 * @brief create a curl easy handle and set the options.
 * @param RECV_BUF *ptr points to user data needed by the curl write call back function
 * @param const char *url is the target url to fetch resoruce
 * @return a valid CURL * handle upon sucess; NULL otherwise
 * Note: the caller is responsbile for cleaning the returned curl handle
 */

CURL *easy_handle_init(RECV_BUF *ptr, const char *url)
{
    CURL *curl_handle = NULL;

    if ( ptr == NULL || url == NULL) {
        return NULL;
    }

    /* init user defined call back function buffer */
    if ( recv_buf_init(ptr, BUF_SIZE) != 0 ) {
        return NULL;
    }
    /* init a curl session */
    curl_handle = curl_easy_init();

    if (curl_handle == NULL) {
        fprintf(stderr, "curl_easy_init: returned NULL\n");
        return NULL;
    }

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    /* register write call back function to process received data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3); 
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)ptr);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl); 
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)ptr);

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "ece252 lab4 crawler");

    /* follow HTTP 3XX redirects */
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    /* continue to send authentication credentials when following locations */
    curl_easy_setopt(curl_handle, CURLOPT_UNRESTRICTED_AUTH, 1L);
    /* max numbre of redirects to follow sets to 5 */
    curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 5L);
    /* supports all built-in encodings */ 
    curl_easy_setopt(curl_handle, CURLOPT_ACCEPT_ENCODING, "");

    /* Max time in seconds that the connection phase to the server to take */
    //curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 5L);
    /* Max time in seconds that libcurl transfer operation is allowed to take */
    //curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10L);
    /* Time out for Expect: 100-continue response in milliseconds */
    //curl_easy_setopt(curl_handle, CURLOPT_EXPECT_100_TIMEOUT_MS, 0L);

    /* Enable the cookie engine without reading any initial cookies */
    curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, "");
    /* allow whatever auth the proxy speaks */
    curl_easy_setopt(curl_handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
    /* allow whatever auth the server speaks */
    curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);

    return curl_handle;
}

int process_html(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    char fname[256];
    int follow_relative_link = 1;
    char *url = NULL; 
    pid_t pid =getpid();

    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &url);
    find_http(p_recv_buf->buf, p_recv_buf->size, follow_relative_link, url); 
    sprintf(fname, "./output_%d.html", pid);
    return 0; //write_file(fname, p_recv_buf->buf, p_recv_buf->size);
}
int m;

int process_png(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    //printf("HELLO\n");
    pid_t pid =getpid();
    char fname[256];
    char *eurl = NULL;          /* effective URL */
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &eurl);
    //("process_png eurl %s\n",eurl);
    if ( eurl != NULL) {
        
           //printf("eurl: %s\n", eurl);
    unsigned char png_buf[8];
    memcpy(png_buf, p_recv_buf->buf, 8);
     if ((png_buf[1] == 0x50 && png_buf[2] == 0x4E && png_buf[3] == 0x47)){
        //printf("%s: Not a PNG file\n");
       // printf("png eurl: %s\n", eurl);
        pthread_mutex_lock(&png_URL_mutex);
        push(png_URL,  eurl);
        __sync_fetch_and_add(&glob_counter,1);
        pthread_mutex_unlock(&png_URL_mutex);
        
    }else{
        return 0;
        
    }

    }



    sprintf(fname, "./output_%d_%d.png", p_recv_buf->seq, pid);
    return 0; //write_file(fname, p_recv_buf->buf, p_recv_buf->size);
}
/**
 * @brief process teh download data by curl
 * @param CURL *curl_handle is the curl handler
 * @param RECV_BUF p_recv_buf contains the received data. 
 * @return 0 on success; non-zero otherwise
 */

int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    CURLcode res;
    char fname[256];
    pid_t pid =getpid();
    long response_code;

    res = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    if ( res == CURLE_OK ) {
	   // printf("Response code: %ld\n", response_code);
    }

    if ( response_code >= 400 ) { 
    	//fprintf(stderr, "Error.\n");
        //fd
        return 1;
    }

    char *ct = NULL;
    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
    if ( res == CURLE_OK && ct != NULL ) {
    	//printf("Content-Type: %s, len=%ld\n", ct, strlen(ct));
    } else {
        fprintf(stderr, "Failed obtain Content-Type\n");
        return 2;
    }

    if ( strstr(ct, CT_HTML) ) {
        //printf("HTMLLL\n");
        return process_html(curl_handle, p_recv_buf);
        
    } else if ( strstr(ct, CT_PNG) ) {
        //printf("hhhhh\n");
        return process_png(curl_handle, p_recv_buf);

        //add to png stack
    } else {
        sprintf(fname, "./output_%d", pid);
    }

    return 0;// write_file(fname, p_recv_buf->buf, p_recv_buf->size);
}
typedef struct thread_args{
   int thread_count;
   int img_limit;
   struct hsearch_data* hashmap;

} pthread_args;

void *do_work(void * args){
     pthread_args *thread_arguments = args;

     ENTRY e, *ep;

  
    while (1){

        //("hhh: %i\n", is_empty(frontier));
        CURL *curl_handle;
        char url[256];
        CURLcode res;
        RECV_BUF recv_buf;
        
        //printf("top\n");



        pthread_mutex_lock(&frontier_mutex);
     
        //printf("top free and %d\n",is_empty(frontier));
        if (is_empty(frontier)){
           //mak printf("empty and %s\n");
            __sync_fetch_and_add(&wait_thread, 1);
            //printf("\nwaiting\n");
            pthread_cond_wait(&cond_var, &frontier_mutex);
            //printf("\nnow waiting\n");
             if( __sync_fetch_and_add(&glob_counter,0) >= m||( (__sync_fetch_and_add(&wait_thread, 0) >=thread_arguments->thread_count) &&  is_empty(frontier))){
                // printf("m value %d and %d and %dn",m,is_empty(frontier) == 1,__sync_fetch_and_sub(&wait_thread, 0));
               // printf("\nbroke now\n");
                pthread_mutex_unlock(&frontier_mutex);
                //pthread_cond_broadcast(&cond_var);
                return 0;
            }
            __sync_fetch_and_sub(&wait_thread, 1);

        }
       // int temp_m_og  =  __sync_fetch_and_sub(&m,0);
        pop(frontier, url);

        pthread_mutex_unlock(&frontier_mutex);

        


    

        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_handle = easy_handle_init(&recv_buf, url);

        if ( curl_handle == NULL ) {
            fprintf(stderr, "Curl initialization failed. Exiting...\n");
            curl_global_cleanup();
            abort();
        }
        /* get it! */
        res = curl_easy_perform(curl_handle);

        if( res != CURLE_OK) {
            
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            cleanup(curl_handle, &recv_buf);
            continue;
        } else {
       // printf("%lu bytes received in memory %p, seq=%d.\n", \ recv_buf.size, recv_buf.buf, recv_buf.seq);
        }

        /* process the download data */
      //  printf("process begin");
        process_data(curl_handle, &recv_buf);
        //printf("process end");
        //int temp_m  =  __sync_fetch_and_sub(&m,0);
        //printf("frontier  lock mutex\n");

        pthread_mutex_lock(&frontier_mutex);
        //printf("waiting thread %d and %d\n",wait_thread, is_empty(frontier));
        if(  __sync_fetch_and_sub(&glob_counter,0) >= m||  ((__sync_fetch_and_add(&wait_thread, 0) >= thread_arguments->thread_count )  && is_empty(frontier))){
        //    printf("m value %d and %d and %dn",m,is_empty(frontier),__sync_fetch_and_sub(&wait_thread, 0));
          
          
            
            //printf("broke now \n");
            pthread_mutex_unlock(&frontier_mutex);
                        pthread_cond_broadcast(&cond_var);

            cleanup(curl_handle, &recv_buf);
            break;
        }
        pthread_mutex_unlock(&frontier_mutex);
       // printf("done work\n");
        
    

        /* cleaning up */
      
        cleanup(curl_handle, &recv_buf);
    }
    pthread_exit(0);

   // printf("BROKE THREAD\n");

}



int main( int argc, char** argv ) 
{
    char url[256];
    int c;
    int t = 1;
    m = 5;
    char* v = NULL;
    char *str = "option requires an argument";
  
    
    while ((c = getopt(argc, argv, "t:m:v:")) != -1)
    {
        switch (c)
        {
        case 't':
            t = strtoul(optarg, NULL, 10);
            // printf("option -t specifies a value of %d.\n", t);
            if (t <= 0)
            {
                fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
                return -1;
            }

            break;

        case 'm':
            m = strtoul(optarg, NULL, 10);
            // printf("option -m specifies a value of %d.\n", m);
            if (m <= 0)
            {
                fprintf(stderr, "%s: %s > 0 -- 'm'\n", argv[0], str);
                return -1;
            }
            break;
        case 'v':

            v = optarg;
            // printf("option -v specifies a value of %s.\n", v);
            break;
        default:
            return -1;
        }
    }
    strcpy(url, argv[optind]);




    

    //printf("m: %i\n", m);
    // printf("t: %i\n", t);
    // printf("v: %i\n", v);

    strcpy(url, argv[optind]);

    png_URL = create_stack(10000);
    
    init_shm_stack(png_URL, 10000);

    frontier = create_stack(1000);

    init_shm_stack(frontier, 1000);




    //  pthread_mutex_init(&frontier_mutex, NULL);
    //  pthread_mutex_init(&png_URL_mutex, NULL);
    //  pthread_mutex_init(&hashmap_mutex, NULL);
    //  pthread_cond_init(&cond_var, NULL);

    visited = calloc(1, sizeof(struct hsearch_data)*100000);

    hcreate_r(100000, visited);

    ENTRY e, *ep;


    push(frontier, url);

    e.data = NULL;
    e.key = malloc(256);
    strcpy(e.key,url);

    hsearch_r(e, ENTER, &ep, visited);



    pthread_t *p_tids = malloc(sizeof(pthread_t) * t);

      double times[2];
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;

   // pthread_args *thread_arguments = args;
   //printf("bruh: %i\n", t);
    pthread_args array_of_args[t];
    for (int i = 0; i < t; i++){
        array_of_args[i].thread_count = t;
        array_of_args[i].img_limit = m;
        array_of_args[i].hashmap = visited;
        pthread_create(p_tids + i, NULL, do_work, array_of_args + i);

    }

    for (int i =0 ; i < t; i++){
        pthread_join(p_tids[i], NULL);
    }
    free(p_tids);

    //printf("no\n");


    int png_count = 0;
    if (v != 0){
        FILE * fp = fopen(v, "wb+");
        while(is_empty(png_URL)==0 && png_count < m ){
            char s[256];
            pop(png_URL, s);
            fprintf(fp,"%s\n",s);
            png_count++;
        }
    }
  
    if (gettimeofday(&tv, NULL) != 0) {
            perror("gettimeofday");
            abort();
        }
    times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;

    // for (int i =0; i<1000; i++){
    //     free(key_collection[i]);
    // }

    printf("findpng2 execution time: %.6lf seconds\n",  times[1] - times[0]);
    destroy_stack(frontier);
    destroy_stack(png_URL);
    hdestroy_r(visited);
    free(visited);
  
    pthread_mutex_destroy(&frontier_mutex);
    pthread_mutex_destroy(&png_URL_mutex);
    pthread_cond_destroy(&cond_var);

    xmlCleanupParser();


    //printf("empty: %i\n", is_empty(png_URL));
    // while (is_empty(png_URL) == 0){
    //     char s[256];
    //     pop(png_URL, s);
    //     printf("the png eurl: %s\n", s);
    // }




    // ENTRY e, *ep;
        
   
    // printf("URL is %s\n", url);

    // while ((is_empty(frontier) == 0  ) && m > 0){
    //     printf("hhh: %i\n", is_empty(frontier));
    //     printf("m: %i\n", m);
    //     CURL *curl_handle;
    //     CURLcode res;
    //     RECV_BUF recv_buf;
        
    //     pop(frontier, url);
    //     printf("the url: %s\n", url);

    //     e.data = NULL;
    //     e.key = malloc(256);
    //     strcpy(e.key,url);

    //     hsearch_r(e, FIND, &ep, visited);
    //     if (ep != NULL){
    //         printf("fasdsfa\n");
    //         continue;
    //     }
    //     hsearch_r(e, ENTER, &ep, visited);


    //     curl_global_init(CURL_GLOBAL_DEFAULT);
    //     curl_handle = easy_handle_init(&recv_buf, url);

    //     if ( curl_handle == NULL ) {
    //         fprintf(stderr, "Curl initialization failed. Exiting...\n");
    //         curl_global_cleanup();
    //         abort();
    //     }
    //     /* get it! */
    //     res = curl_easy_perform(curl_handle);

    //     if( res != CURLE_OK) {
    //         fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    //         cleanup(curl_handle, &recv_buf);
    //         continue;
    //     } else {
    //     printf("%lu bytes received in memory %p, seq=%d.\n", \
    //             recv_buf.size, recv_buf.buf, recv_buf.seq);
    //     }

    //     /* process the download data */
    //     process_data(curl_handle, &recv_buf);

    //     // while (is_empty(frontier) != 0){
    //     //   char s[256];
    //     //     pop(frontier, s);
    //     //      printf("the href: %s\n", s);
    //     // }
    //     //while (is_empty(png_URL) != 0){
    //         // pop(png_URL, s);
    //         //  printf("the png href: %s\n", s);
    // // }
    

    //     /* cleaning up */
      
    //     cleanup(curl_handle, &recv_buf);
    // }

    
    
    return 0;
}
