#include "simrts.h"

#ifdef DEBUG

void
print_queued_tasks(void)
{
	struct list_head	*lp;

	printf("#wcet period memreq idle    det deadline\n");
	list_for_each (lp, &tasks) {
		task_t	*task;

		task = list_entry(lp, task_t, list);
		printf("%5u %5u %5u %5u %5u %5u\n",
		       task->wcet, task->period, task->memreq,
		       task->idle, task->det, task->deadline);
	}
}

#endif
