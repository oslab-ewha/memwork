#include "simrts.h"

const char *
desc_task(task_t *task)
{
	static char	buf[1024];

	snprintf(buf, 1024, "[%2u: %u,%u,%u] ", task->no, task->wcet, task->period, task->memreq);
	return buf;
}

void
show_queued_tasks(void)
{
	struct list_head	*lp;
	unsigned	ticks;

	printf("%.3lf %5u: ", get_tasks_ndet(), simtime);

	ticks = simtime;
	list_for_each (lp, &tasks) {
		task_t	*task;
		unsigned	start, deadline;

		task = list_entry(lp, task_t, list_sched);
		start = ticks + task->gap_head;
		deadline = ticks + task->gap_head + task->deadline;
		printf("[%2u: %u,%u,%u] ", task->no, start, start + task->det_remain, deadline);
		ticks += (task->gap_head + task->gap);
	}

	printf("\n");
}
