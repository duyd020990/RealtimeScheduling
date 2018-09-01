#ifndef RUN_H
#define RUN_H

#define RUN

#include "Schedule.h"

#define DUAL  0
#define PACK  1
#define LEAF  2

typedef struct scb
{
    int                is_pack;      //Determin this is a pack or a dual server.
    double             ultilization; //The total ultilization of this pack or dual server.
    unsigned long long deadline;     //The deadline of this pack or dual server.
    TCB*               tcb;	         //For a leaf SCB in a tree,this point to a TCB correspond to this SCB.
    struct scb*        leaf;         //For those packed server,this point to a list of sub-server.For a dual server,this point to the another phase of this dual server. 
    struct scb*        next;         //Point to the next SCB block.
}SCB;


void RUN_scheduling_initialize();

int RUN_insert_OK(TCB*,TCB*);

void RUN_reorganize_function(TCB**);

void RUN_schedule();


#endif