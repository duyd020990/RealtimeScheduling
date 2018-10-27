

all:
	gcc -c main.c -pthread
	gcc -c DataGenerator.c -lm -pthread
	gcc -o DataGenerator main.o DataGenerator.o -lm -pthread
	mv DataGenerator *.o ../

clean:
	rm periodic.*
