#ifndef SIGNAL_H
#define SIGNAL_H

#define SIGNAL

#include <signal.h>

void signal_init();

void segfault_handler(int signal,siginfo_t* si,void* arg);

#endif