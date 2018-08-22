#include "Schedule.h"

int main ( int argc, char *argv[] )
{

  FILE              *p_cfp, *ap_cfp;
  FILE              *ovhd_dl_max_fp, *ovhd_al_max_fp, *ovhd_dl_total_fp, *ovhd_al_total_fp, *ovhd_total_max_fp, *ovhd_total_full_fp;
  int                i, j;
  char               buf[BUFSIZ];
  unsigned int       task_id, task_wcet, task_et, prev_task_id=MAX_TASKS;
  unsigned long long task_period, task_req_tim;
  TCB               *entry;

  if ( (p_cfp = fopen ( "periodic.cfg", "r") ) == NULL ) {
    printf ( "Cannot open \"periodic.cfg\"\n" );
    return 1;
  }
  if ( (ap_cfp = fopen ( "aperiodic.cfg", "r") ) == NULL ) {
    printf ( "Cannot open \"aperiodic.cfg\"\n" );
    return 1;
  }

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
  
  if ( ( ovhd_total_max_fp = fopen ( "ovhd_total_max.csv", "a" ) ) == NULL ) {
    printf ( "Cannot open \"ovhd_total_max.csv\"\n" );
    return 1;
  }
  fseek ( ovhd_total_max_fp, 0, SEEK_END );
  
  if ( ( ovhd_total_full_fp = fopen ( "ovhd_total_full.csv", "a" ) ) == NULL ) {
    printf ( "Cannot open \"ovhd_total_full.csv\"\n" );
    return 1;
  }
  fseek ( ovhd_total_full_fp, 0, SEEK_END );

  for ( i = 0; i < MAX_TASKS; i++ ) {
    for ( j = 0; j < MAX_INSTANCES; j++ ) {
      req_tim[i][j] = 0xFFFFFFFFFFFFFFFF;
    }
  }

  /* Inputting periodic tasks' information */
  while ( fgets ( buf, BUFSIZ, p_cfp ) != NULL ) {
    if ( buf[0] == '#' )
      continue;
    sscanf ( buf, "%d\t%d\t%d\t%ld\t%ld\n", &task_id, &task_wcet, &task_et, &task_period, &task_req_tim );

    wcet[task_id]                        = task_wcet;
    et[task_id][inst_no[task_id]]        = task_wcet;    /* At the moment, WCET is set for periodic tasks */
    req_tim[task_id][inst_no[task_id]++] = task_req_tim;

    if ( task_id != prev_task_id ) {
      phase[task_id]    = task_req_tim;
      prev_task_id      = task_id;
      periodic[task_id] = 1;
      period[task_id]   = task_period;
      p_util += (double)task_wcet / (double)task_period;
      p_tasks++;
    }
  }

  for ( i = 0; i < MAX_TASKS; i++ )
    inst_no[i] = 0;

  /* Imputting aperiodic tasks' information */
  while ( fgets ( buf, BUFSIZ, ap_cfp ) != NULL ) {

    double dummy1, budget;
    int    dummy2;

    if ( strncmp ( buf, "#average", 8 ) == 0 ) {
      sscanf ( buf, "#average_interval=%lf, average_wcet=%ld, average_et=%lf", &dummy1, &dummy2, &budget );
      C0 = cbs_Q = (unsigned int)ceil (budget/C0_cbs_Q_DIV);
    }
    if ( buf[0] == '#' )
      continue;
    sscanf ( buf, "%d\t%d\t%d\t%ld\t%ld\n", &task_id, &task_wcet, &task_et, &task_period, &task_req_tim );

    wcet[task_id]                        = task_wcet;
    et[task_id][inst_no[task_id]]        = task_et;
	//overhead_total += ASSIGN + MEM + MEM + IADD + IADD + FMUL + FMUL; // overhead for execution time prediction
	// Calculate PET based-on historical PE and ET
	if (inst_no[task_id] == 0){
		pre_et[task_id][inst_no[task_id]] = wcet[task_id];
	}
	else {
		pre_et[task_id][inst_no[task_id]] = pre_alpha*pre_et[task_id][inst_no[task_id] - 1] + (1-pre_alpha)*et[task_id][inst_no[task_id] - 1];
	}
	
	// Calculate PET based-on historical ET
	//if (inst_no[task_id] == 0){
	//	prev_sum_et[task_id][inst_no[task_id]] = 0;
	//	pre_et[task_id][inst_no[task_id]] = wcet[task_id];
	//}
	//else {
		//overhead_total += ASSIGN + ASSIGN + MEM + MEM + IADD + IADD + FDIV; // overhead for execution time prediction
	//	prev_sum_et[task_id][inst_no[task_id]] = prev_sum_et[task_id][inst_no[task_id] - 1] + et[task_id][inst_no[task_id] - 1];
	//	pre_et[task_id][inst_no[task_id]] = prev_sum_et[task_id][inst_no[task_id]] / inst_no[task_id];
	//}
    req_tim[task_id][inst_no[task_id]++] = task_req_tim;
    ap_tasks++;
  }

  fclose ( p_cfp );
  fclose ( ap_cfp );

  if ( p_util > 1.0 ) {
    printf ( "Cannot execute tasks over 100%% utilization\n" );
    return;
  }


/****************************************************************************************************************/
/* TBS                                                                                                          */
/****************************************************************************************************************/
#if TBS == 1

  Initialize ();

  for ( tick = 0; tick < TICKS; ) {
    for ( i = 0; i < MAX_TASKS; i++ ) {
      if ( req_tim[i][inst_no[i]] == tick ) {
	entry = entry_set ( i );

	if ( periodic[i] == 1 ) {
	  entry -> a_dl = tick + period[i];
	  insert_queue ( &p_ready_queue, entry );
	}
	else {
	  insert_queue_fifo ( &fifo_ready_queue, entry );
	}
	inst_no[i]++;
      }
    }

    if ( ap_ready_queue == NULL && fifo_ready_queue != NULL ) {
      from_fifo_to_ap ();

      DL_set_TBS ( ap_ready_queue, wcet[ap_ready_queue -> tid] );
    }

    /* Scheduling */
    Scheduling ();

    /* Tick increment/et decrement and Setting last_empty & used_dl*/
    Tick_inc ();

    if ( _kernel_runtsk != NULL ) {
      if ( _kernel_runtsk -> et == 0 ) {
	Job_exit ( "TBS", 1 );
      }
    }
  }

  printf ( ", %lf", aperiodic_response_time );

  fprintf ( ovhd_dl_max_fp, ", %d", overhead_dl_max );
  fprintf ( ovhd_dl_total_fp, ", %d", overhead_dl_total );
  fprintf ( ovhd_al_max_fp, ", %d", overhead_alpha_max );
  fprintf ( ovhd_al_total_fp, ", %d", overhead_alpha_total );
  fprintf ( ovhd_total_max_fp, ", %d", overhead_total_max );
  fprintf ( ovhd_total_full_fp, ", %d", overhead_total_full );


#else
  printf ( ", ", aperiodic_response_time );

  fprintf ( ovhd_dl_max_fp, "," );
  fprintf ( ovhd_dl_total_fp, "," );
  fprintf ( ovhd_al_max_fp, "," );
  fprintf ( ovhd_al_total_fp, "," );
  fprintf ( ovhd_total_max_fp, "," );
  fprintf ( ovhd_total_full_fp, "," );
  

#endif

/****************************************************************************************************************/
/* TBS+VRA                                                                                                      */
/****************************************************************************************************************/
#if TBS_VRA == 1

  Initialize ();

  for ( tick = 0; tick < TICKS; ) {
    for ( i = 0; i < MAX_TASKS; i++ ) {
      if ( req_tim[i][inst_no[i]] == tick ) {
	entry = entry_set ( i );

	if ( periodic[i] == 1 ) {
	  entry -> a_dl = tick + period[i];
	  insert_queue ( &p_ready_queue, entry );
	}
	else {
	  insert_queue_fifo ( &fifo_ready_queue, entry );
	}
	inst_no[i]++;
      }
    }

    if ( ap_ready_queue == NULL && fifo_ready_queue != NULL ) {
      from_fifo_to_ap ();

      DL_set_TBS_vra ( ap_ready_queue, wcet[ap_ready_queue -> tid]);
    }

    /* Scheduling */
    Scheduling ();

    /* Tick increment/et decrement and Setting last_empty & used_dl*/
    Tick_inc ();

    if ( _kernel_runtsk != NULL ) {
      if ( _kernel_runtsk -> et == 0 ) {
	Job_exit ( "TBS+VRA", 1 );
      }
    }
  }

  printf ( ", %lf", aperiodic_response_time );

  fprintf ( ovhd_dl_max_fp, ", %d", overhead_dl_max );
  fprintf ( ovhd_dl_total_fp, ", %d", overhead_dl_total );
  fprintf ( ovhd_al_max_fp, ", %d", overhead_alpha_max );
  fprintf ( ovhd_al_total_fp, ", %d", overhead_alpha_total );
  fprintf ( ovhd_total_max_fp, ", %d", overhead_total_max );
  fprintf ( ovhd_total_full_fp, ", %d", overhead_total_full );


#else
  printf ( ",", aperiodic_response_time );

  fprintf ( ovhd_dl_max_fp, "," );
  fprintf ( ovhd_dl_total_fp, "," );
  fprintf ( ovhd_al_max_fp, "," );
  fprintf ( ovhd_al_total_fp, "," );
  fprintf ( ovhd_total_max_fp, "," );
  fprintf ( ovhd_total_full_fp, "," );

#endif

/****************************************************************************************************************/
/* TBS+EVRA                                                                                                      */
/****************************************************************************************************************/
#if TBS_EVRA == 1

  Initialize ();

  for ( tick = 0; tick < TICKS; ) {
    for ( i = 0; i < MAX_TASKS; i++ ) {
      if ( req_tim[i][inst_no[i]] == tick ) {
	entry = entry_set ( i );

	if ( periodic[i] == 1 ) {
	  entry -> a_dl = tick + period[i];
	  insert_queue ( &p_ready_queue, entry );
	}
	else {
	  insert_queue_fifo ( &fifo_ready_queue, entry );
	}
	inst_no[i]++;
      }
    }

    if ( ap_ready_queue == NULL && fifo_ready_queue != NULL ) {
      from_fifo_to_ap ();

      DL_set_TBS_evra ( ap_ready_queue, wcet[ap_ready_queue -> tid]);
	  //DL_set_TBS_vra_V2 ( ap_ready_queue, wcet[ap_ready_queue -> tid]);
    }

    /* Scheduling */
    Scheduling ();

    /* Tick increment/et decrement and Setting last_empty & used_dl*/
    Tick_inc ();

    if ( _kernel_runtsk != NULL ) {
      if ( _kernel_runtsk -> et == 0 ) {
	Job_exit ( "TBS+EVRA", 1 );
      }
    }
  }

  printf ( ", %lf", aperiodic_response_time );

  fprintf ( ovhd_dl_max_fp, ", %d", overhead_dl_max );
  fprintf ( ovhd_dl_total_fp, ", %d", overhead_dl_total );
  fprintf ( ovhd_al_max_fp, ", %d", overhead_alpha_max );
  fprintf ( ovhd_al_total_fp, ", %d", overhead_alpha_total );
  fprintf ( ovhd_total_max_fp, ", %d", overhead_total_max );
  fprintf ( ovhd_total_full_fp, ", %d", overhead_total_full );


#else
  printf ( ",", aperiodic_response_time );

  fprintf ( ovhd_dl_max_fp, "," );
  fprintf ( ovhd_dl_total_fp, "," );
  fprintf ( ovhd_al_max_fp, "," );
  fprintf ( ovhd_al_total_fp, "," );
  fprintf ( ovhd_total_max_fp, "," );
  fprintf ( ovhd_total_full_fp, "," );

#endif


/****************************************************************************************************************/
/* ATBS                                                                                                         */
/****************************************************************************************************************/
#if ATBS == 1

  Initialize ();

  for ( tick = 0; tick < TICKS; ) {

    for ( i = 0; i < MAX_TASKS; i++ ) {
      if ( req_tim[i][inst_no[i]] == tick ) {
	entry = entry_set ( i );

	if ( periodic[i] == 1 ) {
	  entry -> a_dl = tick + period[i];
	  insert_queue ( &p_ready_queue, entry );
	}
	else {
	  insert_queue_fifo ( &fifo_ready_queue, entry );
	}
	inst_no[i]++;
      }
    }

    if ( ap_ready_queue == NULL && fifo_ready_queue != NULL ) {
      from_fifo_to_ap ();

      //overhead_total += ASSIGN + MEM + MEM + IADD + IADD + FMUL + FMUL; // overhead for execution time prediction
	  DL_set_TBS ( ap_ready_queue, ap_ready_queue -> pet );
    }

    /* Scheduling */
    Scheduling ();

    /* Tick increment/et decrement and Setting last_empty & used_dl*/
    Tick_inc ();

    if ( _kernel_runtsk != NULL ) {
      /* Deadline Update if necessary */
      DL_update_ATBS ( _kernel_runtsk, wcet[_kernel_runtsk -> tid],_kernel_runtsk -> pet);

      if ( _kernel_runtsk -> et == 0 ) {
	Job_exit ( "ATBS", 1 );
      }
    }
  }

  printf ( ", %lf", aperiodic_response_time );

  fprintf ( ovhd_dl_max_fp, ", %d", overhead_dl_max );
  fprintf ( ovhd_dl_total_fp, ", %d", overhead_dl_total );
  fprintf ( ovhd_al_max_fp, ", %d", overhead_alpha_max );
  fprintf ( ovhd_al_total_fp, ", %d", overhead_alpha_total );
  fprintf ( ovhd_total_max_fp, ", %d", overhead_total_max );
  fprintf ( ovhd_total_full_fp, ", %d", overhead_total_full );


#else
  printf ( ", ", aperiodic_response_time );

  fprintf ( ovhd_dl_max_fp, "," );
  fprintf ( ovhd_dl_total_fp, "," );
  fprintf ( ovhd_al_max_fp, "," );
  fprintf ( ovhd_al_total_fp, "," );
  fprintf ( ovhd_total_max_fp, "," );
  fprintf ( ovhd_total_full_fp, "," );

#endif

/****************************************************************************************************************/
/* ATBS+VRA                                                                                                     */
/****************************************************************************************************************/
#if ATBS_VRA == 1

  Initialize ();

  for ( tick = 0; tick < TICKS; ) {
    for ( i = 0; i < MAX_TASKS; i++ ) {
      if ( req_tim[i][inst_no[i]] == tick ) {
	entry = entry_set ( i );

	if ( periodic[i] == 1 ) {
	  entry -> a_dl = tick + period[i];
	  insert_queue ( &p_ready_queue, entry );
	}
	else {
	  insert_queue_fifo ( &fifo_ready_queue, entry );
	}
	inst_no[i]++;
      }
    }

    if ( ap_ready_queue == NULL && fifo_ready_queue != NULL ) {
      from_fifo_to_ap ();

      //overhead_total += ASSIGN + ASSIGN + MEM + MEM + IADD + IADD + FDIV; // overhead for execution time prediction
	  DL_set_TBS_vra ( ap_ready_queue, ap_ready_queue -> pet );
    }

    /* Scheduling */
    Scheduling ();

    /* Tick increment/et decrement and Setting last_empty & used_dl*/
    Tick_inc ();

    if ( _kernel_runtsk != NULL ) {
      /* Deadline Update if necessary */
      DL_update_ATBS ( _kernel_runtsk, wcet[_kernel_runtsk -> tid],_kernel_runtsk -> pet);

      if ( _kernel_runtsk -> et == 0 ) {
	Job_exit ( "ATBS+VRA", 1 );
      }
    }
  }

  printf ( ", %lf", aperiodic_response_time );

  fprintf ( ovhd_dl_max_fp, ", %d", overhead_dl_max );
  fprintf ( ovhd_dl_total_fp, ", %d", overhead_dl_total );
  fprintf ( ovhd_al_max_fp, ", %d", overhead_alpha_max );
  fprintf ( ovhd_al_total_fp, ", %d", overhead_alpha_total );
  fprintf ( ovhd_total_max_fp, ", %d", overhead_total_max );
  fprintf ( ovhd_total_full_fp, ", %d", overhead_total_full );


#else
  printf ( ",", aperiodic_response_time );

  fprintf ( ovhd_dl_max_fp, "," );
  fprintf ( ovhd_dl_total_fp, "," );
  fprintf ( ovhd_al_max_fp, "," );
  fprintf ( ovhd_al_total_fp, "," );
  fprintf ( ovhd_total_max_fp, "," );
  fprintf ( ovhd_total_full_fp, "," );

#endif

/****************************************************************************************************************/
/* ATBS+EVRA                                                                                                     */
/****************************************************************************************************************/
#if ATBS_EVRA == 1

  Initialize ();

  for ( tick = 0; tick < TICKS; ) {
    for ( i = 0; i < MAX_TASKS; i++ ) {
      if ( req_tim[i][inst_no[i]] == tick ) {
	entry = entry_set ( i );

	if ( periodic[i] == 1 ) {
	  entry -> a_dl = tick + period[i];
	  insert_queue ( &p_ready_queue, entry );
	}
	else {
	  insert_queue_fifo ( &fifo_ready_queue, entry );
	}
	inst_no[i]++;
      }
    }

    if ( ap_ready_queue == NULL && fifo_ready_queue != NULL ) {
      from_fifo_to_ap ();

      //overhead_total += ASSIGN + ASSIGN + MEM + MEM + IADD + IADD + FDIV; // overhead for execution time prediction
	  DL_set_TBS_evra ( ap_ready_queue, ap_ready_queue -> pet );
	  
    }

    /* Scheduling */
    Scheduling ();

    /* Tick increment/et decrement and Setting last_empty & used_dl*/
    Tick_inc ();

    if ( _kernel_runtsk != NULL ) {
      /* Deadline Update if necessary */
      DL_update_ATBS ( _kernel_runtsk, wcet[_kernel_runtsk -> tid],_kernel_runtsk -> pet);

      if ( _kernel_runtsk -> et == 0 ) {
	Job_exit ( "ATBS+EVRA", 1 );
      }
    }
  }

  printf ( ", %lf", aperiodic_response_time );

  fprintf ( ovhd_dl_max_fp, ", %d", overhead_dl_max );
  fprintf ( ovhd_dl_total_fp, ", %d", overhead_dl_total );
  fprintf ( ovhd_al_max_fp, ", %d", overhead_alpha_max );
  fprintf ( ovhd_al_total_fp, ", %d", overhead_alpha_total );
  fprintf ( ovhd_total_max_fp, ", %d", overhead_total_max );
  fprintf ( ovhd_total_full_fp, ", %d", overhead_total_full );


#else
  printf ( ",", aperiodic_response_time );

  fprintf ( ovhd_dl_max_fp, "," );
  fprintf ( ovhd_dl_total_fp, "," );
  fprintf ( ovhd_al_max_fp, "," );
  fprintf ( ovhd_al_total_fp, "," );
  fprintf ( ovhd_total_max_fp, "," );
  fprintf ( ovhd_total_full_fp, "," );

#endif

/****************************************************************************************************************/
/* TB* / TB(n)                                                                                                  */
/****************************************************************************************************************/
#if ITBS == 1

  Initialize();

  for ( tick = 0; tick < TICKS; ) {
    for ( i = 0; i < MAX_TASKS; i++ ) {
      if ( req_tim[i][inst_no[i]] == tick ) {
	entry = entry_set ( i );

	if ( periodic[i] == 1 ) {
	  entry -> a_dl = tick + period[i];

	  insert_queue ( &p_ready_queue, entry );
	}
	else {
	  insert_queue_fifo ( &fifo_ready_queue, entry );
	}
	inst_no[i]++;
      }
    }

    if ( ap_ready_queue == NULL && fifo_ready_queue != NULL ) {
      from_fifo_to_ap ();

      DL_set_ITBS ( ap_ready_queue );
    }

    /* Scheduling */
    Scheduling ();

    /* Tick increment/et decrement and Setting last_empty & used_dl*/
    Tick_inc ();

    if ( _kernel_runtsk != NULL ) {
      if ( _kernel_runtsk -> et == 0 ) {
	Job_exit ( "TB*/TB(n)", 0 );
      }
    }
  }

  printf ( ", %lf", aperiodic_response_time );

  fprintf ( ovhd_dl_max_fp, ", %d", overhead_dl_max );
  fprintf ( ovhd_dl_total_fp, ", %d", overhead_dl_total );
  fprintf ( ovhd_al_max_fp, ", %d", overhead_alpha_max );
  fprintf ( ovhd_al_total_fp, ", %d", overhead_alpha_total );
  fprintf ( ovhd_total_max_fp, ", %d", overhead_total_max );
  fprintf ( ovhd_total_full_fp, ", %d", overhead_total_full );

#else
  printf ( ", ", aperiodic_response_time );

  fprintf ( ovhd_dl_max_fp, "," );
  fprintf ( ovhd_dl_total_fp, "," );
  fprintf ( ovhd_al_max_fp, "," );
  fprintf ( ovhd_al_total_fp, "," );
  fprintf ( ovhd_total_max_fp, "," );
  fprintf ( ovhd_total_full_fp, "," );

#endif

/****************************************************************************************************************/
/* CBS                                                                                                          */
/****************************************************************************************************************/
#if CBS == 1

  Initialize ();

  for ( tick = 0; tick < TICKS; ) {
    for ( i = 0; i < MAX_TASKS; i++ ) {
      if ( req_tim[i][inst_no[i]] == tick ) {
	entry = entry_set ( i );

	if ( periodic[i] == 1 ) {
	  entry -> a_dl = tick + period[i];
	  insert_queue ( &p_ready_queue, entry );
	}
	else {
	  insert_queue_fifo ( &fifo_ready_queue, entry );
	}
	inst_no[i]++;
      }
    }

    if ( ap_ready_queue == NULL && fifo_ready_queue != NULL ) {
      from_fifo_to_ap ();

      DL_set_CBS ( ap_ready_queue );
    }

    /* Scheduling */
    Scheduling ();

    /* Tick increment/et decrement and Setting last_empty & used_dl*/
    Tick_inc ();

    if ( _kernel_runtsk != NULL ) {
      /* Deadline Update if necessary */
      DL_update_CBS ( _kernel_runtsk );

      if ( _kernel_runtsk -> et == 0 ) {
	Job_exit ( "CBS", 0 );
      }
    }
  }

  printf ( ", %lf\n", aperiodic_response_time );

  fprintf ( ovhd_dl_max_fp, ", %d\n", overhead_dl_max );
  fprintf ( ovhd_dl_total_fp, ", %d\n", overhead_dl_total );
  fprintf ( ovhd_al_max_fp, ", %d\n", overhead_alpha_max );
  fprintf ( ovhd_al_total_fp, ", %d\n", overhead_alpha_total );
  fprintf ( ovhd_total_max_fp, ", %d\n", overhead_total_max );
  fprintf ( ovhd_total_full_fp, ", %d\n", overhead_total_full );


#else
  printf ( ",\n", aperiodic_response_time );

  fprintf ( ovhd_dl_max_fp, ",\n" );
  fprintf ( ovhd_dl_total_fp, ",\n" );
  fprintf ( ovhd_al_max_fp, ",\n" );
  fprintf ( ovhd_al_total_fp, ",\n" );
  fprintf ( ovhd_total_max_fp, ",\n" );
  fprintf ( ovhd_total_full_fp, ",\n" );

#endif

/********************************************************************************************************************************************/


  fclose ( ovhd_dl_max_fp );
  fclose ( ovhd_dl_total_fp );
  fclose ( ovhd_al_max_fp );
  fclose ( ovhd_al_total_fp );
  fclose ( ovhd_total_max_fp );
  fclose ( ovhd_total_full_fp );

  return 0;
}

void insert_queue ( TCB **rq, TCB *entry )
{
  int   i;
  TCB  *p;

  for ( p = *rq; p != NULL; p = p -> next ) {
    if ( entry -> a_dl < p -> a_dl ) {
      if ( p == *rq ) {
	entry -> next = p;
	p -> prev = entry;
	*rq = entry;
      }
      else {
	p -> prev -> next = entry;
	entry -> next = p;
	entry -> prev = p -> prev;
	p -> prev     = entry;
      }
      break;
    }
    if ( p -> next == NULL ) {
      p -> next     = entry;
      entry -> prev = p;
      break;
    }
  }
  if ( *rq == NULL ) {
    *rq = entry;
  }

  return;
}


void insert_queue_fifo ( TCB **rq, TCB *entry )
{
  TCB  *p;

  if ( *rq == NULL ) {
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
    fifo_ready_queue -> prev = NULL;

  return;
}

void delete_queue ( TCB **rq )
{
  TCB *p;

  p = *rq;

  *rq = (*rq) -> next;

  if ( *rq != NULL)
    (*rq) -> prev = NULL;

  free ( p );

  return;
}


void Initialize ( void )
{
  int i;

  aperiodic_exec_times    = 0;
  aperiodic_total_et      = 0;
  aperiodic_response_time = 0;

  _kernel_runtsk = _kernel_runtsk_pre = NULL;

  for ( i = 0; i < MAX_TASKS; i++ ) {
    exec_times[i] = 0;
    inst_no[i] = 0;
  }

  p_ready_queue    = NULL;
  ap_ready_queue   = NULL;
  fifo_ready_queue = NULL;

  di_1 = 0;
  fi_1 = 0;

  for ( i = 0; i < TICKS; i++ ) {
    used_dl[i] = 0;
  }

  last_empty  = 0;
  max_used_dl = 0;

  cbs_d = 0;
  cbs_c = cbs_Q;

	/* Initials for SVRA*/
  max_period = 0;
  Tmax_last_used =0;
  ins_num = 0;
  for ( i = 0; i < MAX_TASKS*MAX_INSTANCES; i++ ) {
    ins_start[i] = 0;
	ins_start_dl[i] = 0;
  }
  pre_ins_id = -1;
  overhead_dl = overhead_alpha = overhead_dl_max = overhead_alpha_max = overhead_dl_total = overhead_alpha_total = overhead_total = overhead_total_max = overhead_total_full = 0;

  return;
}

TCB *entry_set ( int i )
{
  TCB *entry;

  entry = malloc ( sizeof ( TCB ) );
  entry -> tid             = i;
  entry -> inst_no         = inst_no[i];
  entry -> req_tim         = tick;
  entry -> req_tim_advance = tick;
  entry -> et              = et[i][inst_no[i]];
  entry -> initial_et      = et[i][inst_no[i]];
  entry -> next 		   = entry -> prev = NULL;
  entry -> pet 			   = pre_et[i][inst_no[i]];

  entry -> a_dl = (unsigned long long)0xFFFFFFFFFFFFFFFF;  // dummy necessary

  return entry;

}


void Scheduling ( void )
{
  if ( p_ready_queue == NULL ){
    _kernel_runtsk = ap_ready_queue;
	}
  else if ( ap_ready_queue == NULL ) {
			_kernel_runtsk = p_ready_queue;
			if ( _kernel_runtsk != _kernel_runtsk_pre ){
				if (period[_kernel_runtsk-> tid] >= max_period) {
					max_period = period[_kernel_runtsk-> tid];
					Tmax_last_used = _kernel_runtsk-> a_dl;
				}
			}
		}
		else {
			if ( p_ready_queue->a_dl <= ap_ready_queue->a_dl ) { /* favoring periodic tasks  */
				_kernel_runtsk = p_ready_queue;
				if ( _kernel_runtsk != _kernel_runtsk_pre ){
					if (period[_kernel_runtsk-> tid] >= max_period) {
						max_period = period[_kernel_runtsk-> tid];
						Tmax_last_used = _kernel_runtsk-> a_dl;
					}
				}
			}
			else {
			  _kernel_runtsk = ap_ready_queue;
			}
		}
  if ( _kernel_runtsk != _kernel_runtsk_pre ) {
	_kernel_runtsk_pre = _kernel_runtsk;
  }

  return;
}

void Tick_inc ( void )
{
  if ( _kernel_runtsk == NULL )
	{
		last_empty = tick;
		pre_ins_id = -1;
	}
  else{
		used_dl[tick] = _kernel_runtsk -> a_dl;
		if (_kernel_runtsk -> tid != pre_ins_id){
			ins_num ++;
			ins_start[ins_num] = tick;
			ins_start_dl[ins_num] = _kernel_runtsk -> a_dl;
			pre_ins_id = _kernel_runtsk -> tid;
		}
	  }

  Overhead_Record ( );

  tick++;

  if ( _kernel_runtsk != NULL ) {
    --(_kernel_runtsk -> et);
    --(_kernel_runtsk -> pet);
  }

  return;
}


void Overhead_Record ( void )
{

  if ( overhead_total_max < overhead_total )
    overhead_total_max = overhead_total;

  if ( overhead_dl_max < overhead_dl )
    overhead_dl_max = overhead_dl;

  if ( overhead_alpha_max < overhead_alpha )
    overhead_alpha_max = overhead_alpha;

  overhead_dl_total    += overhead_dl;
  overhead_alpha_total += overhead_alpha;
  overhead_total_full += overhead_total;

  //overhead_total_full += overhead_dl + overhead_alpha;

  overhead_dl = overhead_alpha = overhead_total = 0;

  return;
}


void Job_exit ( const char *s, int rr) /* rr: resource reclaiming */
{

  if ( _kernel_runtsk -> a_dl < tick ) {
    fprintf( stderr, "\n# MISS: %s: tick=%lld: DL=%lld: DL_ORIG=%lld: TID=%d: INST_NO=%d, WCET=%d, ET=%d, req_tim=%lld, req_tim_advance=%lld\n",
	     s, tick, _kernel_runtsk->a_dl, _kernel_runtsk->a_dl_TBS, _kernel_runtsk->tid, _kernel_runtsk->inst_no,
	     wcet[_kernel_runtsk->tid], _kernel_runtsk->initial_et, _kernel_runtsk->req_tim, _kernel_runtsk->req_tim_advance);
    printf( "\n# MISS: %s: tick=%lld: DL=%lld: DL_ORIG=%lld: TID=%d: INST_NO=%d\n",
	    s, tick, _kernel_runtsk->a_dl, _kernel_runtsk->a_dl_TBS, _kernel_runtsk->tid, _kernel_runtsk->inst_no );
	printf( "\n# MISS: %s: tick=%lld: DL=%lld: Bound_dl=%lld\n",
	    s, tick, _kernel_runtsk->a_dl, a_dl_bound );
  }
	
  exec_times[_kernel_runtsk -> tid]++;

  if ( periodic[_kernel_runtsk->tid] == 0 ) {
    aperiodic_exec_times++;
    aperiodic_total_et += tick - _kernel_runtsk -> req_tim;
    aperiodic_response_time = (double)aperiodic_total_et / (double)aperiodic_exec_times;

    if ( rr == 1 ) {/* Trying resource reclaiming */
      overhead_total += COMP + COMP + FDIV + CEIL + IADD;
	  overhead_dl += COMP + COMP + FDIV + CEIL + IADD; /* Of course, (1.0 - p_util) is statically solved. */
      di_1 = (double)max3(_kernel_runtsk->req_tim_advance, di_1, fi_1) + ceil ( (double)_kernel_runtsk->initial_et/ (1.0 - p_util) );
    }
    else { /* Not concerned with CBS */
      overhead_total += ASSIGN;
	  overhead_dl += ASSIGN;
      di_1 = _kernel_runtsk -> a_dl_TBS;
    }
    overhead_total += ASSIGN;
	overhead_dl += ASSIGN;
    fi_1 = tick;
  }
  if ( periodic[_kernel_runtsk->tid] == 1 ) {
		delete_queue ( &p_ready_queue );
	}
  else
    delete_queue ( &ap_ready_queue );

  return;
}


void DL_set_TBS ( TCB *entry, unsigned int et )
{

  overhead_total += COMP + COMP + IADD;
  overhead_dl += COMP + COMP + IADD; /* FDIV & CEIL are statically solved */
  entry -> a_dl = (double)max3(entry -> req_tim, di_1, fi_1) + ceil ( (double)et / (1.0 - p_util) );

  return;
}


void DL_update_ATBS ( TCB *entry, unsigned int wcet, unsigned int pet)
{
	

  overhead_dl += COMP + COMP;
  if ( ( periodic[entry -> tid] == 0 ) && ( entry -> et != 0 ) && ( entry -> pet <= 0 ) ) {

    overhead_total += ASSIGN + IADD;
	overhead_dl += IADD; /* FDIV & CEIL are statically solved. */
    // Non-stepwise deadline update
	entry -> a_dl += ceil ( (double)(wcet- entry -> pet) / (1.0 - p_util) );
	entry -> pet = wcet- entry -> pet;
	
	// Stepwise deadline update
	//entry -> a_dl += ceil ( (double)1 / (1.0 - p_util) );
	//entry -> pet = 1;
  }

  return;
}


void DL_set_TBS_vra0 ( TCB *entry, unsigned int et, unsigned long long bound, unsigned long long last_dl )
{

  overhead_total += COMP + IADD + ASSIGN;
  overhead_dl += COMP + IADD + ASSIGN;  /* FDIV is statically solved */
  entry -> a_dl =  max( entry -> req_tim_advance, last_dl ) + ceil ( (double)et / (1.0 - p_util) );

  overhead_total += COMP;
  overhead_alpha += COMP;
  while ( (signed)bound <= (signed)(entry -> req_tim_advance) ) {
	overhead_total += IADD + MEM + COMP;
    overhead_alpha += IADD + MEM + COMP; /* "-1" is solved by immediate-addressed access */
    if ( max_used_dl < used_dl[ (entry -> req_tim_advance) - 1 ] ) {
      overhead_total += ASSIGN;
	  overhead_alpha += ASSIGN;   /* element already read */
      max_used_dl = used_dl[ (entry -> req_tim_advance) - 1 ];
    }
	overhead_total += COMP;
    overhead_alpha += COMP;
    if ( entry -> a_dl > max_used_dl ) { /* ">" is correct. */
      overhead_total += ASSIGN + IADD;
	  overhead_alpha += ASSIGN + IADD;
      entry -> req_tim_advance -= 1;
      overhead_alpha += ASSIGN + IADD;
      entry -> a_dl -= 1;
      overhead_alpha += IADD;
	  overhead_alpha += IADD;
    }
    else {
      break;
    }
	overhead_total += COMP;
    overhead_alpha += COMP;
  }

  return;
}


void DL_set_TBS_vra ( TCB *entry, unsigned int et )
{
  unsigned long long  bound;
  unsigned long long  last_dl;

  unsigned long long  req_tim_advance_tmp, a_dl_tmp;

  unsigned int i;

  max_used_dl = 0;

  overhead_total += COMP + ASSIGN;
  overhead_dl += COMP + ASSIGN;
  last_dl = max( di_1, fi_1 );

  overhead_total += IADD + IADD + COMP + COMP + ASSIGN;
  overhead_alpha += IADD + IADD + COMP + COMP + ASSIGN;
  bound = max3((signed long long)tick - VRA_BOUND,
	       (signed long long)last_empty + 1,
	       (signed long long)last_dl );

  overhead_total += COMP + IADD + ASSIGN;
  overhead_dl += COMP + IADD + ASSIGN;  /* FDIV is statically solved */
  entry -> a_dl =  max( entry -> req_tim_advance, last_dl ) + ceil ( (double)et / (1.0 - p_util) );

  overhead_total += ASSIGN + COMP + COMP;
  overhead_alpha += ASSIGN + COMP + COMP;
  for ( i = et; entry -> req_tim_advance >= bound && i <= wcet[entry -> tid]; i++ ) {
    overhead_total += ASSIGN + ASSIGN;
	overhead_alpha += ASSIGN + ASSIGN;
    req_tim_advance_tmp = entry -> req_tim_advance;
    a_dl_tmp = entry -> a_dl;
    overhead_total += ASSIGN;
	overhead_alpha += ASSIGN;
    entry -> pet  = i;

    DL_set_TBS_vra0 ( entry, i, bound, last_dl );

    overhead_total += COMP;
	overhead_alpha += COMP;
    if ( entry -> a_dl > a_dl_tmp ) {
      overhead_total += ASSIGN + ASSIGN;
	  overhead_alpha += ASSIGN + ASSIGN;
      entry -> req_tim_advance = req_tim_advance_tmp;
      entry -> a_dl            = a_dl_tmp;
      overhead_total += ASSIGN + IADD;
	  overhead_alpha += ASSIGN + IADD;
      entry -> pet  = i-1;
      break;
    }

    overhead_total += COMP + COMP + IADD;
	overhead_alpha += COMP + COMP + IADD;
  }
  return;
}


void DL_set_CBS ( TCB *entry )
{
  /* CBS is idle */ /* In the Book, ">=" is used instead of ">". But ">" is right! */

  overhead_total += IADD + FMUL + FLOOR + COMP;
  overhead_dl += IADD + FMUL + FLOOR + COMP;
  if ( cbs_c > floor ( ((signed long long)cbs_d - (signed long long)tick ) * (1.0 - p_util) ) ) {
    overhead_total += IADD;
	overhead_dl += IADD;
    cbs_d = tick + cbs_T;
	overhead_total += ASSIGN;
    overhead_dl += ASSIGN;
    cbs_c = cbs_Q;
  }
  overhead_total += ASSIGN;
  overhead_dl += ASSIGN;
  ap_ready_queue -> a_dl = cbs_d;

  return;
}


void DL_update_CBS ( TCB *entry )
{

  if ( periodic[entry -> tid] == 0 ) {

    overhead_total += IADD;
	overhead_dl += IADD;
    --cbs_c;  /* decrease CBS budget */

    overhead_total += COMP;
	overhead_dl += COMP;
    if (cbs_c == 0 ) {
      overhead_total += ASSIGN;
	  overhead_dl += ASSIGN;
      cbs_c = cbs_Q;
      overhead_total += IADD;
	  overhead_dl += IADD;
      cbs_d += cbs_T;

      overhead_total += ASSIGN;
	  overhead_dl += ASSIGN;
      entry -> a_dl = cbs_d;

    }
  }

  return;
}


void DL_set_ITBS( TCB *entry )
{
  unsigned long long f, d, Ia, If;
  unsigned long long next_r;
  int s=0, j;
  TCB *queue;

  long long tmp;

  overhead_total += COMP + IADD;
  overhead_dl += COMP + IADD;
  entry -> a_dl_TBS = (double)max(tick,di_1) + ceil ( (double)wcet[ entry -> tid ] / (1.0 - p_util) );

  overhead_total += ASSIGN;
  overhead_alpha += ASSIGN; /* f0 must be saved for later use by the next instance */
  f = entry -> a_dl_TBS;

  do {

    overhead_total += IADD;
	overhead_alpha += IADD;
    s++;

    overhead_total += ASSIGN;
	overhead_alpha += ASSIGN;
    d = f;

    overhead_total += ASSIGN + ASSIGN + COMP;
	overhead_alpha += ASSIGN + ASSIGN + COMP;
    for ( Ia=0, queue = p_ready_queue; queue != NULL; queue = queue -> next ) {
      overhead_total += MEM + COMP;
	  overhead_alpha += MEM + COMP; /* queue manupulation is considered */
      if ( queue -> a_dl <= d ) {   /* "<=" because favoing periodic tasks in scheduling */
	overhead_total += MEM + IADD;
	overhead_alpha += MEM + IADD;
	Ia += queue -> et;
      }
      overhead_total += MEM + COMP;
	  overhead_alpha += MEM + COMP;
    }

    overhead_total += ASSIGN + ASSIGN + COMP;
	overhead_alpha += ASSIGN + ASSIGN + COMP;
    for ( If=0, j=0; j < p_tasks; j++ ) {

      overhead_total += IADD + FDIV + CEIL + IMUL + IADD;
	  overhead_alpha += IADD + FDIV + CEIL + IMUL + IADD;
      next_r = phase[j] + ceil( ((double)(tick - phase[j]))/(double)period[j] ) * period[j];

      overhead_total += IADD + FDIV + FLOOR;
	  overhead_alpha += IADD + FDIV + FLOOR;
      //tmp = (unsigned long long)ceil((double)((long long)d-(long long)next_r)/(double)period[j]) - 1;
      tmp = (unsigned long long)floor((double)((long long)d-(long long)next_r)/(double)period[j]); /* because favoring periodic tasks */

      overhead_total += COMP + IMUL + IADD;
	  overhead_alpha += COMP + IMUL + IADD;
      If += max(0,tmp) * wcet[j];

      overhead_total += IADD + COMP;
	  overhead_alpha += IADD + COMP; /* for loop */
    }

    overhead_total += IADD + IADD + IADD;
	overhead_alpha += IADD + IADD + IADD;
    f = tick + wcet[ entry -> tid ] + Ia + If;

    overhead_total += COMP + COMP + ILOG;
	overhead_alpha += COMP + COMP + ILOG;
  } while ( f < d && s < ITBS_BOUND );  // +3


  entry -> a_dl = d;

  return;
}

void DL_set_TBS_evra ( TCB *entry, unsigned int et )
{
  unsigned long long  bound;
  unsigned long long  taskTime;
  unsigned long long  last_dl;
  unsigned long long  bound_dl;

  unsigned long long  req_tim_advance_tmp, a_dl_tmp;

  unsigned int i = ins_num;
  int test = 0;

  max_used_dl = 0;
  overhead_total += COMP + ASSIGN;
  last_dl = max( di_1, fi_1 );
  taskTime =  (unsigned long long)et / (1.0 - p_util);
  overhead_total += COMP;
  if (entry -> req_tim <= last_dl){
	overhead_total += IADD + ASSIGN;
	entry -> a_dl = last_dl + taskTime;
	return;
  }
  overhead_total += ASSIGN + COMP + COMP + IADD + IADD;
  bound = max(last_empty + 1, last_dl);
  bound_dl = max(bound + taskTime, Tmax_last_used);
  overhead_total += IADD + ASSIGN;
  entry -> a_dl =  entry -> req_tim + taskTime;
  overhead_total += COMP;
  while (entry -> a_dl > bound_dl){
	overhead_total += COMP + MEM;
	if (max_used_dl < ins_start_dl[i]){
		overhead_total += ASSIGN;
		max_used_dl = ins_start_dl[i];
	}
	overhead_total += COMP;
	if (entry -> a_dl <= max_used_dl){
		break;
	}
	overhead_total += ASSIGN + COMP + IADD + MEM;
	entry -> a_dl = max(ins_start[i] + taskTime, bound_dl);
	overhead_total += COMP;
	if(entry -> a_dl <= max_used_dl){
		overhead_total += ASSIGN;
		entry -> a_dl = max_used_dl;
		break;
	}
	overhead_total += IADD;
	i -= 1;
	overhead_total += COMP;
  }
  return;
}
