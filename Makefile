PWD = $(shell pwd)
CC = g++
LINKER = g++
CPPFLAGS = -Wall -O2 -g
LINKER_FLAG = -lpthread
LIBS =
INCLUDE =

OBJECTS = miswork.o \
    main.o

CORE_DEPS = miswork.h \
    work.h

CORE_INC = -I ./

default: mis_server clean

mis_server: $(OBJECTS)
	$(LINKER) -o $@ $(OBJECTS) $(LINKER_FLAG)

miswork.o: $(CORE_DEPS) \
    miswork.cpp
	$(CC) -c $(CPPFLAGS) $(CORE_INC) -o $@ miswork.cpp

main.o: $(CORE_DEPS) \
    main.cpp
	$(CC) -c $(CPPFLAGS) $(CORE_INC) -o $@ main.cpp

clean:
	rm $(OBJECTS)
