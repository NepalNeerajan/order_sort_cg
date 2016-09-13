.KEEP_STATE:

OS=Linux64

##COMPILE.C=	$(CC)  $(CFLAGS) $(CPPFLAGS) -c $(INCLUDES)
##LINK.cc=        $(CC) 

# Compiler.
CC          	= g++
# Flags for compiler.
CFLAGS		= -c -Wall -Wextra -DUNIX -DLINUX -DLINUX64 -DPOSIX
# Flags for linker.
LDFLAGS		=-lrt


ROOTCONFIG   := $(shell which root-config)
ROOTCINT     := $(shell which rootcint)

ROOTCFLAGS   := $(shell $(ROOTCONFIG) --cflags)
ROOTLDFLAGS  := $(shell $(ROOTCONFIG) --ldflags)
ROOTLIBS     := $(shell $(ROOTCONFIG) --glibs)
#aeROOTLIBS     += $(shell $(ROOTCONFIG) --glibs)
#aeLIBS         := $(USERLIBS) $(ROOTLIBS) $(LIBS)
LIBS         := $(ROOTLIBS)

# Add root flags.
CFLAGS 		+= $(ROOTCFLAGS)
# Add root.
LDFLAGS 	+= $(ROOTLDFLAGS) $(ROOTLIBS)

#INCLUDES=  -I/MIDAS/DataPackage/DataXferLib/V4_TCP -I/MIDAS/DataPackage/DataSpyLib
INCLUDES=  -I/Disk/ds-sopa-group/np/RIKEN/AIDAsort/DataXferLib/V4_TCP -I/scratch/cgriffin/DataSpyLib
#INCLUDES=  -I/Disk/ds-sopa-group/np/RIKEN/AIDAsort/DataXferLib/V4_TCP -I/Disk/ds-sopa-group/np/RIKEN/AIDAsort/DataSpyLib
#INCLUDES=  -I../../DataSpyLib/Linux64
LDLIBS=   -L/usr/ucblib -lrt -lpthread -L/scratch/cgriffin/MIDAS/Linux/lib64 -lxfer -ldataspy
#LDLIBS=   -L/usr/ucblib -lrt -lpthread -L/Disk/ds-sopa-group/np/RIKEN/AIDAsort/MIDAS/Linux/lib64 -lxfer -ldataspy
#LDLIBS=   -L/usr/ucblib -lrt -lpthread -L/MIDAS/Linux/lib64  -ldataspy

# The object files.
OBJECTS =   DataSource.o Unpacker.o Calibrator.o Analysis.o
 
#ANALYSIS =  AnalysisPulse.o  
#OBJECTS += $(ANALYSIS)

# Specify the file to search for .cc/.h files.
vpath %.cpp ./src
vpath %.h ./include

#AIDA-unpack.exe : main.o $(OBJECTS)
AIDAsort.v2.exe : main.o $(OBJECTS)
	$(CC) $(LDFLAGS)  $(LDLIBS) -o $@ $^
#	$(CC) $(LDFLAGS) $(LIBS) -o $@ $^

main.o : main.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -Iinclude/ $^


DataSource.o : DataSource.h DataSource.cpp 
	$(CC) $(CFLAGS) $(INCLUDES) -Iinclude/ $^


Unpacker.o : Unpacker.h Unpacker.cpp 
	$(CC) $(CFLAGS) $(INCLUDES) -Iinclude/ $^

Calibrator.o : Calibrator.h Calibrator.cpp 
	$(CC) $(CFLAGS) $(INCLUDES) -Iinclude/ $^

Analysis.o : Analysis.h Analysis.cpp 
	$(CC) $(CFLAGS) $(INCLUDES) -Iinclude/ $^

#MyStyle.o : MyStyle.h MyStyle.cc
#	$(CC) $(CFLAGS) -Iinclude/ $^

## ## ## 
## THE ANALYSES.
## ## ## 

#AnalysisPulse.o : AnalysisPulse.h AnalysisPulse.cc
#	$(CC) $(CFLAGS) -Iinclude/ $^

clean:
	rm -vf *.exe *.o *~ include/*.gch
