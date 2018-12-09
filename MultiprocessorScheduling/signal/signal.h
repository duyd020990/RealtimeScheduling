#ifndef SIGNAL_H
#define SIGNAL_H

#define SIGNAL

#include <signal.h>

#define SIG_SHOULD_NOT_HAPPENED SIGUSR1 

void signal_init();

void segfault_handler(int signal,siginfo_t* si,void* arg);

void should_not_happened_handler(int signal,siginfo_t* si,void* arg);

void intrp_handler(int signal,siginfo_t* si,void* arg);

#endif