PRINT		= 0
CFLAGS		= -Wall -O2 -DPRINT=$(PRINT)
LDFLAGS		= -lm
TARGET		= Schedule
objects		= Schedule.o RM.o LSF.o EDF.o ILSF.o RUN.o LLREF.o HEDF.o signal.o log.o tool.o

all:		$(TARGET)

log.o:
	@cd log && make log.o

tool.o:
	@cd tool && make tool.o

signal.o:
	@cd signal && make signal.o

RM.o:RM.c RM.h
	gcc -c $(CFLAGS) RM.c

LSF.o:LSF.c LSF.h
	gcc -c $(CFLAGS) LSF.c

EDF.o:EDF.c EDF.h
	gcc -c $(CFLAGS) EDF.c

HEDF.o:HEDF.c HEDF.h
	gcc -c $(CFLAGS) HEDF.c
	
ILSF.o:ILSF.c ILSF.h
	gcc -c $(CFLAGS) ILSF.c

RUN.o:RUN.c RUN.h
	gcc -c $(CFLAGS) RUN.c

LLREF.o:LLREF.c LLREF.h
	gcc -c $(CFLAGS) LLREF.c

Schedule.o:Schedule.c Schedule.h
	gcc -c $(CFLAGS) Schedule.c

Schedule:$(objects)
	gcc -o Schedule $(objects) $(LDFLAGS) 
	mv *.o Schedule ../

clean:
	rm -f $(TARGET) *.o
