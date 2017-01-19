#include "simrts.h"

static BOOL
fast_assign_task(task_t *task)
{
	task->idx_cpufreq = 1;
	return assign_mem(task, MEMTYPE_DRAM);
}

static BOOL
fast_reassign_task(task_t *task)
{
	return TRUE;
}

policy_t	policy_fast = {
	"fast",
	fast_assign_task,
	fast_reassign_task
};
