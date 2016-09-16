.KEEP_STATE:

EG = /eurogam

OS=Linux64

%.o:	../%.c
	$(COMPILE.c)  -o $@ $<

CC = gcc
CFLAGS    = -O -DUNIX -DLINUX -DPOSIX
INCLUDES = -I..

COMPILE.c = $(CC) $(CFLAGS) $(CPPFLAGS) $(INCLUDES) -fPIC -c
LINK.so=   $(CC) $(CFLAGS) $(CPPFLAGS) $(INCLUDES) -shared

OBJECTS   = dataspy.o

TARGET = libdataspy.so

all: $(TARGET)


$(TARGET): $(OBJECTS)
	$(LINK.so) $(OBJECTS) $(OUTPUT_OPTION) -o $@ $(LDLIBS)
	@echo "made Linux (64 bit) data spy library"

install:
	cp $(TARGET) $(EG)/Linux/lib64
	cp $(TARGET) /MIDAS/Linux/lib64

clean:
	rm -f *.o *% 

