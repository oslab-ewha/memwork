#include "simrts.h"

static BOOL
dvshm_assign_task(task_t *task)
{
	mem_type_t	mem_types_try[] = { MEMTYPE_DRAM, MEMTYPE_NVRAM };
	int	i;

	task->idx_cpufreq = 1;
	for (i = 0; i < 2; i++) {
		if (assign_mem(task, mem_types_try[i]))
			return TRUE;
	}
	return FALSE;
}

static BOOL
dvshm_reassign_task(task_t *task)
{
	mem_type_t	mem_types_try[] = { MEMTYPE_NVRAM, MEMTYPE_DRAM };
	int	i;

	revoke_mem(task);

	for (i = 0; i < 2; i++) {
		if (assign_mem(task, mem_types_try[i])) {
			int	j;

			for (j = n_cpufreqs; j > 0; j--) {
				task->idx_cpufreq = j;
				calc_task_det(task);
				if (is_schedulable(task))
					return TRUE;
				revert_task_det(task);
			}
			revoke_mem(task);
		}
	}
	return FALSE;
}

policy_t	policy_dvshm = {
	"dvshm",
	"DVS with hybrid memory(dvs-hm)",
	FALSE,
	NULL,
	dvshm_assign_task,
	dvshm_reassign_task
};
