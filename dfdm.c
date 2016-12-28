#include "simrts.h"

static BOOL
dfdm_assign_task(task_t *task)
{
	mem_type_t	mem_types_try[] = { MEMTYPE_DRAM, MEMTYPE_NVRAM };
	int	i;

	task->idx_cpufreq = 1;
	for (i = 0; i < 2; i++) {
		mem_t	*mem = &mems[mem_types_try[i] - 1];
		if (mem->max_capacity > task->memreq + mem->amount) {
			task->mem_type = mem_types_try[i];
			mem->amount += task->memreq;
			return TRUE;
		}
	}
	return FALSE;
}

static void
dfdm_reassign_task(task_t *task)
{
	if (task->mem_type != MEMTYPE_NONE) {
		ASSERT(mems[task->mem_type - 1].amount >= task->memreq);
		mems[task->mem_type - 1].amount -= task->memreq;
		task->mem_type = MEMTYPE_NONE;
	}
}

policy_t	dfdm_policy = {
	dfdm_assign_task,
	dfdm_reassign_task
};
