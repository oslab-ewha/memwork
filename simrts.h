#ifndef _SIMRTS_H_
#define _SIMRTS_H_

#include <stdio.h>

#include "ecm_list.h"

typedef struct {
	unsigned	wcet;
	unsigned	period;
	unsigned	memreq;

	struct list_head	list;
} task_t;

#endif
