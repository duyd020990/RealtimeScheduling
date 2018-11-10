#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define LOG_FILE_NAME "log"

char* log_file_name = LOG_FILE_NAME;

FILE* fp = NULL;

//Open the log file.
int log_open(char* file_name)
{
    if(file_name == NULL)
    {
        fp = stderr;
        return 1;
    }
    
    fp = fopen(file_name,"w+");
    if(fp == NULL){fprintf(stderr,"fp is NULL,in log_open\n");exit(-1);}
    
    return 1;
}

//Close the log file.
int log_close()
{
    if(fp==NULL || fp==stderr){return 0;}
    
    if(!fclose(fp)){return 0;}
    
    fp = NULL;
    return 1;
}

//For continues output.
int log_c(const char* fmt,...)
{
    va_list args;

    if(fp == NULL){fp = stdout;}

    va_start(args,fmt);
    vfprintf(fp,fmt,args);
    va_end(args);

    return 0;
}

// For log message out immediatelly.
int log_once(char* file_name,const char* fmt,...)
{
    FILE* local_fp = NULL;
    va_list args;

    if(file_name == NULL){local_fp = stderr;}
    else
    {
        local_fp = fopen(file_name,"a+");
        if(local_fp == NULL){return 0;}
    }
    fseek(local_fp,0,SEEK_END);

    va_start(args,fmt);
    vfprintf(local_fp,fmt,args);
    va_end(args);

    if(local_fp != stderr){fclose(local_fp);}
    
    return 0;
}

