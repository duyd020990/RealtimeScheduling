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

#include "RUN.h"

#include "Schedule.h"
#include "log/log.h"
#include "tool/tool.h"
#include "signal/signal.h"

#include "EDF.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <unistd.h>

#define DEBUG

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

extern unsigned long long tick;

int assignment_history[MAX_TASKS];
int has_server_finished;

SCB* SCB_root;

SVR_CNTNR* selected_server_list;
TCB_CNTNR* execution_queue;
TCB_CNTNR* update_tc;
TCB* RUN_ready_queue;

SCHEDULING_ALGORITHM RUN_sa = {
    .name                  = "RUN",
    .scheduling_initialize = RUN_scheduling_initialize,
    .scheduling_exit       = RUN_scheduling_exit,
    .scheduling            = RUN_schedule,
    .insert_OK             = RUN_insert_OK,
    .reorganize_function   = RUN_reorganize_function,
    .scheduling_update     = RUN_scheduling_update,
    .job_delete            = NULL
};

/*************************** FOR DEBUG ***********************************/
#ifdef DEBUG
void SCB_list_print(SCB** scb_list)
{
    int i;
    SCB* scb = NULL;

    if(scb_list==NULL || *scb_list==NULL)
    {
        log_once(NULL,"scb_list is NULL, in SCB_queue_print\n");
        return;
    }

    for(scb = *scb_list,i=0;scb;scb = scb->next,i++)
    {
        log_c("%p\t%d\t%f\t%f\t%llu\t%p\n",scb              ,
                                           i                ,
                                           scb->et          ,
                                           scb->ultilization,
                                           scb->a_dl        ,
                                           scb->leaf        );
    }
}

void SCB_reduction_tree_print(SCB** SCB_node,int level,int index)
{
    SCB*      scb = NULL;
    TCB*      tcb = NULL;

    if(SCB_node==NULL || *SCB_node==NULL){log_c("SCB_node is NULL,in SCB_reduction_tree_print\n");return;}
    
    log_c("level %d-%d:",level,index);

    scb = *SCB_node;
    switch(scb->is_pack)
    {
        case LEAF:
            log_c("LEAF %p\n",scb);
            tcb = (TCB*)(scb->leaf);
            if(tcb == NULL){log_once(NULL,"This tc was exhausted, tid %d\n",tcb->tid);}
            else
            {
                log_c("%p\ttid:%d\ttet:%u\tset:%f\tdl:%llu\n",scb,
                                                              tcb->tid,
                                                              tcb->et,
                                                              scb->et,
                                                              scb->a_dl);         // display job information
            }
            SCB_reduction_tree_print(&(scb->next),level,index+1);
        break;

        case DUMMY:
            log_c("DUMMY\n");
        break;

        case PACK:
            log_c("PACK %p\n",scb);
            log_c("level:%d\tul:%f\tet:%f\tdl:%llu\n",level,scb->ultilization,scb->et,scb->a_dl);         // display ultilization and deadline
            SCB_reduction_tree_print((SCB**)(&(scb->leaf)),level+1,0);
            SCB_reduction_tree_print(&(scb->next),level,index+1);
        break;

        case DUAL: 
            log_c("DUAL %p\n",scb);
            log_c("ul:%f\tet:%f\trdl:%llu\tadl:%llu\n",scb->ultilization,scb->et,scb->r_dl,scb->a_dl);         // display ultilization and deadline
            SCB_reduction_tree_print((SCB**)(&(scb->leaf)),level+1,0);
            SCB_reduction_tree_print(&(scb->next),level,index+1);
        break;
    }
}

void execution_queue_print(TCB_CNTNR* TC_list)
{
    TCB*      tcb = NULL;
    TCB_CNTNR* tc = NULL;
    if(TC_list == NULL)
    {
        log_c("TC_list is NULL,in execution_queue_print\n");
        return;
    }

    for(tc=TC_list;tc;tc=tc->next)
    {
        tcb = tc->tcb;
        if(tcb == NULL){log_once(NULL,"tcb is NULL,in execution_queue_print\n");kill(getpid(),SIG_SHOULD_NOT_HAPPENED);}

        log_c("job:%p\t%d\t%u\t%llu\t%p\n",tcb,tcb->tid,tcb->et,tcb->a_dl,tcb->something_else);
    }
}

void SVR_CNTNR_list_print(SVR_CNTNR* SVR_list)
{
    SCB* server    = NULL;
    SVR_CNTNR* sc  = NULL;

    if(SVR_list == NULL){return;}

    for(sc=SVR_list;sc;sc=sc->next)
    {
        server = sc->scb;
        if(server == NULL){log_c("This server is NULL\n");continue;}
        log_c("%p\t%f\t%f\t%llu\n",server,
                                              server->ultilization,
                                              server->et,
                                              server->a_dl);
    }
}

#endif
/***************************************************************************************/

TCB_CNTNR* TCB_to_TCB_CNTNR(TCB* tcb)
{
    TCB_CNTNR* tcb_cntnr = NULL;

    overhead_dl += COMP;
    if(tcb==NULL){return NULL;}

    overhead_dl += ASSIGN+MEM;
    tcb_cntnr = (TCB_CNTNR*)malloc(sizeof(TCB_CNTNR));
    if(tcb_cntnr == NULL){log_once(NULL,"Error with malloc,in TCB_to_TCB_CNTNR\n");kill(getpid(),SIG_SHOULD_NOT_HAPPENED);}

    tcb_cntnr->tcb  = tcb;
    tcb_cntnr->next = NULL;

    return tcb_cntnr;
}

SCB* TCB_to_SCB(TCB* tcb)
{
    SCB* scb      = NULL;

    overhead_dl += COMP;
    if(tcb == NULL){return NULL;}

    overhead_dl += ASSIGN + MEM;
    scb = (SCB*)malloc(sizeof(SCB));
    if(scb == NULL){log_once(NULL,"Fail to allocate the memory, in RUN TCB_to_SCB\n");kill(getpid(),SIG_SHOULD_NOT_HAPPENED);}

    overhead_dl += FDIV;
    scb->is_pack          = LEAF;
    scb->ultilization     = my_round(((double)tcb->wcet)/period[tcb->tid]);
    scb->a_dl             = tcb->a_dl;
    scb->r_dl             = period[tcb->tid];
    scb->req_tim          = tcb->req_tim;
    scb->et               = (double)tcb->et;
    scb->leaf             = (void*)tcb; 
    scb->next = scb->root = NULL;

    tcb->something_else = scb;

    return scb;
}

SCB* dummy_SCB_create(double ultilization)
{
    SCB* scb = NULL;

    overhead_dl += COMP;
    if(ultilization <= 0){return NULL;}

    overhead_dl += ASSIGN+MEM;
    scb = (SCB*)malloc(sizeof(SCB));
    if(scb == NULL){log_once(NULL,"scb is NULL,in dummy_SCB_create\n");kill(getpid(),SIG_SHOULD_NOT_HAPPENED);}

    scb->is_pack      = DUMMY;
    scb->ultilization = ultilization;
    scb->a_dl         = 0xFFFFFFFF;
    scb->r_dl         = 0xFFFFFFFF;
    scb->req_tim      = 0xFFFFFFFF;
    scb->et           = (double)0xFFFFFFFF;
    scb->root         = NULL;
    scb->leaf         = NULL;
    scb->next         = NULL;

    return scb;
}

SCB* SCB_to_packed_SCB(SCB* scb)
{
    SCB* packed_scb = NULL;

    overhead_dl += COMP;
    if(scb == NULL){return NULL;}

    overhead_dl += ASSIGN+MEM;
    packed_scb = (SCB*)malloc(sizeof(SCB));
    if(packed_scb == NULL){log_once(NULL,"Error with malloc,in SCB_to_packed_SCB\n");kill(getpid(),SIG_SHOULD_NOT_HAPPENED);}

    packed_scb->is_pack                 = PACK;
    packed_scb->a_dl                    = scb->a_dl;
    packed_scb->r_dl                    = scb->r_dl;
    packed_scb->et                      = scb->et;
    packed_scb->ultilization            = scb->ultilization;
    packed_scb->leaf                    = (void*)scb;
    packed_scb->next = packed_scb->root = NULL;

    scb->root                           = packed_scb;

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
    overhead_dl += FADD; 
    total_ultilization = my_round((packed_SCB->ultilization)+(scb->ultilization));
    
    //Is it available to put this server into this pack
    overhead_dl += COMP;
    if(total_ultilization <= (double)1)
    {
        //Yes, change the ultilization of this pack
        packed_SCB->ultilization = total_ultilization;
        
        //If the new server has later deadline, then extend the deadline of this pack.
        scb->root = packed_SCB;

        overhead_dl += COMP;
        if(packed_SCB->a_dl > scb->a_dl)
        {
            packed_SCB->a_dl = scb->a_dl;
            packed_SCB->r_dl = scb->a_dl;
        }

        overhead_dl += FMUL;
        packed_SCB->et = my_round(total_ultilization * packed_SCB->a_dl);
        
        //New server is a "leaf" for this pack
        scb->root = packed_SCB;
        scb->next = (SCB*)(packed_SCB->leaf);
        packed_SCB->leaf = (void*)scb;

        //If you are here, then you already completed this function
        return 1;
    }

    // Sorry, this server could not put in this server pack
    return 0;
}

SCB* SCB_list_build(TCB** rq)
{
    TCB* p              = NULL;
    SCB* scb            = NULL;
    SCB* scb_list       = NULL;
    double ultilization = (double)0;

    overhead_dl += COMP+COMP;
    if(rq==NULL || *rq==NULL){log_once(NULL,"rq is NULL,in SCB_list_build\n");return NULL;}

    for(p = *rq;p;p=p->next)
    {
        overhead_dl += IADD+COMP;

        scb = TCB_to_SCB(p);
        ultilization += scb->ultilization;
        
        overhead_dl += COMP;
        if(scb_list == NULL){scb_list = scb;}
        else
        {
            scb->next = scb_list;
            scb_list  = scb;
        }
    }

    for(ultilization=((double)PROCESSOR_NUM)-ultilization;ultilization>0;)
    {
        if(ultilization>1)
        {
            overhead_dl += FADD;
            scb = dummy_SCB_create((double)1);
            ultilization = ultilization - (double)1;       
        }
        else
        {
            overhead_dl += FADD;
            scb = dummy_SCB_create(ultilization);
            ultilization -= ultilization;
        }

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
    SCB* SCB_dual          = NULL;

    overhead_dl += COMP;
    if(scb == NULL){log_once(NULL,"Error with malloc,in SCB_server_to_dual\n");return NULL;}

    overhead_dl += ASSIGN+MEM;
    SCB_dual = (SCB*)malloc(sizeof(SCB));
    if(SCB_dual == NULL){log_once(NULL,"Error with malloc,in SCB_to_dual\n");kill(getpid(),SIG_SHOULD_NOT_HAPPENED);}

    overhead_dl += FADD + FMUL;
    SCB_dual->is_pack      = DUAL;
    SCB_dual->a_dl         = scb->a_dl;
    SCB_dual->r_dl         = scb->r_dl;
    SCB_dual->ultilization = my_round((double)1-(scb->ultilization));
    SCB_dual->et           = my_round(SCB_dual->ultilization * SCB_dual->r_dl);
    SCB_dual->leaf         = (void*)scb;
    SCB_dual->next         = NULL;

    scb->root              = SCB_dual;

    return SCB_dual;
}

void server_list_insert(SVR_CNTNR** server_list,SCB* server)
{
    SVR_CNTNR* sc = NULL;

    overhead_dl += COMP+COMP;
    if(server_list==NULL || server==NULL){return;}

    overhead_dl += ASSIGN+MEM+COMP;
    sc = (SVR_CNTNR*)malloc(sizeof(SVR_CNTNR));
    if(sc == NULL)
    {
        log_once(NULL,"Error with malloc, in server_list_insert\n");
        kill(getpid(),SIG_SHOULD_NOT_HAPPENED);
    }

    sc->scb      = server;
    sc->next     = *server_list;
    *server_list = sc;  
}

void server_list_destory(SVR_CNTNR** server_list)
{
    SVR_CNTNR* sc = NULL;

    overhead_dl += COMP+COMP;
    if(server_list==NULL || *server_list==NULL){return;}

    overhead_dl += COMP;
    while(*server_list)
    {
        sc = *server_list;
        *server_list = sc->next;

        overhead_dl += MEM;
        free(sc);
    }
}

int server_list_update(SVR_CNTNR* server_list)
{
    int zero_cnt    = 0;
    SVR_CNTNR* sc   = NULL;
    SCB*      scb   = NULL;
    TCB*      tcb   = NULL;

    overhead_dl += COMP;
    if(server_list == NULL){return 0;}

    for(sc=server_list;sc;sc=sc->next)
    {
        overhead_dl += COMP+MEM;

        overhead_dl += COMP;
        scb = sc->scb;
        if(scb == NULL)
        {
            log_once(NULL,"Server is NULL,in server_list_update\n");
            kill(getpid(),SIG_SHOULD_NOT_HAPPENED);
        }

        overhead_dl += COMP;
        if(scb->is_pack == LEAF)
        {
            overhead_dl += COMP+COMP;
            tcb = (TCB*)(scb->leaf);
            if(tcb == NULL){log_once(NULL,"tcb is NULL,in server_list_update\n");kill(getpid(),SIG_SHOULD_NOT_HAPPENED);}
            scb->et = tcb->et;
        }
        else
        {
            overhead_dl += FADD;
            scb->et-=(double)1;
        }

#ifdef DEBUG
        log_c("scb %p\t%f\n",scb,scb->et);
#endif

        overhead_dl += COMP;
        if(scb->et <= (double)0)
        {
#ifdef DEBUG
            log_c("Server exhausted : %p\n",scb);
#endif
        
            zero_cnt++;
        }
    }

    return zero_cnt;
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
        else{tc = &((*tc)->next);}
    }

    overhead_dl += COMP;
    if(*tc == NULL){*tc = new_tc;}
}

TCB_CNTNR* execution_queue_retrieve(TCB_CNTNR** TCB_CNTNR_list)
{
    TCB_CNTNR* tc=NULL;
    
    overhead_dl += COMP+COMP;
    if(TCB_CNTNR_list==NULL || *TCB_CNTNR_list==NULL){return NULL;}
    
    tc              = *TCB_CNTNR_list;
    *TCB_CNTNR_list = tc->next;
    
    tc->next = NULL;
    
    return tc;
}

void execution_queue_destory(TCB_CNTNR** TCB_CNTNR_queue)
{
    overhead_dl += COMP+COMP;
    if(TCB_CNTNR_queue==NULL || *TCB_CNTNR_queue==NULL){return;}
    
    while(execution_queue_retrieve(TCB_CNTNR_queue)!=NULL){overhead_dl += COMP;}
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

void RUN_proper_server_remove(SCB** SCB_node,SCB** SCB_list)
{
    SCB*  SCB_tmp = NULL;
    SCB** scb     = NULL;

    overhead_dl += COMP+COMP+COMP;
    if(SCB_node==NULL || SCB_list==NULL || *SCB_list==NULL){return;}

    overhead_dl += COMP;
    scb = SCB_list;
    if((*scb)->next == NULL)         // only one element in this SCB_list
    {
        SCB_tmp       = *scb;
        *scb          = ((*scb)->next);
        SCB_tmp->next = NULL;

        overhead_dl += COMP;
        if(*SCB_node == NULL){*SCB_node = SCB_tmp;}
        else
        {
            SCB_tmp->next = *SCB_node;
            *SCB_node = SCB_tmp;
        }
        return;
    }

    for(scb = SCB_list;*scb;)
    {
        overhead_dl += IADD+COMP;
        overhead_dl += COMP+FADD;
        if(IS_EQUAL((*scb)->ultilization,(double)1))
        {
            SCB_tmp = *scb;
            *scb = ((*scb)->next);
            SCB_tmp->next = NULL;

            overhead_dl += COMP;
            if(*SCB_node == NULL){*SCB_node = SCB_tmp;}
            else
            {
                SCB_tmp->next = *SCB_node;
                *SCB_node = SCB_tmp;
            }
        }
        else{scb=&((*scb)->next);}
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
#ifdef DEBUG
    log_c("scb list server \n");
    SCB_list_print(&SCB_server_list);
    log_c("\n");
#endif

    while(TRUE)
    {
        overhead_dl += COMP+COMP;

        SCB_packed_server_list = SCB_list_pack(&SCB_server_list);
#ifdef DEBUG
        log_c("packed server \n");
        SCB_list_print(&SCB_packed_server_list);
        log_c("\n");
#endif
        RUN_proper_server_remove(&SCB_reduction_root,&SCB_packed_server_list);
#ifdef DEBUG
        log_c("proper system\n");
        SCB_list_print(&SCB_reduction_root);
        log_c("\n");
#endif
        if(SCB_packed_server_list == NULL){break;}

        SCB_dual_server_list = SCB_dual_list_build(&SCB_packed_server_list);
#ifdef DEBUG
        log_c("dual server\n");
        SCB_list_print(&SCB_dual_server_list);
        log_c("\n");
#endif
        SCB_server_list = SCB_dual_server_list;
    }

    return SCB_reduction_root;
}


int SCB_reduction_tree_node_update(SCB* SCB_node)
{
    int                    updated = 0;
    SCB*                       scb = NULL;
    unsigned long long earliest_dl = TICKS;

    overhead_dl += COMP;
    if(SCB_node == NULL){return 0;}

    overhead_dl += COMP;
    switch(SCB_node->is_pack)
    {
        case PACK:
            overhead_dl += COMP;
            scb = (SCB*)(SCB_node->leaf);
            if(scb == NULL){log_once(NULL,"SCB_node->leaf is NULL,in SCB_reduction_tree_node_update");kill(getpid(),SIG_SHOULD_NOT_HAPPENED);}

            for(;scb;scb=scb->next)
            {   
                overhead_dl += COMP+IADD;
                overhead_dl += COMP;
                updated |= SCB_reduction_tree_node_update(scb);
                if(scb->a_dl > tick)
                {
                    if(scb->a_dl < earliest_dl){earliest_dl = scb->a_dl;}
                }   
            }

            overhead_dl += COMP;
            if(updated)
            {
                overhead_dl += FADD+FMUL;
                SCB_node->a_dl = earliest_dl;
                SCB_node->r_dl = earliest_dl - tick;
                SCB_node->et   = my_round(SCB_node->ultilization * SCB_node->r_dl + SCB_node->et);
            }
            return updated;
        break;
        
        case DUAL:
            overhead_dl += COMP;
            if(!SCB_reduction_tree_node_update((SCB*)(SCB_node->leaf))){return 0;}
            
            overhead_dl += COMP;
            scb = (SCB*)(SCB_node->leaf);
            if(scb == NULL){log_once(NULL,"SCB_node->leaf is NULL,in SCB_reduction_tree_node_update");kill(getpid(),SIG_SHOULD_NOT_HAPPENED);}
            
            overhead_dl += FMUL+FADD;
            SCB_node->a_dl = scb->a_dl;
            SCB_node->r_dl = scb->r_dl;
            SCB_node->et   = my_round(SCB_node->r_dl * SCB_node->ultilization + SCB_node->et) ;
            return 1;
        break;

        case LEAF:
            overhead_dl += COMP;
            if(SCB_node->req_tim == tick){return 1;}
        break;

        case DUMMY:
            return 0;
        break;
    }

    return 0;
}

void SCB_reduction_tree_update(SCB* SCB_root)
{
    SCB* scb = NULL;

    overhead_dl += COMP;
    if(SCB_root == NULL){return;}

    for(scb=SCB_root;scb;scb=scb->next)
    {
        overhead_dl += COMP+MEM;
        SCB_reduction_tree_node_update(scb);
    }
}

void SCB_reduction_tree_destory(SCB** SCB_node)
{
    SCB*      scb = NULL;
    TCB_CNTNR* tc = NULL;

    overhead_dl += COMP+COMP;
    if(SCB_node==NULL || *SCB_node==NULL){return;}
    
    scb = *SCB_node;

    overhead_dl += COMP;
    switch(scb->is_pack)
    {
        case LEAF:
            overhead_dl += COMP+MEM;
            tc = (TCB_CNTNR*)scb->leaf;
            if(tc!=NULL){free(tc);}
        break;

        case DUMMY:
        break;

        default:
            SCB_reduction_tree_destory(&(scb->next));
            SCB_reduction_tree_destory((SCB**)&(scb->leaf));
            *SCB_node = NULL;
        break;
    }
    overhead_dl += MEM;
    free(scb);
}

void SCB_reduction_tree_unpack_by_node(SCB** SCB_node,int selected)
{
    SCB* scb              = NULL;
    SCB* SCB_tmp          = NULL;
    SCB* earliest_ddl_SCB = NULL;
    TCB_CNTNR* tc         = NULL;

    overhead_dl += COMP+COMP;
    if(SCB_node==NULL || *SCB_node==NULL){return;}

    overhead_dl += COMP+COMP;
    if(selected && (*SCB_node)->et>0){server_list_insert(&selected_server_list,*SCB_node);}

    scb = *SCB_node;

    overhead_dl += COMP;
    switch(scb->is_pack)
    {
        case LEAF:
            overhead_dl += COMP;
            if(!selected){return;}
            
            tc = TCB_to_TCB_CNTNR((TCB*)(scb->leaf));
            execution_queue_insert(tc);
        break;

        case DUMMY:
            return;
        break;

        case PACK:
            overhead_dl += COMP;
            if(!selected)
            {
                for(SCB_tmp = (SCB*)(scb->leaf);SCB_tmp;SCB_tmp=SCB_tmp->next)
                {
                    overhead_dl += COMP+MEM;
                    SCB_reduction_tree_unpack_by_node(&(SCB_tmp),0);
                }
                return;
            }

            // This part is quite confusing people
            earliest_ddl_SCB = NULL;
            for(SCB_tmp = (SCB*)(scb->leaf);SCB_tmp;SCB_tmp=SCB_tmp->next)
            {
                overhead_dl += COMP+MEM;
                // Find the servser with rest execution time bigger than 0
                overhead_dl += COMP;
                if(SCB_tmp->et > (double)0)
                {
                    overhead_dl += COMP;
                    if(earliest_ddl_SCB == NULL){earliest_ddl_SCB = SCB_tmp;}
                    else
                    {
                        // Find a server with earliest deadline, and record it.
                        overhead_dl += COMP;
                        if(SCB_tmp->a_dl < earliest_ddl_SCB->a_dl)
                        {
                            SCB_reduction_tree_unpack_by_node(&(earliest_ddl_SCB),0);
                            earliest_ddl_SCB = SCB_tmp;
                        }
                        else
                        {
                            // These server`s deadline is not the earliest deadline
                            SCB_reduction_tree_unpack_by_node(&(SCB_tmp),0);
                        }   
                    }
                }
                else
                {
                    // These server`s rest exectuion time is 0
                    SCB_reduction_tree_unpack_by_node(&(SCB_tmp),0);
                }
            }
            
            //This is the server which we are looking for.
            SCB_reduction_tree_unpack_by_node(&earliest_ddl_SCB,1);
        break;

        case DUAL: 
            SCB_reduction_tree_unpack_by_node((SCB**)(&(scb->leaf)),!selected);
        break;
    }
}

void SCB_reduction_tree_unpack(SCB** SCB_node)
{
    SCB* scb     = NULL;

    overhead_dl += COMP+COMP;
    if(SCB_node==NULL || *SCB_node==NULL){return;}

    for(scb=*SCB_node;scb;scb=scb->next)
    {
        overhead_dl += COMP+MEM;
        SCB_reduction_tree_unpack_by_node(&scb,1);
    }
}

void RUN_scheduling_initialize()
{
    int i;
    has_server_finished  = 0;
    SCB_root             = NULL;
    execution_queue      = NULL;
    RUN_ready_queue      = NULL;
    update_tc            = NULL;
    selected_server_list = NULL;

    for(i=0;i<MAX_TASKS;i++){assignment_history[i]=-1;}

    log_open("RUN_debug.txt");
}

void RUN_scheduling_update()
{
    overhead_dl += COMP;
    if(server_list_update(selected_server_list)){has_server_finished = 1;}
}

int RUN_insert_OK(TCB* t1,TCB* t2)
{
    SCB*      scb = NULL;

    overhead_dl += COMP;
    if(t1 == NULL){return 0;}

    // This is a new task, so no need to search from RUN_TCB_list
    overhead_dl += COMP;
    if(has_new_task){return 1;}

#ifdef DEBUG
    log_c("At tick:: %llu,%d has came.\n",tick,t1->tid);
#endif

    overhead_dl += COMP;
    scb = (SCB*)(t1->something_else);
    if(scb == NULL){log_once(NULL,"scb is NULL, in RUN_insert_OK\n");kill(getpid(),SIG_SHOULD_NOT_HAPPENED);}

    scb->et      = (double)t1->et;
    scb->a_dl    = t1->a_dl;
    scb->req_tim = tick;

    return 1;
}

void RUN_reorganize_function(TCB** rq)
{

    overhead_dl += COMP+COMP;
    if(!has_new_task && !has_new_instance && !has_server_finished){return;}

    overhead_dl += COMP+COMP;
    if(rq==NULL || *rq==NULL)
    {
        server_list_destory(&selected_server_list);
        execution_queue_destory(&execution_queue);
        return;
    }

#ifdef DEBUG
    log_c("tick : %llu nt:%d ni:%d sf:%d\n",tick,has_new_task,has_new_instance,has_server_finished);
    log_c("Original TCB list:\n");
    TCB_list_print(p_ready_queue);
#endif

    overhead_dl += COMP;
    if(has_new_task)
    {
        execution_queue_destory(&execution_queue);

        SCB_reduction_tree_destory(&SCB_root);

        SCB_root = SCB_reduction_tree_build(rq);
#ifdef DEBUG
        SCB_reduction_tree_print(&SCB_root,0,0);
#endif
        server_list_destory(&selected_server_list);
        
        SCB_reduction_tree_unpack(&SCB_root);
#ifdef DEBUG
        execution_queue_print(execution_queue);
#endif
    }
    else if(has_new_instance)
    {
        overhead_dl += COMP;
#ifdef DEBUG
        log_c("Has new instance,will update reduction tree\n"); 
        log_c("old Servers:\n");
        SVR_CNTNR_list_print(selected_server_list);
#endif
        execution_queue_destory(&execution_queue);

        server_list_destory(&selected_server_list);

        SCB_reduction_tree_update(SCB_root);
#ifdef DEBUG
        SCB_reduction_tree_print(&SCB_root,0,0);
#endif

        SCB_reduction_tree_unpack(&SCB_root);

#ifdef DEBUG
        log_c("New servers:\n");
        SVR_CNTNR_list_print(selected_server_list);
        execution_queue_print(execution_queue);
#endif
    }
    else if(has_server_finished)
    {
        overhead_dl += COMP;
#ifdef DEBUG
        log_c("Has server finished,will unpack reduction tree again\n"); 
        log_c("old Servers:\n");
        SVR_CNTNR_list_print(selected_server_list);
#endif
        execution_queue_destory(&execution_queue);

        server_list_destory(&selected_server_list);
#ifdef DEBUG
        SCB_reduction_tree_print(&SCB_root,0,0);
#endif
        SCB_reduction_tree_unpack(&SCB_root);

#ifdef DEBUG
        log_c("New servers:\n");
        SVR_CNTNR_list_print(selected_server_list);
        execution_queue_print(execution_queue);
#endif
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
    
    overhead_dl += COMP;
    if(tcb == NULL){return -1;}
    
    overhead_dl += COMP;
    if((processor_id=check_assignment_history(tcb)) == -1)
    {
        for(i=0;i<PROCESSOR_NUM;i++)
        {
            overhead_dl += COMP+IADD;
            overhead_dl += COMP;
            if(_kernel_runtsk[i]==NULL){break;}
        }
        overhead_dl += COMP;
        if(i<PROCESSOR_NUM){processor_id = i;}
    }

    return processor_id;
}

void RUN_schedule()
{
    int i,processor_id;
    int assigned[PROCESSOR_NUM] = {0};
    TCB_CNTNR* tc = NULL;
    
    overhead_dl += COMP+COMP+COMP;
    if(!has_new_instance && !has_new_task && !has_server_finished){return;}

    overhead_dl += COMP+COMP+COMP;
    if(has_new_instance || has_new_task || has_server_finished)
    {
        has_new_instance = has_new_task = has_task_finished = has_server_finished = 0;

        for(i=0;i<PROCESSOR_NUM;i++)
        {
            overhead_dl += COMP+IADD;
            _kernel_runtsk[i] = NULL;
        }
    
        while((tc=execution_queue_retrieve(&execution_queue)) != NULL)
        {
            overhead_dl += COMP;
            overhead_dl += COMP+COMP;
            if(tc == NULL){log_once(NULL,"tc is NULL,in RUN schedule\n");break;}
            if(tc->tcb == NULL){log_once(NULL,"tc->tcb is NULL,in RUN schedule\n");break;}
            
            processor_id = processor_assignment(tc->tcb);
            overhead_dl += COMP+COMP;
            if(processor_id==-1 || assigned[processor_id])
            {
                processor_id = -1;
                for(i=0;i<PROCESSOR_NUM;i++)
                {
                    overhead_dl += COMP+IADD;
                    overhead_dl += COMP;
                    if(!assigned[i]){break;}
                }
                overhead_dl += COMP+COMP;
                if(i<PROCESSOR_NUM && i>=0){processor_id = i;}
            }
            
#ifdef DEBUG
            log_c("tc %p,tcb %p,tid %d %d\n",tc,tc->tcb,tc->tcb->tid,processor_id);
#endif
            // Since we didn't found any available Processor.
            overhead_dl += COMP;
            if(processor_id == -1){break;}

            _kernel_runtsk[processor_id] = tc->tcb;
            assigned[processor_id] = 1;
            
            assignment_history[tc->tcb->tid] = processor_id;
        }
    }

#ifdef DEBUG
    for(i=0;i<PROCESSOR_NUM;i++)
    {
        if(!_kernel_runtsk[i]){continue;}
        log_c("pid:%d\t%p\ttid:%d\n",i,_kernel_runtsk[i],_kernel_runtsk[i]->tid);
    }
    //getchar();
#endif
    
}

void RUN_scheduling_exit()
{
    SCB_reduction_tree_destory(&SCB_root);

    server_list_destory(&selected_server_list);

    execution_queue_destory(&execution_queue);

    log_close();
#ifdef DEBUG
    printf("RUN end normally\n");
#endif

}