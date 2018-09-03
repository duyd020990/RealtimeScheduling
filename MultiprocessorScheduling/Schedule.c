#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "Schedule.h"
#include "signal.h"

//Scehduling algorithms
//#include "RM.h"
//#include "LSF.h"
//#include "EDF.h"
#include "LLREF.h"

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

/***********************************************************************************************************/
/* Variables                                                                                               */
/***********************************************************************************************************/

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

TCB*     p_ready_queue;
TCB*     ap_ready_queue;
TCB*     fifo_ready_queue;
TCB*     _kernel_runtsk[PROCESSOR_NUM]     = {NULL};
TCB*     _kernel_runtsk_pre[PROCESSOR_NUM] = {NULL};
unsigned p_tasks;
unsigned ap_tasks;

/* Variables for overheads estimation */
unsigned long long migration;
unsigned long long overhead_dl;
unsigned long long overhead_dl_max;
unsigned long long overhead_dl_total;
unsigned long long overhead_alpha;
unsigned long long overhead_alpha_max;
unsigned long long overhead_alpha_total;

int has_new_task;
int has_new_instance;


/* Variables for TBS95 */////////////////One day,I will remove these things
unsigned long long  di_1, fi_1;

/* Variables for VRA */
unsigned long long  max_used_dl;
unsigned long long  used_dl[PROCESSOR_NUM][TICKS];
unsigned int        last_empty[PROCESSOR_NUM];                         /* 最後の空スロット番号                           */


int main ( int argc, char *argv[] )
{
    FILE              *p_cfp;
#ifdef AP_ALSO
    FILE              *ap_cfp;
#endif
    FILE              *ovhd_dl_max_fp, *ovhd_al_max_fp, *ovhd_dl_total_fp, *ovhd_al_total_fp;
    int                i, j;
    char               buf[BUFSIZ];
    unsigned           task_id, task_wcet, task_et, prev_task_id=MAX_TASKS;
    unsigned long long task_period, task_req_tim;

    signal_init();

#ifdef LLREF
    memset(release_time,0,sizeof(int)*TICKS);
#endif

    if ( (p_cfp = fopen ( "periodic.cfg", "r") ) == NULL ) {
        printf ( "Cannot open \"periodic.cfg\"\n" );
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
  

    for ( i = 0; i < MAX_TASKS; i++ ) {
        for ( j = 0; j < MAX_INSTANCES; j++ ) {
            req_tim[i][j] = 0xFFFFFFFFFFFFFFFF;
        }
    }

  /* Inputting periodic tasks' information */
    while ( fgets ( buf, BUFSIZ, p_cfp ) != NULL ) 
    {
        if ( buf[0] == '#' )
        {
            continue;
        }
        sscanf ( buf, "%d\t%d\t%d\t%llu\t%llu\n", &task_id, &task_wcet, &task_et, &task_period, &task_req_tim );

        wcet[task_id]                        = task_wcet;
        et[task_id][inst_no[task_id]]        = task_wcet;    /* At the moment, WCET is set for periodic tasks */
        req_tim[task_id][inst_no[task_id]++] = task_req_tim;
#ifdef LLREF
        release_time[task_req_tim]=1;
#endif
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

    fclose ( p_cfp );

    if ( p_util > UTIL_UPPERBOUND ) 
    {
        printf ( "Cannot execute tasks over %2f%% utilization\n",UTIL_UPPERBOUND*100 );
        return 0;
    }

/***************************************************************/
// All periodic tasks ordered by deadline, this means it use EDF algorithm,
// All aperiodic tasks oredered by when it comes.
/***************************************************************/

/****************************************************************************************************************/
/* RM                                                                                                          */
/****************************************************************************************************************/
#ifdef RM

    run_simulation("RM",RM_sa);
  
    printf ( ", %lf", aperiodic_response_time );
  
    fprintf ( ovhd_dl_max_fp,   ", %llu", overhead_dl_max );
    fprintf ( ovhd_dl_total_fp, ", %llu", overhead_dl_total );
    fprintf ( ovhd_al_max_fp,   ", %llu", overhead_alpha_max );
    fprintf ( ovhd_al_total_fp, ", %llu", overhead_alpha_total );


#else
    printf ( ", ");

    fprintf ( ovhd_dl_max_fp, "," );
    fprintf ( ovhd_dl_total_fp, "," );
    fprintf ( ovhd_al_max_fp, "," );
    fprintf ( ovhd_al_total_fp, "," );

#endif

#ifdef LSF

    run_simulation("LSF",LSF_sa);
    
    printf ( ", %lf", aperiodic_response_time );
  
    fprintf ( ovhd_dl_max_fp,   ", %llu", overhead_dl_max );
    fprintf ( ovhd_dl_total_fp, ", %llu", overhead_dl_total );
    fprintf ( ovhd_al_max_fp,   ", %llu", overhead_alpha_max );
    fprintf ( ovhd_al_total_fp, ", %llu", overhead_alpha_total );


#else
    printf ( ", ");

    fprintf ( ovhd_dl_max_fp  , "," );
    fprintf ( ovhd_dl_total_fp, "," );
    fprintf ( ovhd_al_max_fp  , "," );
    fprintf ( ovhd_al_total_fp, "," );

#endif

#ifdef EDF

    run_simulation("EDF",EDF_sa);

    printf ( ", %lf", aperiodic_response_time );
  
    fprintf ( ovhd_dl_max_fp,   ", %llu", overhead_dl_max );
    fprintf ( ovhd_dl_total_fp, ", %llu", overhead_dl_total );
    fprintf ( ovhd_al_max_fp,   ", %llu", overhead_alpha_max );
    fprintf ( ovhd_al_total_fp, ", %llu", overhead_alpha_total );
#else
    printf ( ", ");

    fprintf ( ovhd_dl_max_fp  , "," );
    fprintf ( ovhd_dl_total_fp, "," );
    fprintf ( ovhd_al_max_fp  , "," );
    fprintf ( ovhd_al_total_fp, "," );
#endif

#ifdef LLREF

    run_simulation("LLREF",LLREF_sa);

    printf ( ", %lf", aperiodic_response_time );
  
    fprintf ( ovhd_dl_max_fp,   ", %llu", overhead_dl_max );
    fprintf ( ovhd_dl_total_fp, ", %llu", overhead_dl_total );
    fprintf ( ovhd_al_max_fp,   ", %llu", overhead_alpha_max );
    fprintf ( ovhd_al_total_fp, ", %llu", overhead_alpha_total );
#else    
    printf ( ", ");

    fprintf ( ovhd_dl_max_fp  , "," );
    fprintf ( ovhd_dl_total_fp, "," );
    fprintf ( ovhd_al_max_fp  , "," );
    fprintf ( ovhd_al_total_fp, "," );
#endif

    fclose ( ovhd_dl_max_fp );
    fclose ( ovhd_dl_total_fp );
    fclose ( ovhd_al_max_fp );
    fclose ( ovhd_al_total_fp );

    return 0;
}


void run_simulation(char* s,SCHEDULING_ALGORITHM sa)
{
    int  i                             = 0;
    int  processor_id                  = 0;
    void (*scheduling)()               = NULL;
    void (*reorganize_function)(TCB**) = NULL;
    void (*scheduling_initialize)()    = NULL;
    TCB  *entry                        = NULL;

    if(sa.scheduling_initialize==NULL || sa.scheduling==NULL || \
       sa.insert_OK==NULL || sa.reorganize_function==NULL)
    {
        return;
    }
    scheduling_initialize = (void (*)())sa.scheduling_initialize;
    scheduling            = (void (*)())sa.scheduling;
    reorganize_function   = (void (*)(TCB**))sa.reorganize_function;
    
    Initialize ();
    scheduling_initialize();

    for ( tick = 0; tick < TICKS; ) 
    {

        for ( i = 0; i < MAX_TASKS; i++ ) 
        {
            if ( req_tim[i][inst_no[i]] == tick ) 
            {
                entry = entry_set ( i );
                if ( periodic[i] == 1 ) 
                {
                    entry -> a_dl = tick + period[i];
                    insert_queue( &p_ready_queue, entry ,sa.insert_OK);
                    if(phase[i] == tick)
                    {
                        has_new_task = 1;
                    }
                }
                else 
                {
                    insert_queue_fifo ( &fifo_ready_queue, entry );
                }
                inst_no[i]++;
                has_new_instance = 1;
            }
        }
        reorganize_function(&p_ready_queue);

        if ( ap_ready_queue == NULL && fifo_ready_queue != NULL ) 
        {
            from_fifo_to_ap ();
        }  
        
        /* Scheduling */
        scheduling();
        /* Tick increment/et decrement and Setting last_empty & used_dl*/
        Tick_inc ();
        //fprintf(stderr, "Tick: %d\n", tick);
        //getchar();
        for(processor_id=0;processor_id<PROCESSOR_NUM;processor_id++)
        {
            if ( _kernel_runtsk[processor_id] != NULL ) 
            {
                if ( _kernel_runtsk[processor_id] -> et == 0 ) 
                {   
                    Job_exit ( s, 1 ,processor_id);
                }
            }
        }
    }
}

void insert_queue( TCB **rq,TCB *entry,void* function)
{
    TCB  *p;
    int (*insert_OK)(TCB* t1,TCB* t2) = NULL;

    if(function==NULL)
    {
        return;
    }
    insert_OK = (int (*)(TCB*,TCB*)) function;

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

void delete_queue ( TCB **rq,TCB* tcb)
{
    TCB* h_p;

    if(rq==NULL || rq==NULL || *rq==NULL){return;}

    h_p = tcb->prev;
    if(h_p == NULL)
    {
        *rq = (*rq) -> next; 
        if(*rq!=NULL)
        {
            (*rq)->prev = NULL;
        }
    }
    else if(tcb->next == NULL)
    {
        tcb->prev->next = NULL;
    }
    else
    {
        h_p->next = tcb->next;
        tcb->next->prev = h_p;
    }

    if(tcb->something_else!=NULL){free(tcb->something_else);}
    free ( tcb );

    return;
}

void Initialize ( void )
{
    int i,j;

    has_new_instance        = 0;
    has_new_task            = 0;

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        _kernel_runtsk[i] = _kernel_runtsk_pre[i] = NULL;
    }

    for ( i = 0; i < MAX_TASKS; i++ ) 
    {
        exec_times[i] = 0;
        inst_no[i] = 0;
        job_util[i] = 0.0;
    }

    p_ready_queue    = NULL;
    ap_ready_queue   = NULL;
    fifo_ready_queue = NULL;

    di_1 = 0;
    fi_1 = 0;
    for(j=0;j<PROCESSOR_NUM;j++)
    {
        for ( i = 0; i < TICKS; i++ ) 
        {
            used_dl[j][i] = 0;
        }   
    }
    for(i=0;i<PROCESSOR_NUM;i++)
    {
        last_empty[i]  = 0;
    }
    max_used_dl = 0;

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

    entry =  malloc ( sizeof ( TCB ) );
    entry -> tid             = i;
    entry -> inst_no         = inst_no[i];
    entry -> req_tim         = tick;
    entry -> et              = et[i][inst_no[i]];
    entry -> wcet            = wcet[i];
    entry -> initial_et      = et[i][inst_no[i]];
    entry -> next = entry -> prev = NULL;
    entry -> something_else = NULL;
    entry -> a_dl = (unsigned long long)0xFFFFFFFFFFFFFFFF;  // dummy necessary
  
    return entry;
}

void Tick_inc ( )
{
    int i;

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        if ( _kernel_runtsk[i] == NULL )
        {
            last_empty[i] = tick;
        }
        else
        {
            used_dl[i][tick] = _kernel_runtsk[i] -> a_dl;
        }
      
        Overhead_Record ( );
    }
    
    tick++;

    for(i=0;i<PROCESSOR_NUM;i++)
    {
        if ( _kernel_runtsk[i] != NULL ) 
        {
            --(_kernel_runtsk[i] -> et);
        }   
    }
    return;
}


void Overhead_Record ( void )
{

    if ( overhead_dl_max < overhead_dl )
    {
      overhead_dl_max = overhead_dl;
    }
  
    if ( overhead_alpha_max < overhead_alpha )
    {
      overhead_alpha_max = overhead_alpha;
    }

    overhead_dl_total    += overhead_dl;
    overhead_alpha_total += overhead_alpha;

    overhead_dl = overhead_alpha = 0;
  
    return;
}


void Job_exit ( const char *s, int rr, int processor_id) /* rr: resource reclaiming */
{
    if(_kernel_runtsk[processor_id] == NULL)
    {
        return;
    }

    if ( _kernel_runtsk[processor_id] -> a_dl < tick ) 
    {
        fprintf( stderr, "\n# MISS: %s: tick=%lld: DL=%lld: TID=%d: INST_NO=%d, WCET=%d, ET=%d, req_tim=%lld\n",
           s, tick, _kernel_runtsk[processor_id]->a_dl, _kernel_runtsk[processor_id]->tid, _kernel_runtsk[processor_id]->inst_no,
           wcet[_kernel_runtsk[processor_id]->tid], _kernel_runtsk[processor_id]->initial_et, _kernel_runtsk[processor_id]->req_tim );
        printf( "\n# MISS: %s: tick=%lld: DL=%lld: TID=%d: INST_NO=%d\n",
           s, tick, _kernel_runtsk[processor_id]->a_dl,  _kernel_runtsk[processor_id]->tid, _kernel_runtsk[processor_id]->inst_no);
    }

    exec_times[ _kernel_runtsk[processor_id] -> tid]++;

    if ( periodic[_kernel_runtsk[processor_id]->tid] == 0 ) 
    {
        aperiodic_exec_times++;
        aperiodic_total_et += tick - _kernel_runtsk[processor_id] -> req_tim;
        aperiodic_response_time = (double)aperiodic_total_et / (double)aperiodic_exec_times;
    }

    if ( periodic[_kernel_runtsk[processor_id]->tid] == 1 )
    {
        delete_queue ( &p_ready_queue,_kernel_runtsk[processor_id]);
    }
    else
    {
        delete_queue ( &ap_ready_queue,_kernel_runtsk[processor_id]);
    }
    _kernel_runtsk[processor_id] = NULL;
    return;
}

