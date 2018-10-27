// DataGenerator.cpp : Defines the entry point for the console application.
// Utilization cases: 70% --> 100%, interval = 5;
#include "DataGenerator.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#ifdef WINDOWS
int main()
#else
int main(int argc,char** argv)
#endif
{
    int cap;

    GEN_ARG gen_arg1;
    GEN_ARG gen_arg2;
    GEN_ARG gen_arg3;
    GEN_ARG gen_arg4;
    GEN_ARG gen_arg5;
    GEN_ARG gen_arg6;
    GEN_ARG gen_arg7;

    pthread_t pt1;
    pthread_t pt2;
    pthread_t pt3;
    pthread_t pt4;
    pthread_t pt5;
    pthread_t pt6;
    pthread_t pt7;

#ifndef WINDOWS
    if(argc>=2)
    {
        cap = atoi(argv[1]);
    }
#else
    cap = CAP;
#endif

    gen_arg1.cap  = cap;
    gen_arg1.ulti = PERCENT_70;
    pthread_create(&pt1,NULL,generator,&gen_arg1);
    gen_arg2.cap  = cap;
    gen_arg2.ulti = PERCENT_75;
    pthread_create(&pt2,NULL,generator,&gen_arg2);
    gen_arg3.cap  = cap;
    gen_arg3.ulti = PERCENT_80;
    pthread_create(&pt3,NULL,generator,&gen_arg3);
    gen_arg4.cap  = cap;
    gen_arg4.ulti = PERCENT_85;
    pthread_create(&pt4,NULL,generator,&gen_arg4);
    gen_arg5.cap  = cap;
    gen_arg5.ulti = PERCENT_90;
    pthread_create(&pt5,NULL,generator,&gen_arg5);
    gen_arg6.cap  = cap;
    gen_arg6.ulti = PERCENT_95;
    pthread_create(&pt6,NULL,generator,&gen_arg6);
    gen_arg7.cap  = cap;
    gen_arg7.ulti = PERCENT_100;
    pthread_create(&pt7,NULL,generator,&gen_arg7);

    pthread_join(pt1,NULL);
    pthread_join(pt2,NULL);
    pthread_join(pt3,NULL);
    pthread_join(pt4,NULL);
    pthread_join(pt5,NULL);
    pthread_join(pt6,NULL);
    pthread_join(pt7,NULL);

    return 0;
}

