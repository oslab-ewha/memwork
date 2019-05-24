#include "simrts.h"

mem_t	mems[MAX_MEMS];

BOOL
assign_mem(task_t *task, mem_type_t mem_type)
{
	mem_t	*mem = &mems[mem_type - 1];

	if (mem->max_capacity > task->memreq + mem->amount) {
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
