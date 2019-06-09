#include "simrts.h"

static unsigned	total_capacity;

mem_t	mems[MAX_MEMS];
unsigned	n_mem_types;

static mem_t *
get_mem(mem_type_t mem_type)
{
	unsigned	i;

	for (i = 0; i < n_mem_types; i++) {
		if (mems[i].mem_type == mem_type)
			return mems + i;
	}
	return NULL;
}

void
insert_mem(const char *memstr, unsigned max_capacity, double wcet_scale, double power_active, double power_idle)
{
	mem_t	*mem = &mems[n_mem_types];
	mem_type_t	mem_type;

	if (strcmp(memstr, "nvram") == 0)
		mem_type = MEMTYPE_NVRAM;
	else if (strcmp(memstr, "dram") == 0)
		mem_type = MEMTYPE_DRAM;
	else {
		FATAL(2, "invalid memory type: %s", memstr);
	}

	mem->mem_type = mem_type;
	mem->wcet_scale = wcet_scale;
	mem->max_capacity = max_capacity;
	mem->power_active = power_active;
	mem->power_idle = power_idle;
	n_mem_types++;
}

BOOL
assign_mem(task_t *task, mem_type_t mem_type)
{
	mem_t	*mem = get_mem(mem_type);
	unsigned	max_capacity;

	max_capacity = policy->single_memtype ? total_capacity: mem->max_capacity;

	if (max_capacity > task->memreq + mem->amount) {
		task->mem_type = mem_type;
		mem->amount += task->memreq;
		return TRUE;
	}
	return FALSE;
}

void
revoke_mem(task_t *task)
{
	mem_t	*mem = get_mem(task->mem_type);
	ASSERT(mem->amount >= task->memreq);
	mem->amount -= task->memreq;
	task->mem_type = MEMTYPE_NONE;
}

void
init_mems(void)
{
	int	i;

	total_capacity = 0;
	for (i = 0; i < n_mem_types; i++) {
		total_capacity += mems[i].max_capacity;
	}
}

void
reinit_mems(void)
{
	int	i;

	for (i = 0; i < MAX_MEMS; i++) {
		mems[i].amount = 0;
	}
}
