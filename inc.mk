
PREFIX = ../PullerModule
LIBDIR = $(PREFIX)/lib

Debug = -g
Release = 

#### Change the following for your environment:
COMPILE_OPTS =	$(Debug)	$(INCLUDES)  -fPIC -I. -O2 -DSOCKLEN_T=socklen_t -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64
C =		c
C_COMPILER =		cc  # replace with "mipsel-linux-cc" for mipsel platform
C_FLAGS =		$(COMPILE_OPTS)
CPP =			cpp
CPLUSPLUS_COMPILER =	g++ # replace with "mipsel-linux-g++" for mipsel platform
CPLUSPLUS_FLAGS =	$(COMPILE_OPTS) -Wall -DBSD=1
OBJ =			o
LINK =			g++ -o  # replace with "mipsel-linux-g++ -o" for mipsel platform
LINK_OPTS =		-L.
CONSOLE_LINK_OPTS =	$(LINK_OPTS)
LIBRARY_LINK =		ar cr #replace with "mipsel-linux-ar cr" for mipsel platform
LIBRARY_LINK_OPTS =	
LIB_SUFFIX =			a
LIBS_FOR_CONSOLE_APPLICATION =
LIBS_FOR_GUI_APPLICATION =
EXE =
##### End of variables to change
