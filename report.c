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
report_header(void)
{
	if (verbose)
		return;
	printf("#name power util cpu mem active idle\n");
}

void
report_result(void)
{
	double	power_consumed_cpu = power_consumed_cpu_active + power_consumed_cpu_idle;
	double	power_consumed_mem = power_consumed_mem_active + power_consumed_mem_idle;
	double	power_consumed_active = power_consumed_cpu_active + power_consumed_mem_active;
	double	power_consumed_idle = power_consumed_cpu_idle + power_consumed_mem_idle;
	double	power_consumed = power_consumed_cpu + power_consumed_mem;

	double	power_consumed_avg = power_consumed / simtime;
	double	power_consumed_cpu_avg = power_consumed_cpu / simtime;
	double	power_consumed_mem_avg = power_consumed_mem / simtime;
	double	power_consumed_active_avg = power_consumed_active / simtime;
	double	power_consumed_idle_avg = power_consumed_idle / simtime;
	double	utilization = sum_utils / n_utils * 100;

	if (verbose) {
		printf("policy: %s\n", policy->desc);
		printf("simulation time elapsed: %u\n", simtime);
		printf("average power consumed: %.3lf\n", power_consumed_avg);
		printf("CPU + MEM power consumed: %.3lf + %.3lf\n",
		       power_consumed_cpu_avg, power_consumed_mem_avg);
		printf("ACTIVE + IDLE power consumed: %.3lf + %.3lf\n",
		       power_consumed_active_avg, power_consumed_idle_avg);
		printf("utilzation: %.2lf%%\n", utilization);
	}
	else {
		printf("%10s %.3lf %.3lf %.3lf %.3lf %.3lf %.3lf\n", policy->name, 
		       power_consumed_avg, utilization, power_consumed_cpu_avg, power_consumed_mem_avg,
		       power_consumed_active_avg, power_consumed_idle_avg);
	}
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
