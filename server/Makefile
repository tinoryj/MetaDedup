CC = g++
CFLAGS = -O3 -Wall #-g2 -ggdb
LIBS = -lcrypto -lssl -lpthread -lsnappy #-pg -lc
INCLUDES = -I./lib/leveldb/include -I./backend/ -I./utils/ -I./lib/cryptopp -I./comm/ -I./dedup/ 
MAIN_OBJS = ./utils/CryptoPrimitive.o ./dedup/DedupCore.o ./dedup/minDedupCore.o ./backend/BackendStorer.o ./comm/server.o

all: leveldb server
	$(shell ! test -d "meta" && mkdir meta)
	$(shell ! test -d "meta/DedupDB" && mkdir meta/DedupDB)
	$(shell ! test -d "meta/minDedupDB" && mkdir meta/minDedupDB)
	$(shell ! test -d "meta/ShareContainers" && mkdir meta/ShareContainers)
	$(shell ! test -d "meta/minShareContainers" && mkdir meta/minShareContainers)
	$(shell ! test -d "meta/RecipeFiles" && mkdir meta/RecipeFiles)
	$(shell ! test -d "meta/keystore" && mkdir meta/keystore)

leveldb:
	make -C ./lib/leveldb

%.o: %.cc %.hh
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

server: main.cc $(MAIN_OBJS)  
	$(CC) $(CFLAGS) $(INCLUDES) -o SERVER main.cc $(MAIN_OBJS) ./lib/leveldb/libleveldb.a $(LIBS)


clean:
	@rm -f SERVER
	@rm -f $(MAIN_OBJS)
	@rm -rf meta/
