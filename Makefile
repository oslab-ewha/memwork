all: simrts

CFLAGS = -g -Wall -DDEBUG

simrts: simrts.o task.o conf.o mem.o power.o report.o dfdm.o dvs.o dmem.o fast.o output.o
	gcc -o simrts $^ -lm

simrts.o: simrts.h ecm_list.h
task.o: simrts.h ecm_list.h
conf.o: simrts.h ecm_list.h
mem.o: simrts.h ecm_list.h
power.o: simrts.h ecm_list.h
report.o: simrts.h ecm_list.h
dfdm.o: simrts.h ecm_list.h
dvs.o: simrts.h ecm_list.h
dmem.o: simrts.h ecm_list.h
fast.o: simrts.h ecm_list.h
output.o: simrts.h ecm_list.h

tarball:
	tar czvf simrts.tgz *.[ch] *.tmpl Makefile
