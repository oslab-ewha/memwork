#include "simrts.h"

#define TASK_RET(task) (((task)->det_remain < (task)->gap) ? (task)->det_remain: (task)->gap)

LIST_HEAD(tasks);

unsigned	n_tasks;

static task_t *
create_task(unsigned wcet, unsigned period, unsigned memreq, double mem_active_ratio)
{
	task_t	*task;

	task = (task_t *)calloc(1, sizeof(task_t));
	task->wcet = wcet;
	task->period = period;
	task->memreq = memreq;
	task->mem_active_ratio = mem_active_ratio;
	INIT_LIST_HEAD(&task->list_sched);

	n_tasks++;
	task->no = n_tasks;

	return task;
}

void
calc_task_det(task_t *task)
{
	double		det_new_dbl;

	det_new_dbl = task->wcet / (cpufreqs[task->idx_cpufreq - 1].wcet_scale * mems[task->mem_type - 1].wcet_scale);

	task->det_old = task->det;
	task->det = (int)round(det_new_dbl);
	if (task->det == 0)
		task->det = 1;

	task->det_remain_old = task->det_remain;
	if (task->det_remain > 0 && task->det != task->det_old) {
		task->det_remain = (int)round(task->det_remain * (det_new_dbl / task->det_old));
		ASSERT(task->det_remain != 0);/////TEST
		ASSERT(task->det_remain <= task->det);////TEST
	}
}

void
revert_task_det(task_t *task)
{
	task->det = task->det_old;
	task->det_remain = task->det_remain_old;
}

double
get_tasks_ndet(void)
{
	struct list_head	*lp;
	double	sum_ndet = 0;

	list_for_each (lp, &tasks) {
		task_t	*til;
		double	ndet;

		til = list_entry(lp, task_t, list_sched);
		ndet = til->det * 1.0 / til->period;
		sum_ndet += ndet;
	}
	return sum_ndet;
}

BOOL
is_schedulable(task_t *task)
{
	struct list_head	*lp;
	double	sum_ndet = 0;

	ASSERT(&task->list_sched);

	list_for_each (lp, &tasks) {
		task_t	*til;
		double	ndet;

		til = list_entry(lp, task_t, list_sched);
		ndet = til->det * 1.0 / til->period;
		sum_ndet += ndet;
	}
	if (task != NULL)
		sum_ndet += (task->det * 1.0 / task->period);
	if (sum_ndet > 1)
		return FALSE;
	return TRUE;
}

void
insert_task(unsigned wcet, unsigned period, unsigned memreq, double mem_active_ratio)
{
	task_t	*task;

	task = create_task(wcet, period, memreq, mem_active_ratio);
	list_add_tail(&task->list_sched, &tasks);
}

BOOL
setup_tasks(void)
{
	LIST_HEAD(temp);
	struct list_head	*lp, *next;

	list_add(&temp, &tasks);
	list_del_init(&tasks);

	list_for_each_n (lp, &temp, next) {
		task_t	*task = list_entry(lp, task_t, list_sched);

		list_del_init(&task->list_sched);
		if (!policy->assign_task(task)) {
			errmsg("insufficient memory");
			return FALSE;
		}
		calc_task_det(task);
		if (!is_schedulable(task)) {
			errmsg("%s: unschedulable task", desc_task(task));
			return FALSE;
		}

		requeue_task(task, 0);
	}

	return TRUE;
}

static task_t *
get_head_task(void)
{
	if (list_empty(&tasks))
		return NULL;
	return list_entry(tasks.next, task_t, list_sched);
}

task_t *
pop_head_task(void)
{
	task_t	*task;

	if (list_empty(&tasks))
		return NULL;
	task = list_entry(tasks.next, task_t, list_sched);
	list_del_init(&task->list_sched);
	return task;
}

static BOOL
is_preceding_task(task_t *task, task_t *task_cmp)
{
	double	etr, etr_cmp;

	etr = (task->det_remain * 1.0) / task->deadline;
	etr_cmp = (task_cmp->det_remain * 1.0) / task_cmp->deadline;

	if (etr > etr_cmp)
		return TRUE;
	if (etr < etr_cmp)
		return FALSE;
	if (task->no < task_cmp->no)
		return TRUE;
	return FALSE;
}

static unsigned
delay_tasks(task_t *task, task_t **ptarget)
{
	task_t		*target = *ptarget, *next = NULL;
	unsigned	det_remain_saved, deadline_saved;
	unsigned	i;

	det_remain_saved = task->det_remain;
	deadline_saved = task->deadline;

	if (target->list_sched.next != &tasks)
		next = list_entry(target->list_sched.next, task_t, list_sched);

	for (i = 0; i < task->det_remain; i++) {
		if (target->deadline == target->det_remain)
			break;
		if (is_preceding_task(target, task))
			break;

		target->deadline--;
		task->det_remain--;
		task->deadline--;

		if (next != NULL) {
			target->gap--;
			if (target->gap == 0) {
				if (is_preceding_task(target, next)) {
					target->gap = delay_tasks(target, &next);
				}
				else {
					task_t	*temp;

					list_del(&target->list_sched);
					list_add(&target->list_sched, &next->list_sched);
					target->gap = next->gap;
					next->gap = delay_tasks(next, &target);

					temp = target;
					target = next;
					next = temp;
				}
			}
		}
	}

	task->det_remain = det_remain_saved;
	task->deadline = deadline_saved;

	*ptarget = target;

	ASSERT(i != 0);////TEST
	return i;
}

static void
apply_gap_head(unsigned gap_head)
{
	task_t	*head;
	struct list_head	*lp;

	head = get_head_task();
	for (lp = head->list_sched.next; lp != &tasks; lp = lp->next) {
		task_t	*task = list_entry(lp, task_t, list_sched);

		ASSERT(task->deadline > gap_head);
		task->deadline -= gap_head;
		if (task->gap != 0)
			break;
	}
}

static unsigned
get_new_start(task_t *task, task_t *prev, unsigned start)
{
	unsigned	det_remain_saved, deadline_saved;
	unsigned	i;

	det_remain_saved = prev->det_remain;
	deadline_saved = prev->deadline;

	for (i = 0; i < TASK_RET(prev); i++) {
		if (task->deadline == task->det_remain)
			break;
		if (is_preceding_task(task, prev))
			break;
		prev->det_remain--;
		prev->deadline--;
		task->deadline--;
	}
	prev->det_remain = det_remain_saved;
	prev->deadline = deadline_saved;
	return start + i;
}

void
requeue_task(task_t *task, unsigned ticks)
{
	unsigned	start;
	struct list_head	*lp;

	ASSERT(list_empty(&task->list_sched));

	if (task->det_remain == 0) {
		start = task->gap_head + task->deadline;
		task->deadline += task->period;
		task->det_remain = task->det;

		ASSERT(task->deadline >= start);
		task->deadline -= start;
	}
	else {
		start = 0;
	}

	list_for_each (lp, &tasks) {
		task_t	*til = list_entry(lp, task_t, list_sched);
		unsigned	gap;

		if (start < ticks || (start == ticks && is_preceding_task(task, til))) {
			if (lp->prev != &tasks) {
				task_t	*prev = list_entry(lp->prev, task_t, list_sched);
				prev->gap -= (ticks - start);
				ASSERT(prev->gap > 0);///TEST
			}

			if (ticks <= start)
				task->gap = delay_tasks(task, &til);
			else
				task->gap = ticks - start;

			list_add_tail(&task->list_sched, &til->list_sched);
			if (task->list_sched.prev == &tasks) {
				task->gap_head = start;
				apply_gap_head(start);
			}
			else
				task->gap_head = 0;

			return;
		}

		gap = til->gap_head + til->gap;
		start = get_new_start(task, til, start);
		ticks += gap;
	}

	task->gap = task->det_remain;

	if (list_empty(&tasks))
		task->gap_head = start;
	else {
		task_t	*last = list_entry(tasks.prev, task_t, list_sched);

		task->gap_head = 0;
		last->gap = (unsigned)((int)last->gap - ((int)ticks - (int)start));
	}

	list_add_tail(&task->list_sched, &tasks);
}

BOOL
schedule_task(task_t *task)
{
	unsigned	ret;	/* real execution time */

	simtime += task->gap_head;
	calc_idle_power_consumed_mem(task->gap_head);
	calc_idle_power_consumed_task(task, task->gap_head);
	task->gap_head = 0;

	/* processing */
	if (!policy->reassign_task(task)) {
		errmsg("%s: failed to reassign", desc_task(task));
		return FALSE;
	}

	ret = TASK_RET(task);
	if (ret > task->deadline) {
		errmsg("%s: deadline violated", desc_task(task));
		return FALSE;
	}

	simtime += ret;
	calc_active_power_consumed(task, ret);

	task->det_remain -= ret;
	task->deadline -= ret;
	task->gap -= ret;

	if (task->gap > 0) {
		unsigned	idle = task->gap;

		if (idle >= task->deadline)
			idle = task->deadline;
		simtime += idle;
		calc_idle_power_consumed_mem(idle);
		calc_idle_power_consumed_task(task, idle);
		task->deadline -= idle;
		task->gap -= idle;
	}

	requeue_task(task, task->gap);

	return TRUE;
}

void
check_queued_tasks(void)
{
	unsigned	ticks = simtime;
	struct list_head	*lp;

	list_for_each (lp, &tasks) {
		task_t	*task = list_entry(lp, task_t, list_sched);
		unsigned	deadline;

		if (task->det == 0 || task->det_remain == 0) {
			show_queued_tasks();
			FATAL(3, "%s: zero det", desc_task(task));
		}
		if (task->det < task->det_remain) {
			show_queued_tasks();
			FATAL(3, "%s: invalid det", desc_task(task));
		}
		if (task->gap == 0) {
			show_queued_tasks();
			FATAL(3, "%s: zero gap", desc_task(task));
		}

		deadline = ticks + task->gap_head + task->deadline;
		if (deadline % task->period != 0) {
			show_queued_tasks();
			FATAL(3, "%s: invalid deadline", desc_task(task));
		}
		ticks += (task->gap_head + task->gap);
	}
}

void
reinit_tasks(void)
{
	struct list_head	*lp;

	list_for_each (lp, &tasks) {
		task_t	*til;

		til = list_entry(lp, task_t, list_sched);
		til->det = 0;
		til->det_remain = 0;
		til->gap_head = 0;
		til->gap = 0;
		til->deadline = 0;
	}
}
