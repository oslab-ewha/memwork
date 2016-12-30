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
	printf("simulation time elapsed: %u\n", simtime);
	printf("total power consumed: %.3lf\n", power_consumed);
	printf("average power consumed: %.3lf\n", power_consumed / simtime);
	printf("utilzation: %.2lf%%\n", sum_utils / n_utils * 100);
}
