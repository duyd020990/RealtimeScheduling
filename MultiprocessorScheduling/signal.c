#include "signal.h"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>


void signal_init()
{
    struct sigaction sa;

    memset(&sa,0,sizeof(struct sigaction));
    
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = segfault_handler;
    sa.sa_flags     = SA_SIGINFO;

    sigaction(SIGSEGV,&sa,NULL);

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