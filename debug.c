#include "simrts.h"

#ifdef DEBUG

void
print_queued_tasks(void)
{
	struct list_head	*lp;
	unsigned	ticks;

	printf("%5u: ", simtime);

	ticks = simtime;
	list_for_each (lp, &tasks) {
		task_t	*task;

		task = list_entry(lp, task_t, list);
		printf("[%2u: %u,%u,%u] ", task->no, ticks + task->idle, ticks + task->idle + task->det, ticks + task->deadline);
		ticks += (task->idle + task->det);
	}

	printf("\n");
}

#endif
