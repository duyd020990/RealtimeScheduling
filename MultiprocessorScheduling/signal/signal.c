#include "signal.h"

// From simulator
#include "../Schedule.h"
#include "../log/log.h"

// From Linux or POSIX
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

extern char* current_algorithm;

extern FILE* fp;

void signal_init()
{
    struct sigaction sa_segment_fault;
    struct sigaction sa_should_not_happened;
    struct sigaction sa_intrp;

    memset(&sa_segment_fault      ,0,sizeof(struct sigaction));
    memset(&sa_should_not_happened,0,sizeof(struct sigaction));
    memset(&sa_intrp              ,0,sizeof(struct sigaction));

    sigemptyset(&sa_segment_fault.sa_mask);
    sa_segment_fault.sa_sigaction = segfault_handler;
    sa_segment_fault.sa_flags     = SA_SIGINFO;

    sigemptyset(&sa_should_not_happened.sa_mask);
    sa_should_not_happened.sa_sigaction = should_not_happened_handler;
    sa_should_not_happened.sa_flags     = SA_SIGINFO;

    sigemptyset(&sa_intrp.sa_mask);
    sa_intrp.sa_sigaction = intrp_handler; 
    sa_intrp.sa_flags     = SA_SIGINFO;

    sigaction(SIGSEGV,&sa_segment_fault,NULL);
    sigaction(SIG_SHOULD_NOT_HAPPENED,&sa_should_not_happened,NULL);
    sigaction(SIGINT,&sa_intrp,NULL);
}


void segfault_handler(int signal,siginfo_t* si,void* arg)
{
    fprintf(stderr,"Catch segment fault:\n");
    
    switch(si->si_code)
    {
    	case SEGV_MAPERR:
    		fprintf(stderr,"\tAddress not mapped %p\n",si->si_addr);
    	break;
    	case SEGV_ACCERR:
    		fprintf(stderr,"\tInvalid permission %p\n",si->si_addr);
    	break;
    }

    pause();
}

void should_not_happened_handler(int signal,siginfo_t* si,void* arg)
{
    log_once(NULL,"%s\n%s\n%s\n%s\n","=============================================",
                                      "Something should not happened things happened", 
                                      "         Go check your program               ",
                                      "=============================================");
    log_close();
    pause();
}

void deadline_miss_handler(int signal,siginfo_t* si,void* arg)
{
    if(si == NULL){exit(-1);}

    if(current_algorithm == NULL){exit(-1);}

    log_close();
    pause();
}

void intrp_handler(int signal,siginfo_t* si,void* arg)
{
    fprintf(stderr,"INTERRUPTED\n");

    log_close();

    exit(-1);
    return;
}