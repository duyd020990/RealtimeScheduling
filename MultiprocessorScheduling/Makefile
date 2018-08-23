PRINT		= 0
CFLAGS		= -Wall -O2 -DPRINT=$(PRINT)
LDFLAGS		= -lm
TARGET		= Schedule
objects		= Schedule.o RM.o LSF.o EDF.o ILSF.o RUN.o LLREF.o

all:		$(TARGET)


Schedule.o:Schedule.c Schedule.h
	$(CC) -c $(CFLAGS) Schedule.c

Schedule:$(objects)
	$(CC) -o Schedule $(objects) $(LDFLAGS) 
	mv *.o Schedule ../

clean:;		rm -f $(TARGET) *.o
