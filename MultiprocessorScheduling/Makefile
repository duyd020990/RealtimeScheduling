PRINT		= 0
CFLAGS		= -O2 -DPRINT=$(PRINT)
LDFLAGS		= -lm
TARGET		= Schedule
objects		= Schedule.o

all:		$(TARGET)


Schedule.o:Schedule.c Schedule.h
		$(CC) -c $(CFLAGS) Schedule.c

Schedule:	$(objects)
		$(CC) $(LDFLAGS) -o Schedule $(objects)

clean:;		rm -f $(TARGET) *.o
