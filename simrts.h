#ifndef _SIMRTS_H_
#define _SIMRTS_H_

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <math.h>

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
	double		mem_active_ratio;

	int		idx_cpufreq;
	mem_type_t	mem_type;

	/* dynamic execution time */
	unsigned	det;	/* in tick */

	/* remaining execution time */
	unsigned	det_remain;	/* in tick */

	/* for reverting */
	unsigned	det_old, det_remain_old;

	/* ticks gap from a simtime if a task is a head task */
	unsigned	gap_head;	/* in tick */
	/* ticks from a start tick of a following task if any */
	unsigned	gap;	/* in tick */
	/* deadline ticks including det
	 */
	unsigned	deadline;	/* in tick */
	struct list_head	list_sched;
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
	const char	*name;
	BOOL (*assign_task)(task_t *task);
	BOOL (*reassign_task)(task_t *task);
} policy_t;

extern unsigned simtime;
extern unsigned	n_cpufreqs;
extern cpufreq_t	cpufreqs[];
extern policy_t	*policy;
extern struct list_head	tasks;
extern mem_t	mems[];
extern double	power_consumed_cpu_active;
extern double	power_consumed_mem_active;
extern double	power_consumed_cpu_idle;
extern double	power_consumed_mem_idle;

void errmsg(const char *fmt, ...);

void load_conf(const char *fpath);

void insert_task(unsigned wcet, unsigned period, unsigned memreq, double mem_active_ratio);
BOOL setup_tasks(void);

void calc_task_det(task_t *task);
void revert_task_det(task_t *task);

task_t *pop_head_task(void);
double get_tasks_ndet(void);
BOOL is_schedulable(task_t *task);
BOOL schedule_task(task_t *task);
void requeue_task(task_t *task, unsigned ticks);
void check_queued_tasks(void);
void reinit_tasks(void);

BOOL assign_mem(task_t *task, mem_type_t mem_type);
void revoke_mem(task_t *task);

void calc_idle_power_consumed_task(task_t *task, unsigned idle);
void calc_idle_power_consumed_task_mem(task_t *task, unsigned idle);
void calc_idle_power_consumed_task_cpu(task_t *task, unsigned idle);
void calc_idle_power_consumed(unsigned idle);
void calc_active_power_consumed(task_t *task, unsigned ret);

void add_utilization(void);
void report_result(void);
void cleanup_report(void);

extern const char *desc_task(task_t *task);
extern void show_queued_tasks(void);

#endif
