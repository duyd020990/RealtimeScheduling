
all:
	cd DataGen && make
	cd MultiprocessorScheduling && make

clean:
	rm *.o
	rm Schedule
	rm DataGenerator
clean_dataset:
	rm periodic.*
	rm *.cfg
clean_result:
	rm *.csv
