#include "signal.h"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>


void signal_init()
{
    struct sigaction sa_segment_fault;
    struct sigaction sa_should_not_happened;

    memset(&sa_segment_fault,0,sizeof(struct sigaction));
    memset(&sa_should_not_happened,0,sizeof(struct sigaction));
    
    sigemptyset(&sa_segment_fault.sa_mask);
    sa_segment_fault.sa_sigaction = segfault_handler;
    sa_segment_fault.sa_flags     = SA_SIGINFO;

    sigemptyset(&sa_should_not_happened.sa_mask);
    sa_should_not_happened.sa_sigaction = should_not_happened_handler;
    sa_should_not_happened.sa_flags     = SA_SIGINFO;

    sigaction(SIGSEGV,&sa_segment_fault,NULL);
    sigaction(SIG_SHOULD_NOT_HAPPENED,&sa_should_not_happened,NULL);
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
    fprintf(stderr,"%s\n%s\n%s\n%s\n","=============================================",
                                      "Something should not happened things happened", 
                                      "         Go check your program               ",
                                      "=============================================");
    pause();
}
