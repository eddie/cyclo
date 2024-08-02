# vim: set noexpandtab tabstop=8 shiftwidth=8:
CC ?= c99
CFLAGS = -Wall -Wextra -std=c11 -pedantic -Wshadow
LDFLAGS = -lc 


all: assembler emulator

assembler: assembler.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

emulator: emulator.c video.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)


# when looking for something that ends in .o, look
# for the same thing ending in .c and run gcc on it
%.o: %.c 
	$(CC) $(CFLAGS) -c -o $@ $<

asm/%.bin: asm/%.asm
	./assembler $< $@

clean:
	rm emulator
	rm assembler
	rm *.o

format: $(SOURCES)
	clang-format -i -style=file hello.c


.PHONY: all clean
