#include "simrts.h"

#define MAX_TASKS	1000

typedef struct {
	unsigned	mem_idx, cpufreq_idx;
} task_info_t;

static task_info_t task_infos[MAX_TASKS];

static BOOL
fixed_init(void)
{
	FILE	*fp;
	char	buf[1024];
	int	n = 0;
	
	fp = fopen("task.txt", "r");
	if (fp == NULL)
		return FALSE;
	while (fgets(buf, 1024, fp)) {
		if (buf[0] == '#')
			continue;
		if (sscanf(buf, "%u %u", &task_infos[n].mem_idx, &task_infos[n].cpufreq_idx) != 2) {
			fclose(fp);
			return FALSE;
		}
		n++;
	}
	fclose(fp);
	return TRUE;
}

static BOOL
fixed_assign_task(task_t *task)
{
	task_info_t	*info = task_infos + task->no;
	
	task->idx_cpufreq = info->cpufreq_idx + 1;
	return assign_mem(task, info->mem_idx + 1);
}

static BOOL
fixed_reassign_task(task_t *task)
{
	return TRUE;
}

policy_t	policy_fixed = {
	"fixed",
	fixed_init,
	fixed_assign_task,
	fixed_reassign_task
};
