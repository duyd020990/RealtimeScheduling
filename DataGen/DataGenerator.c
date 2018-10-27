#include "DataGenerator.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

double exp_dist(double lambda,unsigned rand)
{
    double u;
    u = rand/(RAND_MAX+1.0);
    return -log(1-u)/lambda;
}

void empty_double(double array[],unsigned len)
{
    int i;
    if(array==NULL){return;}
    for(i=0;i<len;i++){array[i] = 0;}
}


void* generator(void* arg)
{
    double lb_util = UTIL_LOWER_BOUND;
    double ub_util = UTIL_UPPER_BOUND;
    double util[MAX_SIZE][MAX_TSK]; // recode of utilization (70%, 75%, 80%, 85%, 90%, 95% and 100%)
    double num;
    double sum[MAX_SIZE], temp_sum;
    double pd=0,p[MAX_TSK];
    int    mod,i,k,j,l,exp,et,ulti_fact,cap;
    int    wcet[MAX_SIZE][MAX_TSK];                           // We here limit the wcet to be integer numbers lower than 20
    int    period[MAX_SIZE][MAX_TSK];
    char   buff[BUFSIZ];
    FILE*  f = NULL;
    GEN_ARG* gen_arg = NULL;

    if(arg == NULL)
    {
        printf("thread NULL argument.\n");
        return NULL;
    }
    
    printf("child thread\n");
    gen_arg = arg;
    
    srand((unsigned)time(NULL));
    //Generate the Utilization
    ulti_fact = gen_arg->ulti;
    cap = gen_arg->cap;
    for (exp = 1; exp <= MAX_SIZE; exp += 1) 
    {
        i=0;
        empty_double(p,MAX_TSK);
        sum[exp - 1] = 0;
        temp_sum = 0;
        while (i < MAX_TSK) // i correspods to each task
        {
            num = ceil(exp_dist(exp,rand()) * 10) / 10.0;
            if(num >= lb_util && num <= ub_util) // here we just limit the acceptable utilization among lb_util to ub_util in order to increase the number of tasks
            {                             // If we accept a larger utilization, then the number of tasks is just a few.
                temp_sum = sum[exp - 1] + num;      // temporary sum of utilization
                if(temp_sum > cap * (0.7 + 0.05*ulti_fact)) // if sum of utilization exceeds the expected total utilization (for example: 70% of processor capacity)
                {                                       // then the total utilization is set to the exepected one.
                    p[i] = ceil((cap * (0.7 + 0.05*ulti_fact) - sum[exp - 1])*10)/10.0;
                    sum[exp - 1] = cap * (0.7 + 0.05*ulti_fact);
                    break;
                }
                p[i] = num;
                sum[exp - 1] = temp_sum;
                if(sum[exp - 1] == cap * (0.7 + 0.05*ulti_fact)){break;} // if the total utilization is equal to the expected one, then we stop generating tasks
                i++;
            }
        }
        // copy the temporary utilizations to the record of utilization
        for (l = 0; l < MAX_TSK; l++){util[exp - 1][l] = p[l];}
    }
        //Generate WCET
    printf("Case %d%%: generating\n",70 + 5*ulti_fact);
    for (k = 0; k < MAX_SIZE; k++)
    {
        printf(" Set %d generating\n",k);
        for (j = 0; j < MAX_TSK; j++)
        {
            if (util[k][j]*10 > 0)
            {
                wcet[k][j] = UNIDIS(1,rand(),20);
                pd = wcet[k][j] / util[k][j];
                mod = (int)(pd * 10);
                while (mod % 10)
                {
                    wcet[k][j] = UNIDIS(1,rand(),20);
                    pd = wcet[k][j] / util[k][j];
                    mod = (int)(pd * 10);
                }
                period[k][j] = pd;
            }
            else{wcet[k][j] = 0;}
        }
    }
    
    for (k = 0; k < MAX_SIZE; k++)
    {
        sprintf(buff,"periodic.cfg.%d-%d",70 + 5 * ulti_fact,k+1);
        f=fopen(buff,"w+");
        fprintf(f,"#Target Periodic Utilization = %d%%\n",70+5*i);
        fprintf(f,"#tid\twcet\tet\tprd\treq_tim\n");
        for (j = 0; j < MAX_TSK; j++)
        {
            if (util[k][j] > 0)
            {
                for (l = 0; l*period[k][j] < MAX_OBSERVATION; l++)
                {
                    et = (int)UNIDIS(1,rand(),wcet[k][j]);
                    fprintf(f,"%d\t%d\t%d\t%d\t%d\n",j,wcet[k][j],et,period[k][j],l*period[k][j]);
                }
            }
        }
        fclose(f);
    }
    return NULL;
}
