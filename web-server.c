#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h> /* SIGTERM signal handling  */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "web-util.h"
#define __USE_GNU 
#include <pthread.h>     /* pthread functions and data structures     */

/* global mutex for our program. assignment initializes it. */
/* note that we use a RECURSIVE mutex, since a handler      */
/* thread might try to lock it twice consecutively.         */
pthread_mutex_t request_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/* global condition variable for our program. assignment initializes it. */
pthread_cond_t  got_request   = PTHREAD_COND_INITIALIZER;
/*pthread_cond_t buff_available = PTHREAD_COND_INITIALIZER;*/
int num_requests = 0;	/* number of pending requests, initially none */
int NUM_HANDLER_THREADS = 0; /*number of working threads*/
int BUFF_SIZE = 0;  /*buffer size*/
int OVRLD= 0;  /* overload method*/

volatile sig_atomic_t done = 0; /*set to 1 when a SIGTERM SIGNAL COME*/

struct request {
    int number;		    /* number of the request                  */
	int hit;
    struct request* next;   /* pointer to next request, NULL if none. */
};

struct request* requests = NULL;     /* head of linked list of requests. */
struct request* last_request = NULL; /* pointer to last request.         */

/*
 * function add_request(): add a request to the requests list
 * algorithm: creates a request structure, adds to the list, and
 *            increases number of pending requests by one.
 * input:     request number, linked list mutex.
 * output:    none.
 */

 
 void
add_request(int request_num,int hit,
	    pthread_mutex_t* p_mutex,
	    pthread_cond_t*  p_cond_var)
{
    int rc;	                    /* return code of pthreads functions.  */
    struct request* a_request;      /* pointer to newly added request.     */

    /* create structure with new request */
    a_request = (struct request*)malloc(sizeof(struct request));
    if (!a_request) { /* malloc failed?? */
	fprintf(stderr, "add_request: out of memory\n");
	exit(1);
    }
    a_request->number = request_num;
	a_request->hit = hit;
    a_request->next = NULL;

    /* lock the mutex, to assure exclusive access to the list */
    rc = pthread_mutex_lock(p_mutex);

    /* add new request to the end of the list, updating list */
    /* pointers as required */
    if (num_requests == 0) { /* special case - list is empty */
	requests = a_request;
	last_request = a_request;
    }
    else {
	last_request->next = a_request;
	last_request = a_request;
    }

    /* increase total number of pending requests by one. */
	
    num_requests++;
	 printf("add_request: requests now '%d'\n", request_num);
#ifdef DEBUG
    printf("add_request: added request with id '%d'\n", a_request->number);
    fflush(stdout);
#endif /* DEBUG */

	/*logs(MSG6,a_request->number,"File descriptor","with hit",a_request->hit);*/

    /* unlock mutex */
    rc = pthread_mutex_unlock(p_mutex);

    /* signal the condition variable - there's a new request to handle */
    rc = pthread_cond_signal(p_cond_var);
}


/*
 * function get_request(): gets the first pending request from the requests list
 *                         removing it from the list.
 * algorithm: creates a request structure, adds to the list, and
 *            increases number of pending requests by one.
 * input:     request number, linked list mutex.
 * output:    pointer to the removed request, or NULL if none.
 * memory:    the returned request need to be freed by the caller.
 */
struct request*
get_request(pthread_mutex_t* p_mutex)
{
    int rc;	                    /* return code of pthreads functions.  */
    struct request* a_request;      /* pointer to request.                 */

    /* lock the mutex, to assure exclusive access to the list */
    rc = pthread_mutex_lock(p_mutex);

    if (num_requests > 0) {
	a_request = requests;
	requests = a_request->next;
	if (requests == NULL) { /* this was the last request on the list */
	    last_request = NULL;
	}
	/* decrease the total number of pending requests */
	num_requests--;
    }
    else { /* requests list is empty */
	a_request = NULL;
    }

    /* unlock mutex */
    rc = pthread_mutex_unlock(p_mutex);

    /* return the request to the caller. */
    return a_request;
}

void
handle_request(struct request* a_request, int thread_id)
{
    if (a_request) {
	serve(a_request->number,a_request->hit);           // serve and return
    (void)close(a_request->number);         // close read-write descriptor
	/*logs(MSG6,thread_id,"I am thread","serve file descriptor:",a_request->number);*/
	}
	
}


/*
 * function handle_requests_loop(): infinite loop of requests handling
 * algorithm: forever, if there are requests to handle, take the first
 *            and handle it. Then wait on the given condition variable,
 *            and when it is signaled, re-do the loop.
 *            increases number of pending requests by one.
 * input:     id of thread, for printing purposes.
 * output:    none.
 */
void*
handle_requests_loop(void* data)
{
    int rc;	                    /* return code of pthreads functions.  */
    struct request* a_request;      /* pointer to a request.               */
    int thread_id = *((int*)data);  /* thread identifying number           */

#ifdef DEBUG
    printf("Starting thread '%d'\n", thread_id);
    fflush(stdout);
#endif /* DEBUG */

    /* lock the mutex, to access the requests list exclusively. */
    rc = pthread_mutex_lock(&request_mutex);

#ifdef DEBUG
    printf("thread '%d' after pthread_mutex_lock\n", thread_id);
    fflush(stdout);
#endif /* DEBUG */

    /* do forever.... */
    while (1) {
#ifdef DEBUG
    	printf("thread '%d', num_requests =  %d\n", thread_id, num_requests);
    	fflush(stdout);
#endif /* DEBUG */
	/*if(num_requests<5)
	{
		 printf("Hey Main thread wake up....\n");
		pthread_cond_signal(&buff_available);
	}*/
	if (num_requests > 0) { /* a request is pending */
	    a_request = get_request(&request_mutex);
	    if (a_request) { /* got a request - handle it and free it */
		handle_request(a_request, thread_id);
		free(a_request);
		
	    }
	}
	else {
	    /* wait for a request to arrive. note the mutex will be */
	    /* unlocked here, thus allowing other threads access to */
	    /* requests list.                                       */
#ifdef DEBUG
    	    printf("thread '%d' before pthread_cond_wait\n", thread_id);
    	    fflush(stdout);
#endif /* DEBUG */
	    rc = pthread_cond_wait(&got_request, &request_mutex);
	    /* and after we return from pthread_cond_wait, the mutex  */
	    /* is locked again, so we don't need to lock it ourselves */
#ifdef DEBUG
    	    printf("thread '%d' after pthread_cond_wait\n", thread_id);
    	    fflush(stdout);
#endif /* DEBUG */
	}
    }
}


void term(int signum)
{
    done = 1;
}

int main(int argc, char **argv)
{
    int i, port, listenfd, socketfd, hit;
    socklen_t length;
    static struct sockaddr_in cli_addr;  // static = initialised to zeros
    static struct sockaddr_in serv_addr; // static = initialised to zeros


    /* =======================================================================
     * Check for command-line errors, and print a help message if necessary.
     * =======================================================================
     */
    if( argc < 5  || argc > 6 || !strcmp(argv[1], "-?") ) {
     (void)printf("hint: web-server Port-Number Top-Directory\t\tversion %d\n\n"
    "\tweb-server is a small and very safe mini web server\n"
    "\tweb-server only servers out file/web pages with extensions named below\n"
    "\tand only from the named directory or its sub-directories.\n"
    "\tThere is no fancy features = safe and secure.\n\n"
    "\tExample: web-server 8181 /home/webdir\n\n"
    "\tOnly Supports:", VERSION);

        for(i=0;extensions[i].ext != 0;i++)
            (void)printf(" %s",extensions[i].ext);

     (void)printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
    "\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n");
        exit(0);
    }


    /* =======================================================================
     * Check the legality of the specified top directory of the web site.
     * =======================================================================
     */
    if( !strncmp(argv[2],"/"   ,2 ) || !strncmp(argv[2],"/etc", 5 ) ||
        !strncmp(argv[2],"/bin",5 ) || !strncmp(argv[2],"/lib", 5 ) ||
        !strncmp(argv[2],"/tmp",5 ) || !strncmp(argv[2],"/usr", 5 ) ||
        !strncmp(argv[2],"/dev",5 ) || !strncmp(argv[2],"/sbin",6) ){
        (void)printf("ERROR: Bad top directory %s, see web-server -?\n",argv[2]);
        exit(3);
    }


    /* =======================================================================
     * See if we can change the current directory to the one specified.
     * =======================================================================
     */
    if(chdir(argv[2]) == -1){ 
        (void)printf("ERROR: Can't Change to directory %s\n",argv[2]);
        exit(4);
    }


    /* =======================================================================
     * Detach the process from the controlling terminal, after all input args
     * checked out successfuly; then create a child process to continue as a
     * daemon, whose job is to create a communication socket to listen for
     * http requests.
     * =======================================================================
     *
     * Become deamon + unstopable and no zombies children (= no wait())
     * =======================================================================
     */
    if(fork() != 0)  return 0;         // parent terminates, returns OK to shell
    (void)signal(SIGCLD, SIG_IGN);     // ignore child death
    (void)signal(SIGHUP, SIG_IGN);     // ignore terminal hangups

    for(i=0;i<32;i++) (void)close(i);  // close open files
    (void)setpgrp();                   // break away from process group

    logs(MSG6,atoi(argv[1]),"web-server starting at port","pid",getpid());


    /* =======================================================================
     * Setup the network socket
     * =======================================================================
     */
    if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0) {
        logs(ERROR,1,"system call","socket",0);
        exit(3);
    }

    port = atoi(argv[1]);
    if(port < 0 || port >60000) {
        logs(ERROR,2,"Invalid port number (try 1->60000)",argv[1],0);
        exit(3);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0) {
        logs(ERROR,3,"system call","bind",0);
        exit(3);
    }

    if(listen(listenfd,64) <0) {
        logs(ERROR,4,"system call","listen",0);
        exit(3);
    }
	
	
	NUM_HANDLER_THREADS=atoi(argv[3]);

	BUFF_SIZE=atoi(argv[4]);
	
	if(argc == 6)
		OVRLD=atoi(argv[5]);


    /* SIGTERM handler before go threading*/
	struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term;
    sigaction(SIGTERM, &action, NULL);
	
	/* =======================================================================
     * Loop forever, listening to connection attempts and accept them.
     * For each accepted connection, serve it and return. 
     * =======================================================================
     */
    
    int        thr_id[NUM_HANDLER_THREADS];      /* thread IDs            */
    pthread_t  p_threads[NUM_HANDLER_THREADS]; 
	for (i=0; i<NUM_HANDLER_THREADS; i++) {
	thr_id[i] = i;
	pthread_create(&p_threads[i], NULL, handle_requests_loop, (void*)&thr_id[i]);
    }
	for(hit=1; ;hit++) {
		
        length = sizeof(cli_addr);
		socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length);
        if(socketfd< 0)
            logs(ERROR,5,"system call","accept",0);
        
		
		
		else {
			if(OVRLD==BLCK)
			{
				while(num_requests>=BUFF_SIZE){
				}
				add_request(socketfd,hit,&request_mutex, &got_request);
			}
			else if(OVRLD==DRPT){
				if(num_requests>=BUFF_SIZE)
			{
				(void)close(socketfd);
			}
				else
					add_request(socketfd,hit,&request_mutex, &got_request);
			}
			
			else{
				
				if(num_requests>=BUFF_SIZE){
					
					struct request* head_request;
					head_request=get_request(&request_mutex);
					(void)close(head_request->number);
					
				}
				
				add_request(socketfd,hit,&request_mutex, &got_request);
					
			}
				
				
			}
			
			
        }
		
    }

