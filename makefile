
all:
	@cd DataGen && make --no-print-directory
	@cd MultiprocessorScheduling && make --no-print-directory

clean:
	rm *.o
	rm Schedule
	rm DataGenerator

clean_dataset:
	rm periodic.*
	rm *.cfg

clean_result:
	rm *.csv
