#include "RUN.h"

#include "Schedule.h"
#include "EDF.h"

#include <stdio.h>
#include <stdlib.h>

extern int has_new_task; 
extern int has_new_instance;
extern int has_task_finished;

extern unsigned long long period[MAX_TASKS];

extern TCB* _kernel_runtsk[PROCESSOR_NUM];
extern TCB* _kernel_runtsk_pre[PROCESSOR_NUM];
extern TCB* p_ready_queue;
extern TCB* ap_ready_queue;

/* Record the overhead and migration */
extern unsigned long long migration;            // For recording the migration,but not used in uniprocessor case
extern unsigned long long overhead_dl;          // For recording the overhead
extern unsigned long long overhead_dl_max;
extern unsigned long long overhead_dl_total;
extern unsigned long long overhead_alpha;
extern unsigned long long overhead_alpha_max;
extern unsigned long long overhead_alpha_total;

int assignment_history[MAX_TASKS];


SCB* SCB_root;

TCB_CNTNR* execution_queue;

/* 
  This running queue for those Multiprocessor Real-time Scheduling,
  Since the migration only occurse in Multiproccessor platform
 */
TCB *running_queue[PROCESSOR_NUM]; // A running queue for each processor


SCHEDULING_ALGORITHM RUN_sa = {
    .scheduling_initialize = RUN_scheduling_initialize,
    .scheduling_exit       = RUN_scheduling_exit,
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
        printf("%d\t%f\t%llu\t%p\n",i                    ,
                                          scb->ultilization    ,
                                          scb->deadline        ,
                                          scb->tcb            );
    }
}

SCB* TCB_to_SCB(TCB* tcb)
{
    SCB* scb = NULL;

    overhead_dl += COMP;
    if(tcb == NULL){return NULL;}

    overhead_dl += ASSIGN + MEM;
    scb = (SCB*)malloc(sizeof(SCB));
    if(scb == NULL){fprintf(stderr,"Fail to allocate the memory, in RUN TCB_to_SCB\n");return  NULL;}

    overhead_dl += FDIV;
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

    overhead_dl += COMP;
    if(tcb==NULL){return NULL;}

    overhead_dl += ASSIGN+MEM;
    tcb_cntnr = (TCB_CNTNR*)malloc(sizeof(TCB_CNTNR));
    if(tcb_cntnr == NULL){printf("Error with malloc,in TCB_to_TCB_CNTNR\n");return NULL;}

    tcb_cntnr->tcb  = tcb;
    tcb_cntnr->next = NULL;

    return tcb_cntnr;
}

SCB* SCB_to_packed_SCB(SCB* scb)
{
    SCB* packed_scb = NULL;

    overhead_dl += COMP;
    if(scb == NULL){return NULL;}

    overhead_dl += ASSIGN+MEM;
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
    overhead_dl += COMP+COMP;
    if(packed_SCB==NULL || scb==NULL){return 0;}

    //Make sure this is a pack, not a dual server or leaf
    overhead_dl += COMP;
    if(packed_SCB->is_pack != PACK)  {return 0;}

    // Calculate the total ultilization,if this server put in this pack
    overhead_dl += IADD; 
    total_ultilization = (packed_SCB->ultilization)+(scb->ultilization);
    
    //Is it available to put this server into this pack
    overhead_dl += COMP;
    if(total_ultilization <= (double)1)
    {
        //Yes, change the ultilization of this pack
        packed_SCB->ultilization = total_ultilization;
        
        //If the new server has later deadline, then extend the deadline of this pack.
        overhead_dl += COMP;
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

    overhead_dl += COMP+COMP;
    if(rq==NULL || *rq==NULL){return NULL;}

    for(p = *rq;p;p=p->next)
    {
        overhead_dl += IADD+COMP;
        
        scb = TCB_to_SCB(p);
        
        overhead_dl += COMP;
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

    overhead_dl += COMP+COMP;
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

    overhead_dl += COMP+COMP;
    if(SCB_list==NULL || *SCB_list==NULL){return NULL;}

    // If there still are some scb server in the scb_list
    while(*SCB_list)
    {
        overhead_dl += COMP;
        // Retrieve a server from the server list
        scb = SCB_list_retrieve(SCB_list);

        // This is the first element of packed_SCB_list
        overhead_dl += COMP;
        if(packed_SCB_list == NULL)
        {
            packed_SCB_list = SCB_to_packed_SCB(scb);
        }
        else // This is not the first element which would be put in this packed server list 
        {
            // Search for a pack so that we can put this server into this pack
            for(SCB_pack=packed_SCB_list;SCB_pack;SCB_pack=SCB_pack->next)
            {
                overhead_dl += IADD+COMP;
                // Good, there is a pack could contain this server
                overhead_dl += COMP;
                if(SCB_pack_fill(SCB_pack,scb)){break;}
            }  

            // Bad, no any pack could contain this server
            overhead_dl += COMP;
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

    overhead_dl += COMP;
    if(scb == NULL){printf("Error with malloc,in SCB_server_to_dual\n");return NULL;}

    overhead_dl += ASSIGN+MEM;
    SCB_dual = (SCB*)malloc(sizeof(SCB));
    if(SCB_dual==NULL){printf("Error with malloc,in SCB_to_dual\n");return NULL;}

    overhead_dl += FADD;
    SCB_dual->is_pack      = DUAL;
    SCB_dual->deadline     = scb->deadline;
    SCB_dual->ultilization = (double)1-(scb->ultilization);
    SCB_dual->leaf         = scb;
    SCB_dual->next         = NULL;
    SCB_dual->tcb          = NULL;

    return SCB_dual;
}

void execution_queue_insert(TCB_CNTNR* new_tc)
{
    TCB_CNTNR** tc = NULL;

    overhead_dl += COMP;
    if(new_tc == NULL){return;}

    overhead_dl += COMP;
    if(execution_queue == NULL)
    {
        execution_queue = new_tc;
        return;
    }

    for(tc = &execution_queue;*tc;)
    {
        overhead_dl += IADD+COMP;
        overhead_dl += COMP;
        if(EDF_insert_OK(new_tc->tcb,(*tc)->tcb))
        {
            new_tc->next = *tc;
            *tc = new_tc;
            return;
        }
        else
        {
            tc = &((*tc)->next);
        }
    }

    overhead_dl += COMP;
    if(*tc == NULL)
    {
        *tc = new_tc;
    }
}

TCB_CNTNR* execution_queue_retrieve(TCB_CNTNR** TCB_CNTNR_list)
{
    TCB_CNTNR* tc=NULL;
    
    if(TCB_CNTNR_list==NULL || *TCB_CNTNR_list==NULL){return NULL;}
    
    tc              = *TCB_CNTNR_list;
    *TCB_CNTNR_list = tc->next;
    
    return tc;
}

void execution_queue_destory(TCB_CNTNR** TCB_CNTNR_queue)
{
    TCB_CNTNR* tc = NULL;
    if(TCB_CNTNR_queue==NULL || *TCB_CNTNR_queue==NULL){return;}
    
    while((tc=execution_queue_retrieve(TCB_CNTNR_queue))!=NULL){free(tc);}
}

SCB* SCB_dual_list_build(SCB** packed_SCB_list)
{
    SCB* packed_SCB       = NULL;
    SCB* dual_server      = NULL;
    SCB* dual_server_list = NULL;

    overhead_dl += COMP+COMP;
    if(packed_SCB_list==NULL || *packed_SCB_list==NULL){return NULL;}

    while(*packed_SCB_list)
    {
        overhead_dl += COMP;
        packed_SCB = SCB_list_retrieve(packed_SCB_list);
        dual_server = SCB_to_dual(packed_SCB);

        overhead_dl += COMP;
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

    overhead_dl += COMP+COMP+COMP;
    if(SCB_root==NULL || SCB_list==NULL || *SCB_list==NULL){return;}

    for(scb = SCB_list;*scb;)
    {
        overhead_dl += IADD+COMP;
        overhead_dl += COMP;
        if((*scb)->ultilization == (double)1)
        {
            SCB_tmp = *scb;
            *scb = ((*scb)->next);
            SCB_tmp->next = NULL;

            overhead_dl += COMP;
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

    overhead_dl += COMP+COMP;
    if(rq==NULL || *rq==NULL){return NULL;}

    SCB_server_list = SCB_list_build(rq);

    while(SCB_server_list && SCB_server_list->next!=NULL)
    {
        overhead_dl += COMP+COMP;

        SCB_packed_server_list = SCB_list_pack(&SCB_server_list);

        RUN_proper_server_remove(&SCB_reduction_root,&SCB_packed_server_list);

        SCB_dual_server_list = SCB_dual_list_build(&SCB_packed_server_list);

        SCB_server_list = SCB_dual_server_list;
    }

    overhead_dl += COMP;
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

void SCB_reduction_tree_destory(SCB** SCB_root)
{
    SCB* scb = NULL;
    TCB* tcb = NULL;

    overhead_dl += COMP+COMP;
    if(SCB_root==NULL || *SCB_root==NULL){return;}
    
    scb = *SCB_root;

    overhead_dl += COMP;
    switch(scb->is_pack)
    {
        case LEAF:
            SCB_reduction_tree_destory(&(scb->next));
            tcb = scb->tcb;
            if(tcb!=NULL)
            {
                tcb->something_else = NULL;
                free(scb);
            }
        break;

        default:
            SCB_reduction_tree_destory(&(scb->next));
            SCB_reduction_tree_destory(&(scb->leaf));
            free(scb);
            *SCB_root = NULL;
        break;
    }
}

void RUN_reduction_tree_unpack_by_root(SCB** SCB_root,int selected)
{
    SCB* scb              = NULL;
    SCB* SCB_tmp          = NULL;
    SCB* earliest_ddl_SCB = NULL;
    TCB_CNTNR* tcb_cntnr  = NULL;

    overhead_dl += COMP+COMP;
    if(SCB_root==NULL || *SCB_root==NULL){return;}

    scb = *SCB_root;

    overhead_dl += COMP;
    switch(scb->is_pack)
    {
        case LEAF:
            if(!selected){return;}
            
            tcb_cntnr = TCB_to_TCB_CNTNR(scb->tcb);
            
            execution_queue_insert(tcb_cntnr);
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
                    RUN_reduction_tree_unpack_by_root(&(SCB_tmp),0);
                }
            }
            RUN_reduction_tree_unpack_by_root(&earliest_ddl_SCB,1);
        break;

        case DUAL: 
            RUN_reduction_tree_unpack_by_root(&(scb->leaf),!selected);
        break;
    }
}

void RUN_reduction_tree_unpack(SCB** SCB_root)
{
    SCB* scb     = NULL;
    SCB* SCB_min = NULL;

    overhead_dl += COMP+COMP;
    if(SCB_root==NULL || *SCB_root==NULL){return;}

    SCB_min = *SCB_root;

    for(scb=*SCB_root;scb;scb=scb->next)
    {
        overhead_dl += IADD+COMP;
        overhead_dl += COMP;
        if(scb->deadline < SCB_min->deadline){SCB_min = scb;}
        else
        {
            RUN_reduction_tree_unpack_by_root(&scb,0);
        }
    }
    RUN_reduction_tree_unpack_by_root(&SCB_min,1);

}

void RUN_scheduling_initialize()
{
    int i;
    SCB_root        = NULL;
    execution_queue = NULL;

    for(i=0;i<MAX_TASKS;i++){assignment_history[i]=-1;}
}

int RUN_insert_OK(TCB* t1,TCB* t2)
{
    return EDF_insert_OK(t1,t2);
}

void RUN_reorganize_function(TCB** rq)
{
    SCB* SCB_reduction_tree_root = NULL;

    overhead_dl += COMP+COMP;
    if(rq==NULL || *rq==NULL){return;}

    overhead_dl += COMP;
    if(has_new_task || has_new_instance || has_task_finished)
    {
        has_new_task = has_new_instance = has_task_finished = 0;

        SCB_reduction_tree_root = SCB_reduction_tree_build(rq);

        RUN_reduction_tree_unpack(&SCB_reduction_tree_root);

        SCB_reduction_tree_destory(&SCB_reduction_tree_root);
    }
}


int check_assignment_history(TCB* tcb)
{
    return assignment_history[tcb->tid];
}

int processor_assignment(TCB* tcb)
{
    int i;
    int processor_id = -1;
    
    if(tcb == NULL){return -1;}
    
    if((processor_id=check_assignment_history(tcb)) == -1)
    {
        for(i=0;i<PROCESSOR_NUM;i++)
        {
            if(_kernel_runtsk[i]==NULL){break;}
        }
        if(i<PROCESSOR_NUM)
        {
            processor_id = i;
        }
    }
    
    return processor_id;
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
    int i,processor_id;
    TCB_CNTNR* tc = NULL;

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        _kernel_runtsk[i] = NULL;
    }


    for(i=0;i<PROCESSOR_NUM;i++)
    {
        tc = execution_queue_retrieve(&execution_queue);
        if(tc == NULL){printf("tc is NULL,in RUN schedule\n");break;}
        if(tc->tcb == NULL){printf("tc->tcb is NULL,in RUN schedule\n");break;}
        
        processor_id = processor_assignment(tc->tcb);
        if(processor_id == -1){printf("processor_id is -1,in RUN schedule\n");break;}
        
        _kernel_runtsk[processor_id] = tc->tcb;
        assignment_history[tc->tcb->tid] = processor_id;
    }

    execution_queue_destory(&execution_queue);

#ifdef DEBUG
    for(i=0;i<PROCESSOR_NUM;i++)
    {
        printf("%p\t%d\n",_kernel_runtsk[i],_kernel_runtsk[i]->tid);
    }
#endif

}

void RUN_scheduling_exit(){}