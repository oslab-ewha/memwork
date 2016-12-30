#include "simrts.h"

void
report_result(void)
{
	printf("simulation time elapsed: %u\n", simtime);
	printf("total power consumed: %.3f\n", power_consumed);
	printf("average power consumed: %.3f\n", power_consumed / simtime);
}
