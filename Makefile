# CFLAGS=-g -m32 -DFICL # -Wall
CFLAGS=-g -fPIC # -m32 
# CFLAGS=-g # -Wall
INC=-I/usr/local/include
# 
# Don't build old utilities.  Retained for future reference.
# 
BINS=unixToSpread redisSender dbCache tstLib socketToSpread lightSink lightSource socketToMYSQL spreadToSocket spreadToMySQL socketSpreadBridge socketTestClient toSpread newToSpread newSocketToSpread newUnixToSpread 
LIBS=-lpthread -L/usr/local/lib -lspread -ldl -lm -ljansson 
LFLAGS=-Wl,--no-as-needed 

all:	$(BINS)

toSpread:	toSpread.c libConnectToSpread.so
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) toSpread.c -o toSpread -L . -lConnectToSpread $(LIBS) -lsqlite3

newToSpread:	newToSpread.c libcommonToSpread.so
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) -Wl,-rpath,. newToSpread.c -o newToSpread -L . -lcommonToSpread $(LIBS) -lsqlite3

socketToSpread:	socketToSpread.c libConnectToSpread.so
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) socketToSpread.c -o socketToSpread -L . -lConnectToSpread $(LIBS) -lsqlite3

newSocketToSpread:	newSocketToSpread.c libcommonToSpread.so
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) -Wl,-rpath,. newSocketToSpread.c -o newSocketToSpread -L . -lcommonToSpread $(LIBS) -lsqlite3

unixToSpread:	unixToSpread.c libConnectToSpread.so
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) unixToSpread.c -o unixToSpread -L . -lConnectToSpread $(LIBS) -lsqlite3

newUnixToSpread:	newUnixToSpread.c libcommonToSpread.so
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) -Wl,-rpath,. newUnixToSpread.c -o newUnixToSpread -L . -lcommonToSpread $(LIBS) -lsqlite3

tstLib:	tstLib.c libConnectToSpread.so
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) tstLib.c -o tstLib -L . -lConnectToSpread $(LIBS)

lightSink:	lightSink.c
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) -o lightSink lightSink.c -L/usr/local/lib -ldl -lspread

spreadToMySQL:	spreadToMySQL.c
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) -o spreadToMySQL spreadToMySQL.c -L/usr/local/lib -ldl -lspread -lmysqlclient $(LIBS)

lightSource:	lightSource.c
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) -o lightSource lightSource.c -L/usr/local/lib -ldl -lspread

spreadToSocket:	spreadToSocket.c
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) -o spreadToSocket spreadToSocket.c -L/usr/local/lib -ldl -lspread -lmysqlclient $(LIBS)

socketSpreadBridge:	socketSpreadBridge.c
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) -o socketSpreadBridge socketSpreadBridge.c $(LIBS)

socketTestClient:	socketTestClient.c
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) socketTestClient.c -o socketTestClient $(LIBS)

redisSender:	redisSender.c libConnectToSpread.so
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) -o redisSender redisSender.c -L . -lConnectToSpread $(LIBS)

spreadSource:	source.o hash.o  connectToSpread.o
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) source.o hash.o connectToSpread.o -o spreadSource $(LIBS)

source.o:	source.c mine.h hash.h hash.o
	$(CC) $(INC) -c $(CFLAGS) -Wall source.c -o source.o 

dbCache:	dbCache.c libConnectToSpread.so
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) dbCache.c -o dbCache -L . -lConnectToSpread $(LIBS) -lsqlite3

spreadSink:	sink.o hash.o  connectToSpread.o
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) sink.o hash.o connectToSpread.o -o spreadSink $(LIBS)

socketToMYSQL: socketToMYSQL.c
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) socketToMYSQL.c -o socketToMYSQL $(LIBS)

sink.o:	sink.c mine.h hash.h hash.o
	$(CC) $(INC) -c $(CFLAGS) -Wall sink.c -o sink.o 

mine:	mine.o hash.o connectToSpread.o
	$(CC) $(INC) $(CFLAGS) mine.o hash.o connectToSpread.o -o mine -lpthread -lspread -ldl -lficl -lm

connectToSpread.o:	connectToSpread.c connectToSpread.h
	$(CC) $(INC) -c $(CFLAGS) connectToSpread.c -o connectToSpread.o

commonToSpread.o:	commonToSpread.c commonToSpread.h
	$(CC) $(INC) -c $(CFLAGS) commonToSpread.c -o commonToSpread.o

libcommonToSpread.so:	commonToSpread.o
	gcc -fPIC -shared -Wl,-soname,libcommonToSpread.so -o libcommonToSpread.so commonToSpread.o $(LIBS) -lsqlite3

libConnectToSpread.so:	connectToSpread.o hash.o
	gcc -fPIC -shared -Wl,-soname,libConnectToSpread.so -o libConnectToSpread.so connectToSpread.o hash.o -lspread

mine.o:	mine.c mine.h
	$(CC) $(INC) -c $(CFLAGS) mine.c -o mine.o

PthreadsExample:	PthreadsExample.c
	$(CC) $(CFLAGS) PthreadsExample.c -o PthreadsExample -lpthread -ldl

hash.o:	hash.h hash.c mine.h
	$(CC) $(CFLAGS) -c hash.c -o hash.o

clean:
	rm -f $(BINS) *.o *~ cscope.out libConnectToSpread.so libcommonToSpread.so

install:	$(BINS)
	mkdir -p ~/Source/bin
	cp $(BINS) ~/Source/bin

package:
	./install-deb-i686
docs:
	doxygen
backup:	clean
	./backup.sh
