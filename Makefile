
# CFLAGS=-g -m32 -DFICL # -Wall
CFLAGS=-g -fPIC # -m32 
# CFLAGS=-g # -Wall
INC=-I/usr/local/include
# 
# Don'tbuild old utilities.  Retained for future reference.
# 
# BINS=spreadSource spreadSink lightSink lightSource # mine # PthreadsExample 
BINS=redisSender dbCache tstLib toSpread lightSink lightSource socketToMYSQL spreadToSocket spreadToMySQL socketSpreadBridge # mine # PthreadsExample spreadSource spreadSink 
# LIBS=-lpthread -lspread -ldl -L/usr/local/lib -lficl -lm
LIBS=-lpthread -lspread -ldl -lm -ljansson 
LFLAGS=-Wl,--no-as-needed 

all:	$(BINS)

toSpread:	main.c libConnectToSpread.so
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) main.c -o toSpread -L . -lConnectToSpread $(LIBS) -lsqlite3
#	$(CC) $(INC) $(CFLAGS) $(LFLAGS) -o toSpread main.c -lspread -L/usr/local/lib \
#	-lsqlite3 -lpthread 

tstLib:	tstLib.c libConnectToSpread.so
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) tstLib.c -o tstLib -L . -lConnectToSpread $(LIBS)
#	$(CC) $(INC) $(CFLAGS) $(LFLAGS) tstLib.c -o tstLib -lConnectToSpread $(LIBS)

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



redisSender:	redisSender.c libConnectToSpread.so
#	$(CC) $(INC) $(CFLAGS) $(LFLAGS) -o redisSender redisSender.c -L/usr/local/lib -L . -lConnectToSpread $(LIBS)
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) -o redisSender redisSender.c -L . -lConnectToSpread $(LIBS)

spreadSource:	source.o hash.o  connectToSpread.o
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) source.o hash.o connectToSpread.o -o spreadSource $(LIBS)

source.o:	source.c mine.h hash.h hash.o
	$(CC) $(INC) -c $(CFLAGS) -Wall source.c -o source.o 

dbCache:	dbCache.c libConnectToSpread.so
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) dbCache.c -o dbCache -L . -lConnectToSpread $(LIBS) -lsqlite3

spreadSink:	sink.o hash.o  connectToSpread.o
#	$(CC) $(INC) $(CFLAGS) sink.o hash.o connectToSpread.o -o spreadSink -lpthread -lspread -ldl -lm  -lficl
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) sink.o hash.o connectToSpread.o -o spreadSink $(LIBS)

socketToMYSQL: socketToMYSQL.c
	$(CC) $(INC) $(CFLAGS) $(LFLAGS) socketToMYSQL.c -o socketToMYSQL $(LIBS)


sink.o:	sink.c mine.h hash.h hash.o
	$(CC) $(INC) -c $(CFLAGS) -Wall sink.c -o sink.o 

mine:	mine.o hash.o connectToSpread.o
	$(CC) $(INC) $(CFLAGS) mine.o hash.o connectToSpread.o -o mine -lpthread -lspread -ldl -lficl -lm

connectToSpread.o:	connectToSpread.c connectToSpread.h
	$(CC) $(INC) -c $(CFLAGS) connectToSpread.c -o connectToSpread.o

libConnectToSpread.so:	connectToSpread.o hash.o
	gcc -fPIC -shared -Wl,-soname,libConnectToSpread.so -o libConnectToSpread.so connectToSpread.o hash.o -lspread

mine.o:	mine.c mine.h
	$(CC) $(INC) -c $(CFLAGS) mine.c -o mine.o

PthreadsExample:	PthreadsExample.c
	$(CC) $(CFLAGS) PthreadsExample.c -o PthreadsExample -lpthread -ldl

hash.o:	hash.h hash.c mine.h
	$(CC) $(CFLAGS) -c hash.c -o hash.o
clean:
	rm -f $(BINS) *.o *~ cscope.out libConnectToSpread.so

install:	$(BINS)
	mkdir -p ~/Source/bin
	cp $(BINS) ~/Source/bin

package:
	./install-deb-i686
docs:
	doxygen
backup:	clean
	./backup.sh
