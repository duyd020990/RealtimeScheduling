#include "tool.h"

#include "../Schedule.h"

#include <math.h>

extern unsigned long long overhead_dl;

double my_round(double number)
{
    overhead_dl += FMUL+FDIV;
    return ((round(number*10.0))/10.0);
}
