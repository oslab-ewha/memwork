#include "simrts.h"

static LIST_HEAD(tasks);

static task_t *
create_task(unsigned wcet, unsigned period, unsigned memreq)
{
	task_t	*task;

	task = (task_t *)calloc(1, sizeof(task_t));
	task->wcet = wcet;
	task->period = period;
	task->memreq = memreq;
	INIT_LIST_HEAD(&task->list);

	return task;
}

static BOOL
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
	requeue_task(task);
	if (!is_schedulable()) {
		errmsg("unschedulable task detected");
		return FALSE;
	}
	return TRUE;
}

task_t *
get_edf_task(void)
{
	return NULL;
}

void
schedule_task(task_t *task)
{
	/// unsigned	time_runnable;

	//// time_runnable = get_runnable_interval(task);
}

void
calc_det(task_t *task)
{
	task->det = (int)(task->wcet * cpufreqs[task->idx_cpufreq - 1].wcet_scale * mems[task->mem_type - 1].wcet_scale);
}

void
requeue_task(task_t *task)
{
	struct list_head	*lp;
	unsigned	sum_det = 0;

	list_for_each (lp, &tasks) {
		task_t	*til = list_entry(lp, task_t, list);
		if (task->period < sum_det + til->det)
			break;
		sum_det += til->det;
	}
	list_add_tail(&task->list, &tasks);
}
