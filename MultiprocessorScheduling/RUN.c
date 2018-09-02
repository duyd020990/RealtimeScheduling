#include "RUN.h"

#include "Schedule.h"
#include "EDF.h"

#include <stdio.h>
#include <stdlib.h>

extern int has_new_task; 
extern int has_new_instance;

extern unsigned long long period[MAX_TASKS];

extern TCB* _kernel_runtsk[PROCESSOR_NUM];
extern TCB* _kernel_runtsk_pre[PROCESSOR_NUM];
extern TCB* p_ready_queue;
extern TCB* ap_ready_queue;

SCB* SCB_root;

TCB_CNTNR* execution_queue;

/* 
  This running queue for those Multiprocessor Real-time Scheduling,
  Since the migration only occurse in Multiproccessor platform
 */
TCB *running_queue[PROCESSOR_NUM]; // A running queue for each processor


SCHEDULING_ALGORITHM RUN_sa = {
    .scheduling_initialize = RUN_scheduling_initialize,
    .scheduling            = RUN_schedule,
    .insert_OK             = RUN_insert_OK,
    .reorganize_function   = RUN_reorganize_function
};

/*

*/
void SCB_list_print(SCB** scb_list)
{
    int i;
    SCB* scb = NULL;

    if(scb_list==NULL || *scb_list==NULL)
    {
        printf("scb_list is NULL, in SCB_queue_print\n");
        return;
    }

    for(scb = *scb_list,i=0;scb;scb = scb->next,i++)
    {
        printf("%d\t%f\t%llu\t%llu\t%p\n",i                    ,
                                          scb->ultilization    ,
                                          scb->deadline        ,
                                          period[scb->tcb->tid],
                                          scb->tcb            );
    }
}

SCB* TCB_to_SCB(TCB* tcb)
{
    SCB* scb = NULL;

    if(tcb == NULL){return NULL;}

    scb = (SCB*)malloc(sizeof(SCB));
    if(scb == NULL){fprintf(stderr,"Fail to allocate the memory, in RUN TCB_to_SCB\n");return  NULL;}

    scb->is_pack          = LEAF;
    scb->ultilization     = ((double)tcb->wcet)/period[tcb->tid];
    scb->deadline         = tcb->a_dl;
    scb->tcb              = tcb;
    scb->leaf = scb->next = NULL;

    tcb->something_else = scb;

    return scb;
}

TCB_CNTNR* TCB_to_TCB_CNTNR(TCB* tcb)
{
    TCB_CNTNR* tcb_cntnr = NULL;

    if(tcb==NULL){return NULL;}

    tcb_cntnr = (TCB_CNTNR*)malloc(sizeof(TCB_CNTNR));
    if(tcb_cntnr == NULL){printf("Error with malloc,in TCB_to_TCB_CNTNR\n");return NULL;}

    tcb_cntnr->tcb  = tcb;
    tcb_cntnr->next = NULL;

    return tcb_cntnr;
}

SCB* SCB_to_packed_SCB(SCB* scb)
{
    SCB* packed_scb = NULL;

    if(scb == NULL){return NULL;}

    packed_scb = (SCB*)malloc(sizeof(SCB));
    if(packed_scb == NULL){printf("Error with malloc,in SCB_to_packed_SCB\n");return NULL;}

    packed_scb->is_pack      = PACK;
    packed_scb->deadline     = scb->deadline;
    packed_scb->ultilization = scb->ultilization;
    packed_scb->leaf         = scb;
    packed_scb->next         = NULL;
    packed_scb->tcb          = NULL;

    return packed_scb;
}

int SCB_pack_fill(SCB* packed_SCB,SCB* scb)
{
    double total_ultilization = (double)0;

    // Parameter check
    if(packed_SCB==NULL || scb==NULL){return 0;}

    //Make sure this is a pack, not a dual server or leaf
    if(packed_SCB->is_pack != PACK)  {return 0;}

    // Calculate the total ultilization,if this server put in this pack 
    total_ultilization = (packed_SCB->ultilization)+(scb->ultilization);
    
    //Is it available to put this server into this pack
    if(total_ultilization <= (double)1)
    {
        //Yes, change the ultilization of this pack
        packed_SCB->ultilization = total_ultilization;
        
        //If the new server has later deadline, then extend the deadline of this pack.
        if(packed_SCB->deadline < scb->deadline){packed_SCB->deadline = scb->deadline;}
        
        //New server is a "leaf" for this pack
        scb->next = packed_SCB->leaf;
        packed_SCB->leaf = scb;

        //If you are here, then you already completed this function
        return 1;
    }

    // Sorry, this server could not put in this server pack
    return 0;
}

SCB* SCB_list_build(TCB** rq)
{
    TCB* p        = NULL;
    SCB* scb      = NULL;
    SCB* scb_list = NULL;

    if(rq==NULL || *rq==NULL){return NULL;}

    for(p = *rq;p;p=p->next)
    {
        scb = TCB_to_SCB(p);
        
        if(scb_list == NULL){scb_list = scb;}
        else
        {
            scb->next = scb_list;
            scb_list  = scb;
        }
    }

    return scb_list;
}


SCB* SCB_list_retrieve(SCB** scb_list)
{
    SCB* scb = NULL;

    if(scb_list==NULL || *scb_list==NULL){return NULL;}

    scb       = *scb_list;
    *scb_list = scb->next;
    scb->next = NULL;

    return scb;
}

SCB* SCB_list_pack(SCB** SCB_list)
{
    SCB* scb             = NULL;
    SCB* SCB_pack        = NULL;
    SCB* packed_SCB_list = NULL;

    if(SCB_list==NULL || *SCB_list==NULL){return NULL;}

    // If there still are some scb server in the scb_list
    while(*SCB_list)
    {
        // Retrieve a server from the server list
        scb = SCB_list_retrieve(SCB_list);

        // This is the first element of packed_SCB_list
        if(packed_SCB_list == NULL)
        {
            packed_SCB_list = SCB_to_packed_SCB(scb);
        }
        else // This is not the first element which would be put in this packed server list 
        {
            // Search for a pack so that we can put this server into this pack
            for(SCB_pack=packed_SCB_list;SCB_pack;SCB_pack=SCB_pack->next)
            {
                // Good, there is a pack could contain this server
                if(SCB_pack_fill(SCB_pack,scb)){break;}
            }  

            // Bad, no any pack could contain this server
            if(SCB_pack==NULL)
            {
                SCB_pack        = SCB_to_packed_SCB(scb);
                // Link this new element into this list
                SCB_pack->next  = packed_SCB_list;
                packed_SCB_list = SCB_pack;
            }
        }
    }
    //OK, let's return the final result
    return packed_SCB_list;
}

SCB* SCB_to_dual(SCB* scb)
{
    SCB* SCB_dual = NULL;

    if(scb == NULL){printf("Error with malloc,in SCB_server_to_dual\n");return NULL;}

    SCB_dual = (SCB*)malloc(sizeof(SCB));
    if(SCB_dual==NULL){printf("Error with malloc,in SCB_to_dual\n");return NULL;}

    SCB_dual->is_pack      = DUAL;
    SCB_dual->deadline     = scb->deadline;
    SCB_dual->ultilization = (double)1-(scb->ultilization);
    SCB_dual->leaf         = scb;
    SCB_dual->next         = NULL;
    SCB_dual->tcb          = NULL;

    return SCB_dual;
}

SCB* SCB_dual_list_build(SCB** packed_SCB_list)
{
    SCB* packed_SCB       = NULL;
    SCB* dual_server      = NULL;
    SCB* dual_server_list = NULL;

    if(packed_SCB_list==NULL || *packed_SCB_list==NULL){return NULL;}

    while(*packed_SCB_list)
    {
        packed_SCB = SCB_list_retrieve(packed_SCB_list);
        dual_server = SCB_to_dual(packed_SCB);

        if(dual_server_list == NULL)
        {
            dual_server_list = dual_server;
        }
        else
        {
            dual_server->next = dual_server_list;
            dual_server_list  = dual_server;
        }
    }

    return dual_server_list;
}

void RUN_proper_server_remove(SCB** SCB_root,SCB** SCB_list)
{
    SCB*  SCB_tmp = NULL;
    SCB** scb = NULL;

    if(SCB_root==NULL || SCB_list==NULL || *SCB_list==NULL){return;}

    for(scb = SCB_list;*scb;)
    {
        if((*scb)->ultilization == (double)1)
        {
            SCB_tmp = *scb;
            *scb = ((*scb)->next);
            SCB_tmp->next = NULL;

            if(*SCB_root == NULL){*SCB_root = SCB_tmp;}
            else
            {
                SCB_tmp->next = *SCB_root;
                *SCB_root = SCB_tmp;
            }
        }
        else
        {
            scb=&((*scb)->next);
        }
    }
}

SCB* SCB_reduction_tree_build(TCB** rq)
{
    SCB* SCB_reduction_root     = NULL;
    SCB* SCB_server_list        = NULL;
    SCB* SCB_packed_server_list = NULL;
    SCB* SCB_dual_server_list   = NULL;

    if(rq==NULL || *rq==NULL){return NULL;}

    SCB_server_list = SCB_list_build(rq);

    while(SCB_server_list && SCB_server_list->next!=NULL)
    {
        SCB_packed_server_list = SCB_list_pack(&SCB_packed_server_list);

        RUN_proper_server_remove(&SCB_reduction_root,&SCB_packed_server_list);

        SCB_dual_server_list = SCB_dual_list_build(&SCB_packed_server_list);

        SCB_server_list = SCB_dual_server_list;
    }

    if(SCB_reduction_root==NULL)
    {
        SCB_reduction_root = SCB_server_list;
    }
    else
    {
        SCB_reduction_root->next = SCB_server_list;
    }

    return SCB_reduction_root;
}


void RUN_reduction_tree_unpack(SCB** SCB_root,int selected)
{
    SCB* scb              = NULL;
    SCB* SCB_tmp          = NULL;
    SCB* earliest_ddl_SCB = NULL;
    TCB_CNTNR* tcb_cntnr  = NULL;

    if(SCB_root==NULL || *SCB_root==NULL){return;}

    scb = *SCB_root;

    switch(scb->is_pack)
    {
        case LEAF:
            if(!selected){return;}
            
            tcb_cntnr = TCB_to_TCB_CNTNR(scb->tcb);
            
            if(execution_queue == NULL){execution_queue = tcb_cntnr;}
            else
            {
                tcb_cntnr->next   = execution_queue;
                execution_queue = tcb_cntnr;
            }
            break;

        case PACK:
            if(!selected){return;}

            earliest_ddl_SCB = scb->leaf;
            for(SCB_tmp = scb->leaf;SCB_tmp;SCB_tmp=SCB_tmp->next)
            {
                if(SCB_tmp->deadline<earliest_ddl_SCB->deadline)
                {
                    earliest_ddl_SCB = SCB_tmp;
                }
                else
                {
                    RUN_reduction_tree_unpack(&(SCB_tmp),0);
                }
            }
            RUN_reduction_tree_unpack(&earliest_ddl_SCB,1);
            break;

        case DUAL: 
            RUN_reduction_tree_unpack(&(scb->leaf),!selected);
            break;
    }
}

void RUN_scheduling_initialize()
{
    SCB_root        = NULL;
    execution_queue = NULL;
}

int RUN_insert_OK(TCB* t1,TCB* t2)
{
    return EDF_insert_OK(t1,t2);
}

void RUN_reorganize_function(TCB** rq)
{
    SCB* SCB_reduction_tree_root = NULL;

    if(rq==NULL || *rq==NULL){return;}

    if(has_new_task)
    {
        has_new_task = 0;

        SCB_reduction_tree_root = SCB_reduction_tree_build(rq);

        printf("%d ",SCB_reduction_tree_root->is_pack);

        // Since in my implementation, the root is a dual server, 
        //so the parameter "selected" is initialize as 0.
        RUN_reduction_tree_unpack(&SCB_reduction_tree_root,0);

    }
    else if(has_new_instance)
    {
        has_new_instance = 0;



    }
}

/*
Architecture of Scheduling function for RUN algorithm

RUNScheduling:
if has new task:
    build reduction tree
    search run
else if has new instance:
    update reduction tree
    search run
run scheduling
*/
void RUN_schedule()
{
    int i;
    for(i=0;i<PROCESSOR_NUM;i++)
    {
        _kernel_runtsk[i] = NULL;
    }



}