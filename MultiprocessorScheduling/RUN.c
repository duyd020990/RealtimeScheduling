#include "RUN.h"

#include "Schedule.h"
#include "signal.h"
#include "EDF.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

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
    .scheduling_initialize = RUN_scheduling_initialize,
    .scheduling_exit       = RUN_scheduling_exit,
    .scheduling            = RUN_schedule,
    .insert_OK             = RUN_insert_OK,
    .reorganize_function   = RUN_reorganize_function
};


/*************************** FOR DEBUG ***********************************/
#ifdef DEBUG
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
        fprintf(stderr,"%d\t%f\t%llu\t%p\n",i                    ,
                                          scb->ultilization    ,
                                          scb->deadline        ,
                                          scb->leaf            );
    }
}

void SCB_reduction_tree_print(SCB** SCB_node,int level,int index)
{
    SCB*      scb = NULL;
    TCB*      tcb = NULL;
    TCB_CNTNR* tc = NULL;

    if(SCB_node==NULL || *SCB_node==NULL){fprintf(stderr,"SCB_node is NULL,in SCB_reduction_tree_print\n");return;}
    
    fprintf(stderr,"level %d-%d:",level,index);

    scb = *SCB_node;
    switch(scb->is_pack)
    {
        case LEAF:
            fprintf(stderr,"LEAF\n");
            tc = (TCB_CNTNR*)(scb->leaf);
            if(tc == NULL){fprintf(stderr,"tc is NULL, in SCB_reduction_tree_print\n");while(1);}
            tcb = tc->tcb;
            if(tcb == NULL){fprintf(stderr,"This tc was exhausted, tid %d\n",tc->tid);}
            else
            {
                fprintf(stderr,"%p\ttid:%d\tet:%u\tdl:%llu\n",tcb,
                                                  tcb->tid,
                                                  tcb->et,
                                                  tcb->a_dl);         // display job information
            }
            SCB_reduction_tree_print(&(scb->next),level,index+1);
        break;

        case PACK:
            fprintf(stderr,"PACK\n");
            fprintf(stderr,"level:%d\tul:%f\tdl:%llu\tnext:%p\n",level,scb->ultilization,scb->deadline,scb->next);         // display ultilization and deadline
            SCB_reduction_tree_print((SCB**)(&(scb->leaf)),level+1,0);
            SCB_reduction_tree_print(&(scb->next),level,index+1);
        break;

        case DUAL: 
            fprintf(stderr,"DUAL\n");
            fprintf(stderr,"ul:%f\tdl:%llu\n",scb->ultilization,scb->deadline);         // display ultilization and deadline
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
        fprintf(stderr,"TC_list is NULL,in execution_queue_print\n");
        return;
    }

    for(tc=TC_list;tc;tc=tc->next)
    {
        tcb = tc->tcb;
        if(tcb == NULL){fprintf(stderr,"tcb is NULL,in execution_queue_print\n");kill(getpid(),SIG_SHOULD_NOT_HAPPENED);}

        fprintf(stderr,"job:%p\t%d\t%u\t%llu\t%p\n",tcb,tcb->tid,tcb->et,tcb->a_dl,tcb->something_else);
    }
}

void TCB_list_print(TCB* TCB_list)
{
    TCB* tcb = NULL;

    if(TCB_list == NULL){fprintf(stderr,"TCB_list is NULL,in TCB_list_print\n");return;}

    for(tcb=TCB_list;tcb;tcb=tcb->next)
    {
        fprintf(stderr,"job: %p\t%d\t%u\t%llu\t%p\n",tcb,tcb->tid,tcb->et,tcb->a_dl,tcb->something_else);
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
    if(tcb_cntnr == NULL){printf("Error with malloc,in TCB_to_TCB_CNTNR\n");return NULL;}

    tcb_cntnr->tid  = tcb->tid;
    tcb_cntnr->tcb  = tcb;
    tcb_cntnr->root = NULL;
    tcb_cntnr->next = NULL;

    return tcb_cntnr;
}

SCB* TCB_to_SCB(TCB* tcb)
{
    SCB* scb      = NULL;
    TCB_CNTNR* tc = NULL;

    overhead_dl += COMP;
    if(tcb == NULL){return NULL;}

    tc = TCB_to_TCB_CNTNR(tcb);
    if(tc == NULL){fprintf(stderr,"tc is NULL,in TCB_to_SCB\n");kill(getpid(),SIG_SHOULD_NOT_HAPPENED);}

    overhead_dl += ASSIGN + MEM;
    scb = (SCB*)malloc(sizeof(SCB));
    if(scb == NULL){fprintf(stderr,"Fail to allocate the memory, in RUN TCB_to_SCB\n");return  NULL;}

    tc->root = scb;

    overhead_dl += FDIV;
    scb->is_pack          = LEAF;
    scb->ultilization     = ((double)tcb->wcet)/period[tcb->tid];
    scb->deadline         = tcb->a_dl;
    scb->et               = tcb->et;
    scb->leaf             = (void*)tc; 
    scb->next = scb->root = NULL;

    return scb;
}

SCB* SCB_to_packed_SCB(SCB* scb)
{
    SCB* packed_scb = NULL;

    overhead_dl += COMP;
    if(scb == NULL){return NULL;}

    overhead_dl += ASSIGN+MEM;
    packed_scb = (SCB*)malloc(sizeof(SCB));
    if(packed_scb == NULL){printf("Error with malloc,in SCB_to_packed_SCB\n");return NULL;}

    packed_scb->is_pack                 = PACK;
    packed_scb->deadline                = scb->deadline;
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

        packed_SCB->et           = total_ultilization * packed_SCB->deadline;
        
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
    TCB* p        = NULL;
    SCB* scb      = NULL;
    SCB* scb_list = NULL;

    overhead_dl += COMP+COMP;
    if(rq==NULL || *rq==NULL){fprintf(stderr,"rq is NULL,in SCB_list_build\n");return NULL;}

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
    SCB_dual->leaf         = (void*)scb;
    SCB_dual->next         = NULL;

    return SCB_dual;
}

void server_list_insert(SVR_CNTNR** server_list,SCB* server)
{
    SVR_CNTNR* sc = NULL;

    if(server_list==NULL || server==NULL){return;}

    sc = (SVR_CNTNR*)malloc(sizeof(SVR_CNTNR));
    if(sc == NULL)
    {
        fprintf(stderr,"Error with malloc, in server_list_insert\n");
        kill(getpid(),SIG_SHOULD_NOT_HAPPENED);
    }

    sc->scb      = server;
    sc->next     = *server_list;
    *server_list = sc;  
}

void server_list_destory(SVR_CNTNR** server_list)
{
    SVR_CNTNR* sc = NULL;

    if(server_list==NULL || *server_list==NULL){return;}

    while(*server_list)
    {
        sc = *server_list;
        server_list = &(sc->next);
        free(sc);
    }
}

int server_list_update(SVR_CNTNR* server_list)
{
    int zero_cnt  = 0;
    SVR_CNTNR* sc = NULL;
    SCB*      scb = NULL;

    if(server_list == NULL){return 0;}

    for(sc=server_list;sc;sc=sc->next)
    {
        scb = sc->scb;
        if(scb == NULL)
        {
            fprintf(stderr,"server is NULL,in server_list_update\n");
            kill(getpid(),SIG_SHOULD_NOT_HAPPENED);
        }

        scb->et--;
        fprintf(stderr,"scb %p\t%llu\n",scb,scb->et);
        getchar();
        if(scb->et == 0)
        {
#ifdef DEBUG
            fprintf(stderr,"Server exhausted : %p\n",scb);
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
    
    tc->next = NULL;
    
    return tc;
}

void execution_queue_destory(TCB_CNTNR** TCB_CNTNR_queue)
{
    if(TCB_CNTNR_queue==NULL || *TCB_CNTNR_queue==NULL){return;}
    
    while(execution_queue_retrieve(TCB_CNTNR_queue)!=NULL);
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
    SCB** scb = NULL;

    overhead_dl += COMP+COMP+COMP;
    if(SCB_node==NULL || SCB_list==NULL || *SCB_list==NULL){return;}

    scb = SCB_list;
    if((*scb)->next == NULL)         // only one element in this SCB_list
    {
        SCB_tmp = *scb;
        *scb = ((*scb)->next);
        SCB_tmp->next = NULL;

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
        overhead_dl += COMP;
        if((*scb)->ultilization == (double)1)
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
#ifdef DEBUG
    fprintf(stderr,"scb list server \n");
    SCB_list_print(&SCB_server_list);
    fprintf(stderr,"\n");
#endif

    while(1)
    {
        overhead_dl += COMP+COMP;

        SCB_packed_server_list = SCB_list_pack(&SCB_server_list);
#ifdef DEBG
        fprintf(stderr,"packed server \n");
        SCB_list_print(&SCB_packed_server_list);
        fprintf(stderr,"\n");
#endif
        RUN_proper_server_remove(&SCB_reduction_root,&SCB_packed_server_list);
#ifdef DEBUG
        fprintf(stderr,"proper system\n");
        SCB_list_print(&SCB_reduction_root);
        fprintf(stderr,"\n");
#endif
        if(SCB_packed_server_list == NULL){break;}

        SCB_dual_server_list = SCB_dual_list_build(&SCB_packed_server_list);
#ifdef DEBUG
        fprintf(stderr,"dual server\n");
        SCB_list_print(&SCB_dual_server_list);
        fprintf(stderr,"\n");
#endif
        SCB_server_list = SCB_dual_server_list;
    }

    return SCB_reduction_root;
}

TCB_CNTNR* SCB_reduction_tree_TCB_CNTNR_search(SCB* SCB_node,TCB* tcb)
{
    TCB_CNTNR* tc = NULL;

    if(SCB_node==NULL || tcb==NULL){return NULL;}
    
    switch(SCB_node->is_pack)
    {
        case LEAF:
            tc = (TCB_CNTNR*)(SCB_node->leaf);
            if(tc == NULL)
            {
                fprintf(stderr,"tc is NULL,in SCB_reduction_tree_TCB_CNTNR_search\n");
                kill(getpid(),SIG_SHOULD_NOT_HAPPENED);
            }
            if(tc->tid == tcb->tid){fprintf(stderr,"found target %d\n",tc->tid);return tc;}
            else if((tc=SCB_reduction_tree_TCB_CNTNR_search(SCB_node->next,tcb))!=NULL){return tc;}
            else{return NULL;}
        break;
        
        default:
            if((tc=SCB_reduction_tree_TCB_CNTNR_search(SCB_node->next,tcb))!=NULL){return tc;}
            if((tc=SCB_reduction_tree_TCB_CNTNR_search((SCB*)(SCB_node->leaf),tcb))!=NULL){return tc;}
        break;
    }

    return NULL;
}

void SCB_reduction_tree_node_update(SCB* SCB_node)
{
    SCB* SCB_father = NULL;

    if(SCB_node == NULL){return;}

    if(SCB_node->root == NULL){return;}
    
    SCB_father = SCB_node->root;
    if(SCB_father->deadline < SCB_node->deadline){SCB_father->deadline = SCB_node->deadline;}
    SCB_father->et = SCB_father->ultilization * SCB_father->deadline;

    SCB_reduction_tree_node_update(SCB_father);

    return;
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
            //SCB_reduction_tree_destory(&(scb->next));
            tc = (TCB_CNTNR*)scb->leaf;
            if(tc!=NULL)
            {
                free(tc);
            }
        break;

        default:
            SCB_reduction_tree_destory(&(scb->next));
            SCB_reduction_tree_destory((SCB**)&(scb->leaf));
            *SCB_node = NULL;
        break;
    }
    free(scb);
}

void RUN_reduction_tree_unpack_by_root(SCB** SCB_node,int selected)
{
    SCB* scb              = NULL;
    SCB* SCB_tmp          = NULL;
    SCB* earliest_ddl_SCB = NULL;
    TCB_CNTNR* tc         = NULL;

    overhead_dl += COMP+COMP;
    if(SCB_node==NULL || *SCB_node==NULL){return;}

    scb = *SCB_node;

    overhead_dl += COMP;
    switch(scb->is_pack)
    {
        case LEAF:
            if(!selected){return;}
            
            tc = (TCB_CNTNR*)scb->leaf;
            execution_queue_insert(tc);
        break;

        case PACK:
            if(!selected){return;}

            // This part is quite confusing people
            earliest_ddl_SCB = NULL;
            for(SCB_tmp = scb->leaf;SCB_tmp;SCB_tmp=SCB_tmp->next)
            {
                // Find the servser with rest execution time bigger than 0
                if(SCB_tmp->et != 0)
                {
                    if(earliest_ddl_SCB == NULL){earliest_ddl_SCB = SCB_tmp;}
                    else
                    {
                        // Find a server with earliest deadline, and record it.
                        if(SCB_tmp->deadline < earliest_ddl_SCB->deadline)
                        {
                            earliest_ddl_SCB = SCB_tmp;
                        }
                        else
                        {
                            // These server`s deadline is not the earliest deadline
                            RUN_reduction_tree_unpack_by_root(&(SCB_tmp),0);
                        }   
                    }
                }
                else
                {
                    // These server`s rest exectuion time is 0
                    RUN_reduction_tree_unpack_by_root(&(SCB_tmp),0);
                }
            }
            
            //This is the server which we are looking for.
            RUN_reduction_tree_unpack_by_root(&earliest_ddl_SCB,1);
            server_list_insert(&selected_server_list,earliest_ddl_SCB);
        break;

        case DUAL: 
            RUN_reduction_tree_unpack_by_root((SCB**)(&(scb->leaf)),!selected);
        break;
    }
}

void RUN_reduction_tree_unpack(SCB** SCB_node)
{
    SCB* scb     = NULL;
    SCB* SCB_min = NULL;

    overhead_dl += COMP+COMP;
    if(SCB_node==NULL || *SCB_node==NULL){return;}

    server_list_destory(&selected_server_list);

    SCB_min = *SCB_node;

    for(scb=*SCB_node;scb;scb=scb->next)
    {
        RUN_reduction_tree_unpack_by_root(&scb,1);
    }
    /*
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
    */
}

void RUN_scheduling_initialize()
{
    int i;
    has_server_finished = 0;
    SCB_root        = NULL;
    execution_queue = NULL;
    RUN_ready_queue = NULL;
    update_tc       = NULL;
    selected_server_list = NULL;

    for(i=0;i<MAX_TASKS;i++){assignment_history[i]=-1;}
}

int RUN_insert_OK(TCB* t1,TCB* t2)
{
    SCB*      scb = NULL;
    TCB_CNTNR* tc = NULL;

    if(t1 == NULL){return 0;}

    // This is a new task, so no need to search from RUN_TCB_list
    if(has_new_task){return 1;}

#ifdef DEBUG
    fprintf(stderr,"At tick:: %llu,%d has came.\n",tick,t1->tid);
#endif

    tc = SCB_reduction_tree_TCB_CNTNR_search(SCB_root,t1);
    if(tc == NULL){fprintf(stderr,"tc is NULL,in RUN_insert_OK\n");kill(getpid(),SIG_SHOULD_NOT_HAPPENED);}

    tc->tcb = t1;

    scb = tc->root;
    if(scb == NULL){fprintf(stderr,"scb is NULL, in RUN_insert_OK\n");kill(getpid(),SIG_SHOULD_NOT_HAPPENED);}

    scb->et       = t1->et;
    scb->deadline = t1->a_dl;
    SCB_reduction_tree_node_update(scb);

    return 1;
}

void RUN_reorganize_function(TCB** rq)
{

    if(server_list_update(selected_server_list)){has_server_finished = 1;}

    overhead_dl += COMP+COMP;
    if(!has_new_task && !has_new_instance && !has_server_finished){return;}

    if(rq==NULL || *rq==NULL){return;}

#ifdef DEBUG
    fprintf(stderr,"tick : %llu\n",tick);
    fprintf(stderr,"Original TCB list:\n");
    TCB_list_print(p_ready_queue);
#endif

    if(has_new_task)
    {
#ifdef DEBUG
        fprintf(stderr,"Has new task,will rebuild this reduction tree\n");
#endif
        execution_queue_destory(&execution_queue);

        SCB_reduction_tree_destory(&SCB_root);

        SCB_root = SCB_reduction_tree_build(rq);
#ifdef DEBUG
        fprintf(stderr,"reduction tree print\n");
        SCB_reduction_tree_print(&SCB_root,0,0);
#endif
        RUN_reduction_tree_unpack(&SCB_root);
        execution_queue_print(execution_queue);
    }
    else if(has_new_instance)
    {
#ifdef DEBUG
        fprintf(stderr,"Has new instance,will update reduction tree\n"); 
#endif
        execution_queue_destory(&execution_queue);

        RUN_reduction_tree_unpack(&SCB_root);
    }
    else if(has_server_finished)
    {
#ifdef DEBUG
        fprintf(stderr,"Has server finished,will unpack reduction tree again\n"); 
#endif
        execution_queue_destory(&execution_queue);

        RUN_reduction_tree_unpack(&SCB_root);
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
    
    if(!has_new_instance && !has_new_task && !has_task_finished && !has_server_finished){return;}

    if(has_new_instance || has_new_task)
    {
        has_new_instance = has_new_task = has_task_finished = 0;

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
            tc->tcb = NULL;
        }
    }
    
    if(has_task_finished||has_server_finished)
    {
        has_task_finished = 0;
        fprintf(stderr,"Has task or server finished,in RUN_SCheduling\n");

        for(i=0;i<PROCESSOR_NUM;i++)
        {
            if(_kernel_runtsk[i] != NULL){continue;}

            tc = execution_queue_retrieve(&execution_queue);
            if(tc == NULL){printf("tc is NULL,in RUN schedule\n");break;}
            if(tc->tcb == NULL){printf("tc->tcb is NULL,in RUN schedule\n");break;}
           
            fprintf(stderr,"tc %p,tcb %p,tid %d\n",tc,tc->tcb,tc->tcb->tid); 
            _kernel_runtsk[i] = tc->tcb;
            assignment_history[tc->tcb->tid] = i;
            tc->tcb = NULL;
        }
    }

#ifdef DEBUG
    for(i=0;i<PROCESSOR_NUM;i++)
    {
        if(!_kernel_runtsk[i]){continue;}
        fprintf(stderr,"pid:%d\t%p\ttid:%d\n",i,_kernel_runtsk[i],_kernel_runtsk[i]->tid);
    }
    getchar();
#endif
    
}

void RUN_scheduling_exit(){}