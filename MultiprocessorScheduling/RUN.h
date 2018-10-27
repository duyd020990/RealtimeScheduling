#ifndef RUN_H
#define RUN_H

#define RUN

#include "Schedule.h"

#define DUAL  0
#define PACK  1
#define LEAF  2
#define DUMMY 3

// This is a block,I use to represent a server, what ever it's a dual server(DUAL) or a server pack(PACK) or just a server(LEAF)
typedef struct scb
{
    int                is_pack;      //Determin this is a pack or a dual server.
    double             ultilization; //The total ultilization of this pack or dual server.
    unsigned long long a_dl;         //The deadline of this pack or dual server
    unsigned long long r_dl;
    unsigned long long req_tim;         //Realeative deadline,will be used when calculate the et
    double             et;
    struct scb*        root;
    void*              leaf;         //For those packed server,this point to a list of sub-server.For a dual server,this point to the another phase of this dual server. 
    struct scb*        next;         //Point to the next SCB block.
}SCB;

// This is a container for a TCb block
typedef struct tcb_cntnr
{
	TCB*              tcb;
	struct tcb_cntnr* next;
}TCB_CNTNR;

//This struct meant to build a server list
typedef struct svr_cntnr
{
    SCB*              scb;
    struct svr_cntnr* next;
}SVR_CNTNR;

void RUN_scheduling_initialize();

void RUN_scheduling_exit();

int RUN_insert_OK(TCB*,TCB*);

void RUN_reorganize_function(TCB**);

void RUN_schedule();

void RUN_scheduling_update();

#endif