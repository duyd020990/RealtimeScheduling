#ifndef HEDF_H
#define HEDF_H

#include "Schedule.h"

#define HEDF

typedef struct tcb_cntnr
{
    unsigned long     et;
    unsigned long     start_time;
    TCB*              tcb;
    struct tcb_cntnr* next;
}TCB_CNTNR;

typedef struct
{
    TCB_CNTNR* head;
    TCB_CNTNR* tail;
}CPU_ASSIGNMENT;

void HEDF_scheduling_init();

void HEDF_scheduling_exit();

void HEDF_scheduling_update();

void HEDF_scheduling();

int HEDF_insert_OK(TCB*,TCB*);

void HEDF_reorganize_function(TCB**);

#endif