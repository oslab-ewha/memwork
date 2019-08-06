#include "simrts.h"

static BOOL
hm_chk_assign_task(task_t *task)
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
hm_chk_reassign_task(task_t *task)
{
	mem_type_t	mem_types_try[] = { MEMTYPE_NVRAM, MEMTYPE_DRAM };
	double	power;
	int	i;

	power = calc_task_power_consumed(task);
	revoke_mem(task);

	for (i = 0; i < 2; i++) {
		if (assign_mem(task, mem_types_try[i])) {
			calc_task_det(task);
			if (calc_task_power_consumed(task) <= power) {
				if (is_schedulable(task))
					return TRUE;
			}
			revert_task_det(task);
		}
	}
	return FALSE;
}

policy_t	policy_hm_chk = {
	"hm_chk",
	"checked hybrid memory(hm_chk)",
	FALSE,
	NULL,
	hm_chk_assign_task,
	hm_chk_reassign_task
};
