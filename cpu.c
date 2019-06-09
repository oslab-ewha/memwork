#include "simrts.h"

cpufreq_t	cpufreqs[MAX_CPU_FREQS];
unsigned	n_cpufreqs;

BOOL
insert_cpufreq(double wcet_scale, double power_active, double power_idle)
{
	if (n_cpufreqs >= MAX_CPU_FREQS) {
		errmsg("too many cpu frequencies");
		return FALSE;
	}
	if (n_cpufreqs > 0 && cpufreqs[n_cpufreqs - 1].wcet_scale < wcet_scale) {
		errmsg("cpu frequency should be defined in decreasing order");
		return FALSE;
	}
	n_cpufreqs++;
	cpufreqs[n_cpufreqs - 1].wcet_scale = wcet_scale;
	cpufreqs[n_cpufreqs - 1].power_active = power_active;
	cpufreqs[n_cpufreqs - 1].power_idle = power_idle;
	return TRUE;
}
