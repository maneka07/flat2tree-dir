DNAME=libtransfname.so
SNAME=libtransfname.a
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

CFLAGS=-Wall -g -Wno-comment 
CC=gcc

all: $(SNAME) $(DNAME)

$(SNAME): $(OBJ)
	ar rcs $(SNAME) $(OBJ)
	ranlib $(SNAME)

$(DNAME): $(OBJ)
	$(CC) -shared -o $@ $(OBJ) -lssl -lcrypto -ldl 
	#-lm 

%.o: %.c
	$(CC) -c -fPIC $< $(CFLAGS)

clean:
	rm -f $(OBJ) $(SNAME) $(DNAME)
