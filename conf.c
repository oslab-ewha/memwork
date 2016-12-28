#include "simrts.h"

typedef enum {
	SECT_UNKNOWN,
	SECT_CPUFREQ,
	SECT_MEM,
	SECT_TASK,
} section_t;

cpufreq_t	cpufreqs[MAX_CPU_FREQS];
mem_t		mems[MAX_MEMS];
unsigned	n_cpufreqs;

static char *
trim(char *str)
{
	char	*p;

	if (str == NULL)
		return NULL;

	for (p = str; *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'; p++);
	str = p;
	if (*p != '\0') {
		for (p = p + 1; *p != '\0'; p++);
		for (p = p - 1; (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') && p != str; p--);
		p[1] = '\0';
	}

	return str;
}

static section_t
check_section(const char *line)
{
	if (*line != '*')
		return FALSE;
	if (strncmp(line + 1, "cpufreq", 7) == 0)
		return SECT_CPUFREQ;
	if (strncmp(line + 1, "mem", 3) == 0)
		return SECT_MEM;
	if (strncmp(line + 1, "task", 4) == 0)
		return SECT_TASK;
	return SECT_UNKNOWN;
}

static BOOL
insert_cpufreq(double wcet_scale, double energy)
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
	cpufreqs[n_cpufreqs - 1].energy = energy;
	return TRUE;
}

static void
insert_mem(mem_type_t mem_type, unsigned max_capacity, double wcet_scale, double energy)
{
	mem_t	*mem = &mems[mem_type - 1];

	mem->max_capacity = max_capacity;
	mem->energy = energy;
}

static void
parse_cpufreq(FILE *fp)
{
	char	buf[1024];

	while (fgets(buf, 1024, fp)) {
		double	wcet_scale, energy;

		if (buf[0] == '#')
			continue;
		if (buf[0] == '\n' || buf[0] == '*') {
			fseek(fp, -1 * strlen(buf), SEEK_CUR);
			return;
		}
		if (sscanf(buf, "%lf %lf", &wcet_scale, &energy) != 2) {
			FATAL(2, "cannot load configuration: invalid CPU frequency format: %s", trim(buf));
		}

		if (wcet_scale < 0 || wcet_scale > 1) {
			FATAL(2, "invalid cpu frequency wcet scale: %s", trim(buf));
		}
		if (energy < 0) {
			FATAL(2, "invalid cpu frequency energy: %s", trim(buf));
		}
		if (!insert_cpufreq(wcet_scale, energy)) {
			FATAL(2, "cannot insert CPU frequency: %s", trim(buf));
		}
	}
}

static void
parse_mem(FILE *fp)
{
	char	buf[1024];

	while (fgets(buf, 1024, fp)) {
		unsigned	max_capacity;
		double		wcet_scale, energy;
		mem_type_t	mem_type;
		char		type[1024];

		if (buf[0] == '#')
			continue;
		if (buf[0] == '\n' || buf[0] == '*') {
			fseek(fp, -1 * strlen(buf), SEEK_CUR);
			return;
		}
		if (sscanf(buf, "%s %u %lf %lf", type, &max_capacity, &wcet_scale, &energy) != 4) {
			FATAL(2, "cannot load configuration: invalid memory spec: %s", trim(buf));
		}

		if (max_capacity == 0) {
			FATAL(2, "invalid max memory capacity: %s", trim(buf));
		}
		if (wcet_scale < 0 || wcet_scale > 1) {
			FATAL(2, "invalid memory wcet scale: %s", trim(buf));
		}
		if (energy < 0) {
			FATAL(2, "invalid memory energy: %s", trim(buf));
		}

		if (strcmp(type, "nvram") == 0)
			mem_type = MEMTYPE_NVRAM;
		else if (strcmp(type, "dram") == 0)
			mem_type = MEMTYPE_DRAM;
		else {
			FATAL(2, "invalid memory type: %s", trim(buf));
		}
		insert_mem(mem_type, max_capacity, wcet_scale, energy);
	}
}

static void
parse_task(FILE *fp)
{
	char	buf[1024];

	while (fgets(buf, 1024, fp)) {
		unsigned	wcet, period, memreq;

		if (buf[0] == '#')
			continue;
		if (buf[0] == '\n' || buf[0] == '*') {
			fseek(fp, -1 * strlen(buf), SEEK_CUR);
			return;
		}
		if (sscanf(buf, "%u %u %u", &wcet, &period, &memreq) != 3) {
			FATAL(2, "cannot load configuration: invalid task format: %s", trim(buf));
		}

		if (wcet >= period) {
			FATAL(2, "wcet is larger or equal than period: %s", trim(buf));
		}
		if (!insert_task(wcet, period, memreq)) {
			FATAL(2, "cannot insert task: %s", trim(buf));
		}
	}
}

static void
parse_conf(FILE *fp)
{
	char	buf[1024];

	while (fgets(buf, 1024, fp)) {
		if (buf[0] == '\n' || buf[0] == '#')
			continue;
		switch (check_section(buf)) {
		case SECT_CPUFREQ:
			parse_cpufreq(fp);
			break;
		case SECT_MEM:
			parse_mem(fp);
			break;
		case SECT_TASK:
			if (n_cpufreqs == 0) {
				FATAL(2, "cpu frequency section should be defined ahead of task section");
			}
			parse_task(fp);
			break;
		default:
			errmsg("unknown section: %s", trim(buf));
			FATAL(2, "cannot load configuration");
		}
	}
}

void
load_conf(const char *fpath)
{
	FILE	*fp;

	fp = fopen(fpath, "r");
	if (fp == NULL) {
		FATAL(1, "configuration not found: %s", fpath);
	}

	parse_conf(fp);

	fclose(fp);
}
