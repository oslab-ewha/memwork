#include "simrts.h"

extern policy_t	dfdm_policy;

unsigned	max_simtime = 1000;
unsigned	simtime;
double	total_nwcet;
policy_t	*policy = &dfdm_policy;

static void
usage(void)
{
	fprintf(stdout,
"Usage: simrts <options> <config path>\n"
" <options>\n"
"      -h: this message\n"
"      -t <max simulation time>: (default: 1000)\n"
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
parse_args(int argc, char *argv[])
{
	int	c;

	while ((c = getopt(argc, argv, "t:h")) != -1) {
		switch (c) {
		case 't':
			if (sscanf(optarg, "%u", &max_simtime) != 1) {
				usage();
				exit(1);
			}
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

static BOOL
runsim(void)
{
	task_t	*task;

	while (simtime <= max_simtime && (task = get_edf_task())) {
		if (!schedule_task(task))
			return FALSE;
	}
	return TRUE;
}

int
main(int argc, char *argv[])
{
	parse_args(argc, argv);

	if (!runsim()) {
		FATAL(3, "simulation failed");
	}

	report_result();

	return 0;
}
