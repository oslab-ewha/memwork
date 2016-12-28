#ifndef _SIMRTS_H_
#define _SIMRTS_H_

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "ecm_list.h"

#define TRUE	1
#define FALSE	0

#define MAX_CPU_FREQS	10
#define MAX_MEMS	2

#define ASSERT(cond)			do { assert(cond); } while (0)
#define FATAL(exitcode, fmt, ...)	do { errmsg(fmt, ## __VA_ARGS__); exit(exitcode); } while (0)

typedef int	BOOL;

typedef enum {
	MEMTYPE_NONE = 0,
	MEMTYPE_NVRAM = 1,
	MEMTYPE_DRAM = 2,
} mem_type_t;

typedef struct {
	unsigned	wcet;
	unsigned	period;
	unsigned	memreq;

	int		idx_cpufreq;
	mem_type_t	mem_type;

	/* dynamic execution time */
	unsigned	det;	/* in tick */
	unsigned	tick_executed;
	unsigned	running;
	struct list_head	list;
} task_t;

typedef struct {
	double		wcet_scale;
	double		energy;	/* per tick */
} cpufreq_t;

typedef struct {
	unsigned	max_capacity;
	double		wcet_scale;
	double		energy;	/* per tick * mem_req */

	unsigned	amount;
	unsigned	n_tasks;
} mem_t;

typedef struct {
	BOOL (*assign_task)(task_t *task);
	void (*reassign_task)(task_t *task);
} policy_t;

extern unsigned	n_cpufreqs;
extern cpufreq_t	cpufreqs[];
extern policy_t	*policy;
extern mem_t	mems[];

void errmsg(const char *fmt, ...);

void load_conf(const char *fpath);

BOOL insert_task(unsigned wcet, unsigned period, unsigned memreq);

task_t *get_edf_task(void);
void schedule_task(task_t *task);
void requeue_task(task_t *task);
void calc_det(task_t *task);

#endif
