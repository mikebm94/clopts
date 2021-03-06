#include <stdio.h>
#include "clopts.h"

int
main(int argc, char *argv[])
{
	struct clopts_control ctl;
	const struct option options[] = {
		{ 'a', "one", ARG_NONE },
		{ 'b', "two", ARG_REQUIRED },
		{ 'c', "three", ARG_OPTIONAL },
		{ 'z', NULL, ARG_NONE },
		{0}
	};

	clopts_init(&ctl, "myprog", argc, argv, options, 1);

	while (clopts_parse(&ctl)) {
		if (ctl.error)
			continue;

		if (ctl.paramtype == PARAM_SHORTOPT)
			printf(" -%c", ctl.optcode);
		else if (ctl.paramtype == PARAM_LONGOPT)
			printf(" --%s", options[ctl.optind].name);
		
		if (ctl.paramtype == PARAM_NONOPT
		    || options[ctl.optind].argtype != ARG_NONE)
			printf(" '%s'", ctl.optarg ? ctl.optarg : "");
	}
	
	printf(" --");

	for (; ctl.index < argc; ctl.index++)
		printf(" '%s'", argv[ctl.index]);

	putchar('\n');

	return 0;
}

