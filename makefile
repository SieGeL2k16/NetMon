#
# Makefile for NetMon
# Based on my own Linux makefile
# Code is compiled with -D__USE_INLINE__ to have OS 3.x compatibility!

HOST     = `type env:Hostname`
OSTYPE   = Workbench `type env:Workbench`|Kickstart `type env:Kickstart`

PRG      = NetMon
OBJS     = main.o rsfuncs.o NetMon_rev.o
DEPS     = includes.h structs.h NetMon_rev.h

#-----------------------------------------------------------------------------

CC       = @gcc
#CFLAGS   = -D__USE_INLINE__ -DCPU_TYPE="PPC" -O2 -Wunused -c
#LFLAGS	 = -lm
CFLAGS   = -D__USE_INLINE__ -DCPU_TYPE="PPC" -O3 -Wunused -mcrt=newlib -c
LFLAGS	 = -lm -mcrt=newlib
RM       = @Delete
ECH      = @Echo
DIR      = @List
COPY     = @Copy
CLS      = C:cls
#INCLUDE = -Isdk:Local/include 
#INCLUDE  = -ISDK:Local/Include 
COMPILER = gcc `gcc -dumpversion`
STRIP		 = @strip
#-----------------------------------------------------------------------------

.PHONY:  all clean full

all:     cls $(PRG)
	$(ECH) "all done.*N"
	$(ECH) "Executable Size: " NOLINE
	$(DIR) $(PRG) LFORMAT="%L Bytes"
	$(ECH) ""

strip:	cls
	$(ECH) "Removing Symbols from $(PRG)*N"
	$(ECH) "Size before strip: " NOLINE
	$(DIR) $(PRG) LFORMAT="%L Bytes"
	$(STRIP) $(PRG)
	$(ECH) "Size after  strip: " NOLINE
	$(DIR) $(PRG) LFORMAT="%L Bytes"
	$(ECH) "*NAll done.*N"

newlib:     cls $(PRG)
	$(ECH) "all done.*N"
	$(ECH) "Executable Size: " NOLINE
	$(DIR) $(PRG) LFORMAT="%L Bytes"
	$(ECH) ""


$(PRG):  $(OBJS)
	$(ECH) "*NLinking..." NOLINE
	$(CC) -o $(PRG) $(INCLUDE) $(OBJS) $(LFLAGS)

%.o: %.c $(DEPS)
	$(ECH) "Compiling" $<
	$(CC) $(CFLAGS) $(INCLUDE) $<

clean:
	$(ECH) "Cleaning up:*N"
	$(RM)  $(OBJS) $(PRG)
	$(ECH) "*NAll Done."

full:    clean all

cls:
	$(CLS)
	$(ECH) "Compiling $(PRG) on $(HOST) [$(OSTYPE)] with $(COMPILER)*N"
#-----------------------------------------------------------------------------
