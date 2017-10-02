
CC= gcc
CFLAGS= -Wall -g
LDLIBS= -lm
COMMONSRCS= sighandler.c ipchelper.c filehelper.c
OSSSRCS = oss.c
CHILDSRCS = child.c
# or perhaps SOURCES= $(wildcard *.c)

OBJECTS= $(patsubst %.c,%.o,$(COMMONSRCS))

.PHONY: all clean

all: oss child 

oss: $(OBJECTS)

child: $(OBJECTS)

$(OBJECTS): sighandler.h ipchelper.h filehelper.h

clean:
	$(RM) $(OBJECTS) *.o oss child
