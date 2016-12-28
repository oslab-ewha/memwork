#include "simrts.h"

extern policy_t	dfdm_policy;

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

	while ((c = getopt(argc, argv, "h")) != -1) {
		switch (c) {
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

	while ((task = get_edf_task())) {
		schedule_task(task);
		
		requeue_task(task);
	}
}

int
main(int argc, char *argv[])
{
	parse_args(argc, argv);

	runsim();

	return 0;
}
