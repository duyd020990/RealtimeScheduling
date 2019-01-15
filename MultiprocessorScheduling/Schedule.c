#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#include "Schedule.h"

#include "log/log.h"
#include "tool/tool.h"
#include "signal/signal.h"

//Scehduling algorithms
//#include "RM.h"
//#include "LSF.h"
//#include "EDF.h"
//#include "LLREF.h"
//#include "RUN.h"
#include "HEDF.h"
//#define AP_ALSO

#ifdef LSF
extern SCHEDULING_ALGORITHM LSF_sa;
#endif

#ifdef RM
extern SCHEDULING_ALGORITHM RM_sa;
#endif

#ifdef EDF
extern SCHEDULING_ALGORITHM EDF_sa;
#endif

#ifdef LLREF
extern int release_time[TICKS];
extern SCHEDULING_ALGORITHM LLREF_sa;
#endif

#ifdef RUN
extern SCHEDULING_ALGORITHM RUN_sa; 
#endif

#ifdef HEDF
extern SCHEDULING_ALGORITHM HEDF_sa;
#endif

/***********************************************************************************************************/
/* Variables                                                                                               */
/***********************************************************************************************************/
char*               current_algorithm;
unsigned long long  tick;
unsigned            et[MAX_TASKS][MAX_INSTANCES];       /* Rest execution time of each job                 */
unsigned            wcet[MAX_TASKS];                    /* Worst execution time                            */
unsigned            inst_no[MAX_TASKS];                 /* Instance number of each task                    */
unsigned            periodic[MAX_TASKS];                /* This task is periodic tasks or not              */
unsigned long long  period[MAX_TASKS];                  /* The period of each task                         */
unsigned long long  phase[MAX_TASKS];                   /* Phase of periodic tasks                         */
unsigned long long  req_tim[MAX_TASKS][MAX_INSTANCES];  /* The request time of each task                   */
unsigned            exec_times[MAX_TASKS];              /* OK, I don't what's the purpose of this array :P */
double              p_util=0.0;                         /* The total ultilization of task set              */
double              job_util[MAX_TASKS];                /* The ultilization of each job                    */
unsigned int        aperiodic_exec_times;               /* Don't know the purpose, might be for those aperiodic tasks :P */
unsigned int        aperiodic_total_et;                 /* Don't know the purpose, might be for those aperiodic tasks :P */
double              aperiodic_response_time;            /* Don't know the purpose, might be for those aperiodic tasks :P */
int                 task_assignment_history[MAX_TASKS]; /* For recording the processor assignment history */
int                 task_release[TICKS];                /* For recording the task release time  */

TCB*     p_ready_queue;
TCB*     ap_ready_queue;
TCB*     fifo_ready_queue;
TCB*     _kernel_runtsk[PROCESSOR_NUM]     = {NULL};
TCB*     _kernel_runtsk_pre[PROCESSOR_NUM] = {NULL};
unsigned p_tasks;
unsigned ap_tasks;

/* Variables for overheads estimation */
unsigned long long migration;            // For recording the migration,but not used in uniprocessor case
unsigned long long overhead_dl;          // For recording the overhead
unsigned long long overhead_dl_max;
unsigned long long overhead_dl_total;
unsigned long long overhead_alpha;
unsigned long long overhead_alpha_max;
unsigned long long overhead_alpha_total;

int has_new_task;
int has_new_instance;
int has_task_finished;

int main ( int argc, char *argv[] )
{
    FILE              *p_cfp;
#ifdef AP_ALSO
    FILE              *ap_cfp;
#endif
    FILE*              ovhd_dl_max_fp;
    FILE*              ovhd_al_max_fp;
    FILE*              ovhd_dl_total_fp;
    FILE*              ovhd_al_total_fp;
    FILE*              migration_fp;
    int                i, j;
    char               buf[BUFSIZ];
    unsigned           task_id, task_wcet, task_et, prev_task_id=MAX_TASKS;
    unsigned long long task_period, task_req_tim;

    if(argc < 2){log_once(NULL,"Argument Error\n");exit(-1);}

    // Initialize the signal handler
    signal_init();

    // Reset the task_release array 
    memset(task_release,0,sizeof(int)*TICKS);

    if ( (p_cfp = fopen ( argv[1], "r") ) == NULL ) {
        printf ( "Cannot open \"%s\"\n",argv[1] );
        return 1;
    }

#ifdef AP_ALSO
    if ( (ap_cfp = fopen ( "aperiodic.cfg", "r") ) == NULL ) {
        printf ( "Cannot open \"aperiodic.cfg\"\n" );
        return 1;
    }
#endif

    if ( ( ovhd_dl_max_fp = fopen ( "ovhd_dl_max.csv", "a" ) ) == NULL ) {
        printf ( "Cannot open \"ovhd_dl_max.csv\"\n" );
        return 1;
    }
    fseek ( ovhd_dl_max_fp, 0, SEEK_END );

    if ( ( ovhd_dl_total_fp = fopen ( "ovhd_dl_total.csv", "a" ) ) == NULL ) {
        printf ( "Cannot open \"ovhd_dl_total.csv\"\n" );
        return 1;
    }
    fseek ( ovhd_dl_total_fp, 0, SEEK_END );

    if ( ( ovhd_al_max_fp = fopen ( "ovhd_al_max.csv", "a" ) ) == NULL ) {
      printf ( "Cannot open \"ovhd_al_max.csv\"\n" );
      return 1;
    }
    fseek ( ovhd_al_max_fp, 0, SEEK_END );

    if ( ( ovhd_al_total_fp = fopen ( "ovhd_al_total.csv", "a" ) ) == NULL ) {
      printf ( "Cannot open \"ovhd_al_total.csv\"\n" );
      return 1;
    }
    fseek ( ovhd_al_total_fp, 0, SEEK_END );

    if ( ( migration_fp = fopen( "migration.csv", "a" ) ) == NULL){
        printf ( "Cannot open \"migration.csv\"\n" );
        return 1;
    }
    fseek ( migration_fp, 0, SEEK_END );
  

    for ( i = 0; i < MAX_TASKS; i++ ) {
        for ( j = 0; j < MAX_INSTANCES; j++ ) {
            req_tim[i][j] = 0xFFFFFFFFFFFFFFFF;
        }
    }

  /* Inputting periodic tasks' information */
    while(fgets ( buf, BUFSIZ, p_cfp ) != NULL) 
    {
        if(buf[0] == '#'){continue;}
        sscanf ( buf, "%d\t%d\t%d\t%llu\t%llu\n", &task_id, &task_wcet, &task_et, &task_period, &task_req_tim );

        wcet[task_id]                        = task_wcet;
        et[task_id][inst_no[task_id]]        = task_wcet;    /* At the moment, WCET is set for periodic tasks */
        req_tim[task_id][inst_no[task_id]++] = task_req_tim;

        task_release[task_req_tim]=1;

        if ( task_id != prev_task_id ) 
        {
            phase[task_id]    = task_req_tim;
            prev_task_id      = task_id;
            periodic[task_id] = 1;
            period[task_id]   = task_period;
            job_util[task_id] = (double)task_wcet / (double)task_period;
            p_util            += job_util[task_id];
            p_tasks++;
        }
    }

    for ( i = 0; i < MAX_TASKS; i++ )
        inst_no[i] = 0;

#ifdef AP_ALSO
  /* Inputting aperiodic tasks' information */
    while ( fgets ( buf, BUFSIZ, ap_cfp ) != NULL ) 
    {
        double dummy1, budget;
        int    dummy2;

        if ( strncmp ( buf, "#average", 8 ) == 0 ) 
        {
            sscanf ( buf, "#average_interval=%lf, average_wcet=%d, average_et=%lf", &dummy1, &dummy2, &budget );
            C0 = cbs_Q = (unsigned)ceil (budget/C0_cbs_Q_DIV);
        }
        if ( buf[0] == '#' )
        {
            continue;
        }
        sscanf ( buf, "%d\t%d\t%d\t%llu\t%llu\n", &task_id, &task_wcet, &task_et, &task_period, &task_req_tim );

        wcet[task_id]                        = task_wcet;
        et[task_id][inst_no[task_id]]        = task_et;
        req_tim[task_id][inst_no[task_id]++] = task_req_tim;
        ap_tasks++;
    }
    fclose ( ap_cfp );
#endif

    log_once(NULL,"===============================================================\n");
    log_once(NULL,"            Working on Ultilization %.5f       \n",p_util);
    log_once(NULL,"===============================================================\n");

    fclose ( p_cfp );
    if ( p_util > UTIL_UPPERBOUND ) 
    {
        log_once(NULL,"Cannot execute tasks over %2.2f%% utilization\nCurrent ultilization: %2.2f%%\n",UTIL_UPPERBOUND*100,p_util);
        printf ( "Cannot execute tasks over %2.2f%% utilization\nCurrent ultilization: %2.2f%%\n",UTIL_UPPERBOUND,p_util);
        return 0;
    }

/***************************************************************/
// All aperiodic tasks oredered by when it comes.
/***************************************************************/

#ifdef RM

    log_once(NULL,"RN\n");
    
    run_simulation("RM",RM_sa);
  
    fprintf(migration_fp,     ", %llu", migration);
    fprintf(ovhd_dl_max_fp,   ", %llu", overhead_dl_max );
    fprintf(ovhd_dl_total_fp, ", %llu", overhead_dl_total );
    fprintf(ovhd_al_max_fp,   ", %llu", overhead_alpha_max );
    fprintf(ovhd_al_total_fp, ", %llu", overhead_alpha_total );


#else
    
    fprintf(migration_fp      , "," );
    fprintf(ovhd_dl_max_fp    , "," );
    fprintf(ovhd_dl_total_fp  , "," );
    fprintf(ovhd_al_max_fp    , "," );
    fprintf(ovhd_al_total_fp  , "," );

#endif

#ifdef LSF
    log_once(NULL,"LSF\n");

    run_simulation("LSF",LSF_sa);
    
    fprintf(migration_fp,     ", %llu", migration);
    fprintf(ovhd_dl_max_fp,   ", %llu", overhead_dl_max );
    fprintf(ovhd_dl_total_fp, ", %llu", overhead_dl_total );
    fprintf(ovhd_al_max_fp,   ", %llu", overhead_alpha_max );
    fprintf(ovhd_al_total_fp, ", %llu", overhead_alpha_total );


#else
    
    fprintf(migration_fp    , "," );
    fprintf(ovhd_dl_max_fp  , "," );
    fprintf(ovhd_dl_total_fp, "," );
    fprintf(ovhd_al_max_fp  , "," );
    fprintf(ovhd_al_total_fp, "," );

#endif

#ifdef EDF
    log_once(NULL,"EDF\n");

    run_simulation("EDF",EDF_sa);

    fprintf(migration_fp,     ", %llu", migration);
    fprintf(ovhd_dl_max_fp,   ", %llu", overhead_dl_max );
    fprintf(ovhd_dl_total_fp, ", %llu", overhead_dl_total );
    fprintf(ovhd_al_max_fp,   ", %llu", overhead_alpha_max );
    fprintf(ovhd_al_total_fp, ", %llu", overhead_alpha_total );
#else
    
    fprintf(migration_fp    , "," );
    fprintf(ovhd_dl_max_fp  , "," );
    fprintf(ovhd_dl_total_fp, "," );
    fprintf(ovhd_al_max_fp  , "," );
    fprintf(ovhd_al_total_fp, "," );
#endif

#ifdef LLREF
    log_once(NULL,"%s\n",LLREF_sa.name);

    run_simulation(LLREF_sa.name,LLREF_sa);
  
    fprintf(migration_fp,     ", %llu", migration);
    fprintf(ovhd_dl_max_fp,   ", %llu", overhead_dl_max );
    fprintf(ovhd_dl_total_fp, ", %llu", overhead_dl_total );
    fprintf(ovhd_al_max_fp,   ", %llu", overhead_alpha_max );
    fprintf(ovhd_al_total_fp, ", %llu", overhead_alpha_total );
#else    

    fprintf(migration_fp    , "," );
    fprintf(ovhd_dl_max_fp  , "," );
    fprintf(ovhd_dl_total_fp, "," );
    fprintf(ovhd_al_max_fp  , "," );
    fprintf(ovhd_al_total_fp, "," );
#endif

#ifdef RUN
    log_once(NULL,"%s\n",RUN_sa.name);

    run_simulation(RUN_sa.name,RUN_sa);
  
    fprintf(migration_fp,     ", %llu", migration);
    fprintf(ovhd_dl_max_fp,   ", %llu", overhead_dl_max );
    fprintf(ovhd_dl_total_fp, ", %llu", overhead_dl_total );
    fprintf(ovhd_al_max_fp,   ", %llu", overhead_alpha_max );
    fprintf(ovhd_al_total_fp, ", %llu", overhead_alpha_total );
#else    

    fprintf(migration_fp    , "," );
    fprintf(ovhd_dl_max_fp  , "," );
    fprintf(ovhd_dl_total_fp, "," );
    fprintf(ovhd_al_max_fp  , "," );
    fprintf(ovhd_al_total_fp, "," );
#endif

#ifdef HEDF
    log_once(NULL,"%s\n",HEDF_sa.name);

    run_simulation(HEDF_sa.name,HEDF_sa);
  
    fprintf(migration_fp,     ", %llu", migration);
    fprintf(ovhd_dl_max_fp,   ", %llu", overhead_dl_max );
    fprintf(ovhd_dl_total_fp, ", %llu", overhead_dl_total );
    fprintf(ovhd_al_max_fp,   ", %llu", overhead_alpha_max );
    fprintf(ovhd_al_total_fp, ", %llu", overhead_alpha_total );
#else    

    fprintf(migration_fp    , "," );
    fprintf(ovhd_dl_max_fp  , "," );
    fprintf(ovhd_dl_total_fp, "," );
    fprintf(ovhd_al_max_fp  , "," );
    fprintf(ovhd_al_total_fp, "," );
#endif

    fclose(migration_fp );
    fclose(ovhd_dl_max_fp );
    fclose(ovhd_dl_total_fp );
    fclose(ovhd_al_max_fp );
    fclose(ovhd_al_total_fp );

    return 0;
}


void run_simulation(char* s,SCHEDULING_ALGORITHM sa)
{
    int  i                             = 0;
    int  processor_id                  = 0;
    void (*scheduling)()               = NULL;
    void (*reorganize_function)(TCB**) = NULL;
    void (*scheduling_initialize)()    = NULL;
    void (*scheduling_exit)()          = NULL;
    TCB  *entry                        = NULL;

    if(sa.scheduling_initialize==NULL || sa.scheduling==NULL || \
       sa.insert_OK==NULL || sa.reorganize_function==NULL || sa.scheduling_exit==NULL)
    {
        return;
    }
    scheduling_initialize = (void (*)())sa.scheduling_initialize;
    scheduling            = (void (*)())sa.scheduling;
    reorganize_function   = (void (*)(TCB**))sa.reorganize_function;
    scheduling_exit       = (void (*)())sa.scheduling_exit;
    current_algorithm     = s;

    Initialize ();
    scheduling_initialize();

    for ( tick = 0; tick < TICKS; ) 
    {
        for ( i = 0; i < MAX_TASKS; i++ ) 
        {
            if ( req_tim[i][inst_no[i]] == tick ) 
            {
                has_new_instance = 1;

                entry = entry_set(i);
                if (periodic[i] == 1) 
                {
                    if(phase[i] == tick){has_new_task = 1;}
                    entry -> a_dl = tick + period[i];
                    insert_queue( &p_ready_queue, entry ,sa.insert_OK);
                }
                else 
                {
                    insert_queue_fifo ( &fifo_ready_queue, entry );
                }
                inst_no[i]++;

                if(multi_TCB_check(s,&p_ready_queue) > 0)
                {
                    log_once(NULL,"multi TCB detected,in run_simulation\n");
                    TCB_list_print(p_ready_queue);
                    kill(getpid(),SIG_SHOULD_NOT_HAPPENED);
                }
            }
        }
        reorganize_function(&p_ready_queue);

        if(ap_ready_queue==NULL && fifo_ready_queue!=NULL) 
        {
            from_fifo_to_ap ();
        }  
        /* Scheduling */
        scheduling();
        /* Tick increment/et decrement and Setting last_empty & used_dl*/
        Tick_inc(s,sa.scheduling_update);

        for(processor_id=0;processor_id<PROCESSOR_NUM;processor_id++)
        {
            if ( _kernel_runtsk[processor_id] != NULL ) 
            {
                if ( _kernel_runtsk[processor_id] -> et == 0 ) 
                {   
                    Job_exit ( s, 1 ,processor_id,sa.job_delete);
                }
            }
        }
    }
    scheduling_exit();
}

void insert_queue( TCB **rq,TCB *entry,void* function)
{
    TCB  *p;
    int (*insert_OK)(TCB* t1,TCB* t2) = NULL;

    if(function==NULL){return;}
    
    insert_OK = (int (*)(TCB*,TCB*)) function;

    //Unchain p from TCB_list
    for(p=*rq;p;p=p->next)
    {
        if(p->tid == entry->tid)
        {
            if(p->et != 0){log_once(NULL,"OOPS!Wrong with %d\n",p->tid);kill(getpid(),SIG_SHOULD_NOT_HAPPENED);}
            memcpy(p,entry,DIFF(TCB,something_else));
            free(entry);

            if(p->prev != NULL){p->prev->next = p->next;}
            else{*rq = p->next;}
            if(p->next != NULL){p->next->prev = p->prev;}
            entry = p;
            entry->prev = entry->next = NULL;
            break;
        }
    }

    // link it back again
    for ( p = *rq; p != NULL; p = p -> next )
    {
        if (insert_OK(entry,p)) 
        {
            if ( p == *rq ) 
            {
                entry -> next = p;
                p -> prev = entry;
                *rq = entry;
            }
            else 
            {
                p -> prev -> next = entry;
                entry -> next = p;
                entry -> prev = p -> prev;
                p -> prev     = entry;
            }
            break;
        }
        if ( p -> next == NULL ) 
        {
            p -> next     = entry;
            entry -> prev = p;
            break;
        }
    }
    if ( *rq == NULL ) 
    {
        insert_OK(entry,*rq);
        *rq = entry;
    }

    return;
}


void insert_queue_fifo ( TCB **rq, TCB *entry )
{
    TCB  *p;

    if ( *rq == NULL ) 
    {
        *rq = entry;
        entry -> next = NULL;
        entry -> prev = NULL;

        return;
    }

    for ( p = *rq; p -> next != NULL; p = p -> next ); /* loop until tail */

    p -> next = entry;
    entry -> next = NULL;
    entry -> prev = p;

    return;
}

void from_fifo_to_ap ( void )
{
    ap_ready_queue   = fifo_ready_queue;
    fifo_ready_queue = fifo_ready_queue -> next;
    ap_ready_queue -> next = ap_ready_queue -> prev = NULL;
    if ( fifo_ready_queue != NULL )
    {
        fifo_ready_queue -> prev = NULL;
    }

    return;
}

void delete_queue ( TCB **rq,TCB* tcb,void* job_delete)
{
    void (*job_delete_function)(TCB*) = NULL;

    if(rq==NULL || *rq==NULL || tcb==NULL){return;}

    // Tell scheduling algorithm this job has finished.
    if(job_delete != NULL)
    {
        job_delete_function = job_delete;
        job_delete_function(tcb);
    }

    return;
}

void Initialize ( void )
{
    int i;

    has_new_instance  = 0;
    has_new_task      = 0;
    has_task_finished = 0;

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        _kernel_runtsk[i] = _kernel_runtsk_pre[i] = NULL;
    }

    for ( i = 0; i < MAX_TASKS; i++ ) 
    {
        exec_times[i] = 0;
        inst_no[i]    = 0;
        job_util[i]   = 0.0;
        task_assignment_history[i] = -1;
    }

    p_ready_queue    = NULL;
    ap_ready_queue   = NULL;
    fifo_ready_queue = NULL;

    migration            = \
    overhead_dl          = \
    overhead_alpha       = \
    overhead_dl_max      = \
    overhead_alpha_max   = \
    overhead_dl_total    = \
    overhead_alpha_total = 0;

    return;
}

TCB *entry_set ( int i)
{
    TCB *entry;

    entry = (TCB*)malloc(sizeof(TCB));
    entry->tid             = i;
    entry->inst_no         = inst_no[i];
    entry->req_tim         = tick;
    entry->et              = et[i][inst_no[i]];
    entry->wcet            = wcet[i];
    entry->initial_et      = et[i][inst_no[i]];
    entry->next = entry -> prev = NULL;
    entry->something_else  = NULL;
    entry->a_dl            = (unsigned long long)0xFFFFFFFFFFFFFFFF;  // dummy necessary
  
    return entry;
}

void Tick_inc (char* s,void* job_update)
{
    int i;
    void (*scheduling_update)() = NULL;

    Overhead_Record();
    
    tick++;

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        if ( _kernel_runtsk[i] != NULL ) 
        {
            (_kernel_runtsk[i]->et)--;
        }   
    }

    if(job_update != NULL)
    {
        scheduling_update = job_update;
        scheduling_update();
    }

    //Here check the deadline miss event.
    for(i=0;i<PROCESSOR_NUM;i++)
    {
        if(_kernel_runtsk[i] != NULL) 
        {
            if((_kernel_runtsk[i]->a_dl)-tick < (_kernel_runtsk[i]->et))
            {
                deadline_miss_log(s,_kernel_runtsk[i]);
            }
        }
    }
    return;
}


void Overhead_Record ( void )
{

    if(overhead_dl_max < overhead_dl)
    {
        overhead_dl_max = overhead_dl;
    }
  
    if(overhead_alpha_max < overhead_alpha)
    {
        overhead_alpha_max = overhead_alpha;
    }

    overhead_dl_total    += overhead_dl;
    overhead_alpha_total += overhead_alpha;

    overhead_dl = overhead_alpha = 0;
  
    return;
}


void Job_exit ( const char *s, int rr, int processor_id,void* job_delete) /* rr: resource reclaiming */
{

    if(_kernel_runtsk[processor_id] == NULL){return;}

    if ( _kernel_runtsk[processor_id] -> a_dl < tick ) 
    {
        deadline_miss_log(s,_kernel_runtsk[processor_id]);
    }

    exec_times[ _kernel_runtsk[processor_id] -> tid]++;

    if (periodic[_kernel_runtsk[processor_id]->tid] == 0) 
    {
        aperiodic_exec_times++;
        aperiodic_total_et += tick-(_kernel_runtsk[processor_id]->req_tim);
        aperiodic_response_time = ((double)aperiodic_total_et)/((double)aperiodic_exec_times);
    }

    // Delete from queue
    if (periodic[_kernel_runtsk[processor_id]->tid] == 1)
    {
        delete_queue ( &p_ready_queue,_kernel_runtsk[processor_id],job_delete);
    }
    else
    {
        delete_queue ( &ap_ready_queue,_kernel_runtsk[processor_id],job_delete);
    }
    _kernel_runtsk[processor_id] = NULL;
    has_task_finished = 1;

    return;
}

void deadline_miss_log(const char* s,TCB* tcb)
{

    fprintf( stderr, "\n# MISS: %s: tick=%lld: DL=%lld: TID=%d: INST_NO=%d, WCET=%d, ET=%d, req_tim=%lld\n",
           s, tick, tcb->a_dl, tcb->tid, tcb->inst_no, wcet[tcb->tid], tcb->initial_et, tcb->req_tim );
    printf( "\n# MISS: %s: tick=%lld: DL=%lld: TID=%d: INST_NO=%d\n",
           s, tick, tcb->a_dl, tcb->tid, tcb->inst_no);
}

int multi_TCB_check(char* s,TCB** rq)
{
    int check_result;

    TCB* tcb_p = NULL;
    TCB* tcb_q = NULL;

    if(rq==NULL || *rq==NULL){log_once(NULL,"rq or *rq is NULL,in multi_TCB_check\n");return -1;}

    check_result = 0;
    for(tcb_p=*rq;tcb_p;tcb_p=tcb_p->next)
    {
        for(tcb_q=tcb_p->next;tcb_q;tcb_q=tcb_q->next)
        {
            if(tcb_p->tid == tcb_q->tid)
            {
                check_result++;
            }
        }
    }

    return check_result;
}


void TCB_list_print(TCB* TCB_list)
{
    TCB* tcb = NULL;

    if(TCB_list == NULL){log_once(NULL,"TCB_list is NULL,in TCB_list_print\n");return;}

    for(tcb=TCB_list;tcb;tcb=tcb->next)
    {
        log_c("job: %p\t%d\t%u\t%llu\t%p\n",tcb,tcb->tid,tcb->et,tcb->a_dl,tcb->something_else);
    }
}
