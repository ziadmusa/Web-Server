# Web-Server
creating web server using pool threads, in linux and using C language

1-Difficulties:

-Initially when we used the VM engine we had difficulty doing the test in the given code, and also for security reasons sometimes the machine refuses to connect after several requests.
-We used second Ubuntu server to compile c file because static/dynamic compile etc is a problem. However, it is running a web server.
-We encountered a few difficulties when converting the code from signle thread to pool thread.

-------------------------------------------------------
2-Requirements that you designed and realized:
-We used the queuing algorithm and gave a function which is to add a request to the tail of the queued list in the queue. The benefit of this list is the queue function.
-A function get request is a function that returns the master node and frees the memory corresponding to that node.
-Modify make file to compile our thread application.
-Use the Apache benchmark to run tests on a web server.
-Use infinite loop of requests handling this algorithm make it forever, if there are requests to handle, take the firstand handle it. Then wait on the given condition variable,and when it is signaled, re-do the loop.
-Use function for check handle request, if there is an request to server.

------------------------------------------------------------
3-Requirements that you designed but failed to realize:
-the default value for arguments  [portnum] [home] [threads] [buffers]

----------------------------------------------------
4-Project biuld's build command:
-Type make (we add -pthread) and our config file appears.

CC = gcc

-CFLAGS = -c -O2 -Wall -D_REENTRANT


all: web-server


-web-server: web-util.o web-server.o web-util.h

        ${CC} -pthread -o web-server web-util.o web-server.o


-web-server.o: web-server.c web-util.h

        ${CC} ${CFLAGS} web-server.c


-clean:
        rm web-server.o web-server web-server.log



