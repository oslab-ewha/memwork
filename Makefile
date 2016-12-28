all: simrts

CFLAGS = -g -Wall

simrts: simrts.o task.o conf.o dfdm.o
	gcc -o simrts $^

simrts.o: simrts.h ecm_list.h
task.o: simrts.h ecm_list.h
conf.o: simrts.h ecm_list.h
dfdm.o: simrts.h ecm_list.h

