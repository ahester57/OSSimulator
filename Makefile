
CC= gcc
CFLAGS= -Wall -g
LDLIBS= -pthread
COMMONSRCS= sighandler.c ipchelper.c filehelper.c procsched.c proccntl.c
OSSSRCS = oss.c
CHILDSRCS = child.c
# or perhaps SOURCES= $(wildcard *.c)

OBJECTS= $(patsubst %.c,%.o,$(COMMONSRCS))

.PHONY: all clean

all: oss child 

oss: $(OBJECTS)

child: $(OBJECTS)

$(OBJECTS): sighandler.h ipchelper.h filehelper.h procsched.h osstypes.h proccntl.h

clean:
	$(RM) $(OBJECTS) *.o *log oss child
