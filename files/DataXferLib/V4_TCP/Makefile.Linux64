.KEEP_STATE:

EG = /eurogam

%.o:	../%.c
	$(COMPILE.c)  -o $@ $<

CC = gcc
CFLAGS    = -O -DUNIX -DLINUX -DUSEPTHREAD -DSIXTYFOUR_BIT -DTSOVERLAP
INCLUDES = -I.. -I../../../DataStorage

COMPILE.c = $(CC) $(CFLAGS) $(CPPFLAGS) $(INCLUDES) -fPIC -c
LINK.so=   $(CC) $(CFLAGS) $(CPPFLAGS) $(INCLUDES) -shared

OBJECTS   = transfer.o

TARGET = libxfer.so

all: $(TARGET)


$(TARGET): $(OBJECTS)
	$(LINK.so) $(OBJECTS) $(OUTPUT_OPTION) -o $@ $(LDLIBS)
	@echo "made Linux (64 bit) data transfer client library"

install:
	cp $(TARGET) $(EG)/Linux/lib64
	cp $(TARGET) /MIDAS/Linux/lib64

clean:
	rm -f *.o *% 

