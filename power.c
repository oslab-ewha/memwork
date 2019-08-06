#include "simrts.h"

double	power_consumed_cpu_active;
double	power_consumed_mem_active;
double	power_consumed_cpu_idle;
double	power_consumed_mem_idle;

static void
calc_idle_power_consumed_cpu(unsigned idle)
{
	power_consumed_cpu_idle += (idle * cpufreqs[n_cpufreqs - 1].power_idle);
}

static void
calc_idle_power_consumed_task_mem(task_t *task, unsigned idle)
{
	power_consumed_mem_idle += (idle * task->memreq * mems[task->mem_type - 1].power_idle);
}

void
calc_idle_power_consumed_task(task_t *task, unsigned idle)
{
	calc_idle_power_consumed_cpu(idle);
	calc_idle_power_consumed_task_mem(task, idle);
}

void
calc_idle_power_consumed_mem(unsigned idle)
{
	struct list_head	*lp;

	if (idle == 0)
		return;
	list_for_each (lp, &tasks) {
		task_t	*task = list_entry(lp, task_t, list_sched);

		calc_idle_power_consumed_task_mem(task, idle);
	}
}

void
calc_active_power_consumed(task_t *task, unsigned ret)
{
	cpufreq_t	*cpufreq = &cpufreqs[task->idx_cpufreq - 1];
	mem_t	*mem = &mems[task->mem_type - 1];
	double	wcet_scaled_cpu = 1 / cpufreq->wcet_scale;
	double	wcet_scaled_mem = 1 / mem->wcet_scale;
	double	wcet_scaled = wcet_scaled_cpu + wcet_scaled_mem;

	power_consumed_cpu_active += (ret * cpufreq->power_active * wcet_scaled_cpu / wcet_scaled);
	power_consumed_cpu_idle += (ret * cpufreq->power_idle * wcet_scaled_mem / wcet_scaled);
	power_consumed_mem_active += (ret * mem->power_active * task->memreq * task->mem_active_ratio);
	power_consumed_mem_idle += (ret * mem->power_idle * task->memreq * (1 - task->mem_active_ratio));

	/* currently task is excluded from the list */
	ASSERT(list_empty(&task->list_sched));
	calc_idle_power_consumed_mem(ret);
}

double
calc_task_power_consumed(task_t *task)
{
	cpufreq_t	*cpufreq = &cpufreqs[task->idx_cpufreq - 1];
	mem_t	*mem = &mems[task->mem_type - 1];
	double	wcet_scaled_cpu = 1 / cpufreq->wcet_scale;
	double	wcet_scaled_mem = 1 / mem->wcet_scale;
	double	wcet_scaled = wcet_scaled_cpu + wcet_scaled_mem;
	double	power_active, power_idle;

	power_active = (cpufreq->power_active * wcet_scaled_cpu + cpufreq->power_idle * wcet_scaled_mem) / wcet_scaled;
	power_active += (mem->power_active * task->memreq * task->mem_active_ratio);
	power_active += (mem->power_idle * task->memreq * (1 - task->mem_active_ratio));
	power_idle = cpufreq->power_idle + task->memreq * mem->power_idle;

	return (power_active * task->det + power_idle * (task->period - task->det)) / task->period;
}
