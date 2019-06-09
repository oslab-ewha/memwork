#include "simrts.h"

static BOOL
dram_assign_task(task_t *task)
{
	task->idx_cpufreq = 1;
	return assign_mem(task, MEMTYPE_DRAM);
}

static BOOL
dram_reassign_task(task_t *task)
{
	return TRUE;
}

policy_t	policy_dram = {
	"dram",
	"No DVS with dram(dram)",
	TRUE,
	NULL,
	dram_assign_task,
	dram_reassign_task
};
