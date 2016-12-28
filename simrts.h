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
	unsigned	no;
	unsigned	wcet;
	unsigned	period;
	unsigned	memreq;

	int		idx_cpufreq;
	mem_type_t	mem_type;

	/* dynamic execution time */
	unsigned	det;	/* in tick */
	/* idle ticks before execution
	 * If another task is inserted ahead, (idle + det) of the inserted task should be subtracted
	 */
	unsigned	idle;	/* in tick */
	/* remaining ticks to a task deadline time including det.
	 */
	unsigned	deadline;	/* in tick */
	struct list_head	list;
} task_t;

typedef struct {
	double		wcet_scale;
	double		power_active, power_idle;	/* per tick */
} cpufreq_t;

typedef struct {
	unsigned	max_capacity;
	double		wcet_scale;
	double		power_active, power_idle;	/* per tick * mem_req */

	unsigned	amount;
	unsigned	n_tasks;
} mem_t;

typedef struct {
	BOOL (*assign_task)(task_t *task);
	BOOL (*reassign_task)(task_t *task);
} policy_t;

extern unsigned simtime;
extern unsigned	n_cpufreqs;
extern cpufreq_t	cpufreqs[];
extern policy_t	*policy;
extern struct list_head	tasks;
extern mem_t	mems[];
extern double	power_consumed;

void errmsg(const char *fmt, ...);

void load_conf(const char *fpath);

BOOL insert_task(unsigned wcet, unsigned period, unsigned memreq);

task_t *get_edf_task(void);
BOOL is_schedulable(void);
BOOL schedule_task(task_t *task);
BOOL requeue_task(task_t *task);
void calc_det(task_t *task);

BOOL assign_mem(task_t *task, mem_type_t mem_type);
void revoke_mem(task_t *task);

void calc_idle_power_consumed(unsigned idle);
void calc_active_power_consumed(task_t *task);

void report_result(void);

#ifdef DEBUG

extern void print_queued_tasks(void);

#endif

#endif
