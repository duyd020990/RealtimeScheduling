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
    fprintf(stderr,"Catch a segment fault Err\n");
    pause();
}