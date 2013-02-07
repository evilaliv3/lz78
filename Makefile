CFLAGS=-O3 -Wall -Werror -g

BINARYNAME=lz78

OBJFILES=main.o wrapper.o lz78.o bitio.o

all: $(BINARYNAME)

$(BINARYNAME): $(OBJFILES)
	$(CC) -o $@ $^ 

main.o: wrapper.h bitio.h 
wrapper.o: wrapper.h lz78.h 
lz78.o: lz78.h bitio.h
bitio.o: bitio.h

clean:
	rm -rf $(OBJFILES) $(BINARYNAME)
