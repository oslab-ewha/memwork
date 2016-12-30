#include "simrts.h"

extern policy_t	policy_dfdm;
extern policy_t	policy_dvs;
extern policy_t	policy_dmem;

unsigned	max_simtime = 1000;
unsigned	simtime;
policy_t	*policy = NULL;

static BOOL	verbose;

static void
usage(void)
{
	fprintf(stdout,
"Usage: simrts <options> <config path>\n"
" <options>\n"
"      -h: this message\n"
"      -v: verbose mode\n"
"      -t <max simulation time>: (default: 1000)\n"
"      -p <policy>: dfdm(default), dvs, dmem\n"
	);
}

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
	if (strcmp(strpol, "dfdm") == 0)
		policy = &policy_dfdm;
	else if (strcmp(strpol, "dvs") == 0)
		policy = &policy_dvs;
	else if (strcmp(strpol, "dmem") == 0)
		policy = &policy_dmem;
	else {
		FATAL(1, "unknown policy: %s", strpol);
	}
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
			break;
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
	policy_t	*policies[] = { &policy_dfdm, &policy_dvs, &policy_dmem };
	int	i;

	for (i = 0; i < 3; i++) {
		policy = policies[i];
		runsim();

		simtime = 0;
		cleanup_report();
		reinit_tasks();
	}
}

int
main(int argc, char *argv[])
{
	parse_args(argc, argv);

	if (policy == NULL)
		runsim_all();
	else
		runsim();

	return 0;
}
