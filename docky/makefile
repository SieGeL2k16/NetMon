#
# Makefile for NetMon.docky
# Based on my own Linux makefile

HOST     = `type env:Hostname`
OSTYPE   = Workbench `type env:Workbench`|Kickstart `type env:Kickstart`

PRG      = NetMon.docky
OBJS     = NetMon.docky.o
DEPS     = NetMon.docky_rev.h

#-----------------------------------------------------------------------------

CC       = @gcc
CFLAGS   = -ggdb -DDEBUG -Wunused -nostartfiles -c 
LFLAGS	 = -nostartfiles -lm
RM       = @Delete
ECH      = @Echo
DIR      = @List
COPY     = @Copy
CLS      = C:cls
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
