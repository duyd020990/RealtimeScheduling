PRINT		= 0
CFLAGS		= -Wall -O2 -DPRINT=$(PRINT)
LDFLAGS		= -lm
TARGET		= Schedule
objects		= Schedule.o RM.o LSF.o EDF.o ILSF.o RUN.o LLREF.o signal.o

all:		$(TARGET)


Schedule.o:Schedule.c Schedule.h
	gcc -c $(CFLAGS) Schedule.c

Schedule:$(objects)
	gcc -o Schedule $(objects) $(LDFLAGS) 
	mv *.o Schedule ../

clean:;		rm -f $(TARGET) *.o
