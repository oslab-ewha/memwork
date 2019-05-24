#include "simrts.h"

static double	sum_utils;
static unsigned	n_utils;

void
add_utilization(void)
{
	sum_utils += get_tasks_ndet();
	n_utils++;
}

void
report_result(void)
{
	double	power_consumed_cpu = power_consumed_cpu_active + power_consumed_cpu_idle;
	double	power_consumed_mem = power_consumed_mem_active + power_consumed_mem_idle;
	double	power_consumed_active = power_consumed_cpu_active + power_consumed_mem_active;
	double	power_consumed_idle = power_consumed_cpu_idle + power_consumed_mem_idle;
	double	power_consumed = power_consumed_cpu + power_consumed_mem;

	printf("policy: %s\n", policy->name);
	printf("simulation time elapsed: %u\n", simtime);
	printf("average power consumed: %.3lf\n", power_consumed / simtime);
	printf("CPU + MEM power consumed: %.3lf + %.3lf\n",
	       power_consumed_cpu / simtime, power_consumed_mem / simtime);
	printf("ACTIVE + IDLE power consumed: %.3lf + %.3lf\n",
	       power_consumed_active / simtime, power_consumed_idle / simtime);
	printf("utilzation: %.2lf%%\n", sum_utils / n_utils * 100);
}

void
cleanup_report(void)
{
	power_consumed_cpu_active = 0;
	power_consumed_cpu_idle = 0;
	power_consumed_mem_active = 0;
	power_consumed_mem_idle = 0;

	sum_utils = 0;
	n_utils = 0;
}
