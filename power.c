#include "simrts.h"

double	power_consumed;

void
calc_idle_power_consumed(unsigned idle)
{
	struct list_head	*lp;

	if (idle == 0)
		return;
	list_for_each (lp, &tasks) {
		task_t	*task = list_entry(lp, task_t, list);

		power_consumed += (idle * (cpufreqs[task->idx_cpufreq - 1].power_idle + mems[task->mem_type - 1].power_idle));
	}
}

void
calc_active_power_consumed(task_t *task)
{
	power_consumed += (task->det * (cpufreqs[task->idx_cpufreq - 1].power_active + mems[task->mem_type - 1].power_active));
	calc_idle_power_consumed(task->det);
}
