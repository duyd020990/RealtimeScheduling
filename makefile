
all:
	cd DataGen && make
	cd MultiprocessorScheduling && make

clean_all:
	if [ -x "*.o" ]       ;then rm -r *.o       ; fi
	if [ -x "periodic.*" ];then rm -r periodic.*; fi
	if [ -x "*.csv" ]     ;then rm -r *.csv     ; fi