all: memwork

CFLAGS = -g -Wall -DDEBUG

POLICY = dvshm dvsdram hm dram fixed dvshm_greedy hm_chk

SIMRTS_OBJS = memwork.o task.o cpu.o mem.o power.o conf.o report.o output.o
POLICY_OBJS = $(POLICY:%=policy_%.o)

memwork: $(SIMRTS_OBJS) $(POLICY_OBJS)
	gcc -o $@ $^ -lm

$(SIMRTS_OBJS): simrts.h ecm_list.h
$(POLICY_OBJS): simrts.h ecm_list.h

tarball:
	tar czvf simrts.tgz *.[ch] *.tmpl Makefile

clean:
	rm -f *.o memwork
