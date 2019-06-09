#include "simrts.h"

static void
usage(void)
{
	fprintf(stdout,
"Usage: simrts <options> <config path>\n"
" <options>\n"
"      -h: this message\n"
"      -v: verbose mode\n"
"      -t <max simulation time>: (default: 1000)\n"
"      -p <policy>: dvshm, dvsdram, hm, dram, fixed\n"
	);
}

extern policy_t	policy_dvshm;
extern policy_t	policy_dvsdram;
extern policy_t	policy_hm;
extern policy_t	policy_dram;
extern policy_t	policy_fixed;
extern policy_t	policy_dvshm_greedy;

unsigned	max_simtime = 1000;
unsigned	simtime;
policy_t	*policy = NULL;

static BOOL	verbose;

static policy_t	*all_policies[] = {
	&policy_dvshm, &policy_dvsdram, &policy_hm, &policy_dram, &policy_fixed,
	&policy_dvshm_greedy, NULL
};

void
errmsg(const char *fmt, ...)
{
	va_list	ap;
	char	*errmsg;

	va_start(ap, fmt);
	vasprintf(&errmsg, fmt, ap);
	va_end(ap);

	fprintf(stderr, "ERROR: %s\n", errmsg);
	free(errmsg);
}

static void
setup_policy(const char *strpol)
{
	int	i;

	for (i = 0; all_policies[i]; i++) {
		if (strcmp(strpol, all_policies[i]->name) == 0) {
			policy = all_policies[i];
			return;
		}
	}
	FATAL(1, "unknown policy: %s", strpol);
}

static void
parse_args(int argc, char *argv[])
{
	int	c;

	while ((c = getopt(argc, argv, "t:p:h")) != -1) {
		switch (c) {
		case 't':
			if (sscanf(optarg, "%u", &max_simtime) != 1) {
				usage();
				exit(1);
			}
			break;
		case 'p':
			setup_policy(optarg);
			break;
		case 'h':
			usage();
			exit(0);
		default:
			errmsg("invalid option");
			usage();
			exit(1);
		}
	}

	if (argc - optind < 1) {
		usage();
		exit(1);
	}

	load_conf(argv[optind]);
}

static void
runsim(void)
{
	task_t	*task;

	if (policy->init != NULL) {
		if (!policy->init())
			FATAL(3, "failed to initialize policy");
	}

	if (!setup_tasks()) {
		FATAL(3, "failed to setup tasks");
	}

	while (simtime <= max_simtime && (task = pop_head_task())) {
		if (!schedule_task(task)) {
			FATAL(3, "simulation failed");
		}
		check_queued_tasks();
		add_utilization();
		if (verbose)
			show_queued_tasks();
	}

	report_result();
}

static void
runsim_all(void)
{
	int	i;

	init_mems();

	for (i = 0; all_policies[i]; i++) {
		policy = all_policies[i];
		runsim();

		simtime = 0;
		cleanup_report();
		reinit_tasks();
		reinit_mems();
	}
}

int
main(int argc, char *argv[])
{
	parse_args(argc, argv);

	if (policy == NULL)
		runsim_all();
	else {
		init_mems();
		runsim();
	}

	return 0;
}
