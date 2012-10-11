CC        = g++
CFLAGS    = -ggdb -Wall
INCLUDEDIR = -I.

SRC       = tcpproxy.cpp
 
OBJ_FILE  = $(SRC:.cpp=.o)
EXE_FILE  = ttt

${EXE_FILE}: ${OBJ_FILE}
	${CC} ${CFLAGS}  -o ${EXE_FILE}  ${OBJ_FILE}  $(INCLUDEDIR) -ldl -lpthread 

.cpp.o:
	$(CC) -c $(CFLAGS) $(INCLUDEDIR)  $<

clean:
	rm -f ${OBJ_FILE} ${EXE_FILE}

