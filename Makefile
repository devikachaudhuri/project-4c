default: lab4c_tcp.c lab4c_tls.c
	gcc -Wall -Wextra -lmraa -lm lab4c_tcp.c -o lab4c_tcp
	gcc -Wall -Wextra -lmraa -lm -lssl -lcrypto lab4c_tls.c -o lab4c_tls

dist: lab4c_tcp.c
	tar -czf lab4c-304597339.tar.gz lab4c_tcp.c README Makefile lab4c_tls.c 

clean:
	rm lab4c-304597339.tar.gz
