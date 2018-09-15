#ifndef RUN_H
#define RUN_H

#define RUN

#include "Schedule.h"

#define DUAL  0
#define PACK  1
#define LEAF  2

// This is a block,I use to represent a server, what ever it's a dual server(DUAL) or a server pack(PACK) or just a server(LEAF)
typedef struct scb
{
    int                is_pack;      //Determin this is a pack or a dual server.
    double             ultilization; //The total ultilization of this pack or dual server.
    unsigned long long deadline;     //The deadline of this pack or dual server.
    unsigned long long et;
    TCB*               tcb;	         //For a leaf SCB in a tree,this point to a TCB correspond to this SCB.
    struct scb*        root;
    struct scb*        leaf;         //For those packed server,this point to a list of sub-server.For a dual server,this point to the another phase of this dual server. 
    struct scb*        next;         //Point to the next SCB block.
}SCB;

// This is a container for a TCb block
typedef struct tcb_cntnr
{
	TCB*              tcb;
	struct tcb_cntnr* next;
}TCB_CNTNR;

void RUN_scheduling_initialize();
void RUN_scheduling_exit();

int RUN_insert_OK(TCB*,TCB*);

void RUN_reorganize_function(TCB**);

void RUN_schedule();


#endif