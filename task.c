#include "simrts.h"

LIST_HEAD(tasks);

unsigned	n_tasks;

static task_t *
create_task(unsigned wcet, unsigned period, unsigned memreq)
{
	task_t	*task;

	task = (task_t *)calloc(1, sizeof(task_t));
	task->wcet = wcet;
	task->period = period;
	task->memreq = memreq;
	INIT_LIST_HEAD(&task->list);

	n_tasks++;
	task->no = n_tasks;

	return task;
}

BOOL
is_schedulable(void)
{
	struct list_head	*lp;
	double	sum_ndet = 0;

	list_for_each (lp, &tasks) {
		task_t	*task;
		double	ndet;

		task = list_entry(lp, task_t, list);
		ndet = task->det * 1.0 / task->period;
		sum_ndet += ndet;
	}
	if (sum_ndet > 1)
		return FALSE;
	return TRUE;
}

BOOL
insert_task(unsigned wcet, unsigned period, unsigned memreq)
{
	task_t	*task;

	task = create_task(wcet, period, memreq);

	if (!policy->assign_task(task)) {
		errmsg("insufficient memory");
		return FALSE;
	}
	calc_det(task);
	if (!requeue_task(task)) {
		errmsg("unschedulable task detected");
		return FALSE;
	}
	if (!is_schedulable()) {
		errmsg("unschedulable task detected");
		return FALSE;
	}

	return TRUE;
}

task_t *
get_edf_task(void)
{
	task_t	*task;

	if (list_empty(&tasks))
		return NULL;
	return list_entry(tasks.next, task_t, list);

	return task;
}

BOOL
schedule_task(task_t *task)
{
	if (!policy->reassign_task(task)) {
		return FALSE;
	}

	calc_idle_power_consumed(task->idle);

	simtime += task->idle;
	task->deadline -= task->idle;
	task->idle = 0;

	list_del_init(&task->list);
	calc_active_power_consumed(task);
	simtime += task->det;
	task->deadline -= task->det;

	return requeue_task(task);
}

void
calc_det(task_t *task)
{
	task->det = (int)(task->wcet / (cpufreqs[task->idx_cpufreq - 1].wcet_scale * mems[task->mem_type - 1].wcet_scale));
}

BOOL
requeue_task(task_t *task)
{
	struct list_head	*lp;
	unsigned	ticks = 0;

	ASSERT(list_empty(&task->list));

	task->deadline += task->period;

	list_for_each (lp, &tasks) {
		task_t	*til = list_entry(lp, task_t, list);
		if (task->deadline < ticks + til->idle + til->det) {
			task->deadline -= ticks;
			if (task->deadline < til->idle) {
				task->idle = task->deadline - task->det;
				til->idle = til->idle - task->deadline;
			}
			else {
				if (til->idle < task->det)
					return FALSE;
				task->idle = til->idle - task->det;
				til->idle = 0;
			}
			list_add_tail(&task->list, &til->list);
			return TRUE;
		}
		ticks += (til->idle + til->det);
	}

	task->deadline -= ticks;
	if (task->deadline < task->det)
		return FALSE;
	task->idle = task->deadline - task->det;
	list_add(&task->list, &tasks);

	return TRUE;
}
