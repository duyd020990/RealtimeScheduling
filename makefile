
all:
	cd DataGen && make
	cd MultiprocessorScheduling && make

clean:
	rm *.o
	rm Schedule
	rm DataGenerator
clean_dataset:
	rm periodic.*
clean_result:
	rm *.csv
