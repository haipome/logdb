CC= gcc

CFLAGS+= $(CFLAG)
CFLAGS+= -g
CFLAGS+= -Wall -Wextra -Wformat=2 -Wunused -Wno-unused-parameter -Wshadow \
		 -Wwrite-strings -Wstrict-prototypes -Wold-style-definition \
		 -Wnested-externs

RM= rm -f

INC_MYSQL= -I/usr/include/mysql/
LIB_MYSQL= -L/usr/lib/mysql/ -lmysqlclient -lz

INC_ALL= $(INC_MYSQL)
LIB_ALL= $(LIB_MYSQL) -lm

SERVER_O= main.o conf.o job.o db.o dlog.o ini.o net.o queue.o serialize.o sql.o utils.o seq.o api.o protocol.o utf8.o
SERVER= logdb

INTERFACE_O= inf.o dlog.o ini.o net.o queue.o serialize.o utils.o timer.o cache.o bhash.o protocol.o
INTERFACE= loginf

all: $(SERVER) $(INTERFACE)

$(SERVER): $(SERVER_O)
	$(CC) $(CFLAGS) -o $@ $(SERVER_O) $(LIB_ALL)

$(INTERFACE): $(INTERFACE_O)
	$(CC) $(CFLAGS) -o $@ $(INTERFACE_O) $(LIB_ALL)

.c.o:
	$(CC) $(CFLAGS) -c $^ $(INC_ALL)

clean:
	$(RM) *.o $(SERVER) $(INTERFACE)

