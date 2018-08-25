
all:
	cd DataGen && make
	cd MultiprocessorScheduling && make

clean:
	rm *.o
clean_dataset:
	rm periodic.*
clean_result:
	rm *.csv