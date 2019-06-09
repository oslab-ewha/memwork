#include "simrts.h"

static unsigned	total_capacity;

mem_t	mems[MAX_MEMS];

BOOL
assign_mem(task_t *task, mem_type_t mem_type)
{
	mem_t	*mem = &mems[mem_type - 1];
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
	ASSERT(mems[task->mem_type - 1].amount >= task->memreq);
	mems[task->mem_type - 1].amount -= task->memreq;
	task->mem_type = MEMTYPE_NONE;
}

void
init_mems(void)
{
	int	i;

	total_capacity = 0;
	for (i = 0; i < MAX_MEMS; i++) {
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
