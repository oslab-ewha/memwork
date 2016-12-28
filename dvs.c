#include "simrts.h"

static BOOL
dvs_assign_task(task_t *task)
{
	task->idx_cpufreq = 1;
	assign_mem(task, MEMTYPE_DRAM);
	return TRUE;
}

static BOOL
dvs_reassign_task(task_t *task)
{
	int	i;

	for (i = n_cpufreqs; i > 0; i--) {
		task->idx_cpufreq = i;
		if (is_schedulable())
			return TRUE;
	}
	return FALSE;
}

policy_t	policy_dvs = {
	dvs_assign_task,
	dvs_reassign_task
};
