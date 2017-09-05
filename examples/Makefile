CC = gcc
CFLAGS = -g -Wall
HEADERS = ../ppm.h

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

all: ppm_writer ppm_reader

ppm_writer: ppm_writer.o
	$(CC) $^ -Wall -o $@

ppm_reader: ppm_reader.o
	$(CC) $^ -Wall -o $@

clean:
	-rm -f *.o
	-rm -f ppm_writer ppm_reader
