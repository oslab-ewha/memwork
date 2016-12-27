all: simrts

CFLAGS = -g -Wall

simrts: simrts.o
	gcc -o simrts $<
