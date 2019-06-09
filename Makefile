all: simrts

CFLAGS = -g -Wall -DDEBUG

simrts: simrts.o task.o conf.o mem.o power.o report.o policy_dvshm.o policy_dvsdram.o policy_hm.o policy_dram.o policy_fixed.o output.o
	gcc -o simrts $^ -lm

simrts.o: simrts.h ecm_list.h
task.o: simrts.h ecm_list.h
conf.o: simrts.h ecm_list.h
mem.o: simrts.h ecm_list.h
power.o: simrts.h ecm_list.h
report.o: simrts.h ecm_list.h
policy_dvshm.o: simrts.h ecm_list.h
policy_dvsdram.o: simrts.h ecm_list.h
policy_hm.o: simrts.h ecm_list.h
poliyc_dram.o: simrts.h ecm_list.h
poliyc_fixed.o: simrts.h ecm_list.h
output.o: simrts.h ecm_list.h

tarball:
	tar czvf simrts.tgz *.[ch] *.tmpl Makefile

clean:
	rm -f *.o simrts
