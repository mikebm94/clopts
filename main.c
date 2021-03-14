#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clopts.h"

enum {
	CLOPTS_SUCCESS      = 0,
	CLOPTS_PARSE_ERROR  = 1,
	CLOPTS_BAD_USAGE    = 2,
	CLOPTS_INTERNAL_ERROR = 3
};

struct parse_mode_choice {
	const char *key;
	parse_mode value;
};

static const struct parse_mode_choice parse_mode_choices[] = {
	{ "permute",    PARSE_PERMUTE    },
	{ "keep-order", PARSE_KEEP_ORDER },
	{ NULL,         0                }
};

static const char *const progname_intl = "clopts";

static const char *progname;

static const char *user_progname;

static struct option *user_opts = NULL;

static int user_opts_count = 0;

static int user_opts_size = 0;

static parse_mode user_parse_mode = PARSE_PERMUTE;

static int user_print_errors = 1;


static void
cleanup(void)
{
	free(user_opts);
}

static void
die_bad_usage(const char *fmt, ...)
{
	if (fmt != NULL) {
		va_list args;
		fprintf(stderr, "%s: ", progname);

		va_start(args, fmt);
		vfprintf(stderr, fmt, args);
		va_end(args);
	}

	fprintf(stderr, "Try '%s --help' for more information.\n", progname);
	exit(CLOPTS_BAD_USAGE);
}

static void
die_alloc_failed(void)
{
	fprintf(stderr, "%s: out of memory\n", progname);
	exit(CLOPTS_INTERNAL_ERROR);
}

static void
add_option(const struct option *opt)
{
	static const int realloc_increment = 8;

	if (user_opts_count == user_opts_size) {
		struct option *user_opts_p;
		user_opts_size += realloc_increment;
		user_opts_p = realloc(user_opts, sizeof(*user_opts)
		                                 * (user_opts_size + 1));

		if (user_opts_p == NULL)
			die_alloc_failed();

		user_opts = user_opts_p;
	}

	if (opt != NULL)
		user_opts[user_opts_count++] = *opt;

	user_opts[user_opts_count].code = 0;
	user_opts[user_opts_count].name = NULL;
}

static void
add_shortopts(char *optstr)
{
	struct option opt = { 0, NULL, ARG_NONE };

	while ((opt.code = *optstr++) != '\0') {
		if (optstr[0] == ':' && optstr[1] == ':') {
			opt.argtype = ARG_OPTIONAL;
			optstr += 2;
		} else if (optstr[0] == ':') {
			opt.argtype = ARG_REQUIRED;
			optstr++;
		} else {
			opt.argtype = ARG_NONE;
		}

		add_option(&opt);
	}
}

static void
add_longopts(char *optstr)
{
	static const char *const delimiters = ", \r\n\t\v\f";

	struct option opt = { 0, NULL, ARG_NONE };
	char *name = strtok(optstr, delimiters);

	while (name != NULL) {
		char *argspec = strrchr(name, ':');
		size_t argspec_len;

		if (argspec == NULL) {
			argspec = "";
		} else if (argspec != name && *(argspec - 1) == ':') {
			argspec--;
		}

		if (argspec == name)
			die_bad_usage("missing longopt name in argument "
			              "for option 'l' or '--longopts'\n");

		argspec_len = strlen(argspec);
		if (argspec_len == 1) {
			opt.argtype = ARG_REQUIRED;
		} else if (argspec_len == 2) {
			opt.argtype = ARG_OPTIONAL;
		} else {
			opt.argtype = ARG_NONE;
			argspec_len = 0;
		}

		name[strlen(name) - argspec_len] = '\0';
		opt.name = name;
		add_option(&opt);

		name = strtok(NULL, delimiters);
	}
}

static void
set_parse_mode(const char *mode_name)
{
	size_t mode_name_len = strlen(mode_name);
	const struct parse_mode_choice *choice;
	
	for (choice = parse_mode_choices; choice->key != NULL; choice++) {
		if (strncmp(choice->key, mode_name, mode_name_len) == 0) {
			user_parse_mode = choice->value;
			return;
		}
	}

	fprintf(stderr,
	        "%s: invalid argument '%s' for option 'm' or '--parse-mode'\n"
		"Valid arguments are:\n", progname, mode_name);
	
	for (choice = parse_mode_choices; choice->key != NULL; choice++)
		fprintf(stderr, "  - '%s'\n", choice->key);

	die_bad_usage(NULL);
}

static void
print_normalized(const char *param)
{
	char *buffer = malloc(strlen(param) * 4 + 1);
	char *buffer_ptr = buffer;

	if (buffer == NULL)
		die_alloc_failed();

	for (; *param != '\0'; param++) {
		if (*param == '\'') {
			*buffer_ptr++ = '\'';
			*buffer_ptr++ = '\\';
			*buffer_ptr++ = '\'';
			*buffer_ptr++ = '\'';
		} else {
			*buffer_ptr++ = *param;
		}
	}

	*buffer_ptr = '\0';
	printf(" '%s'", buffer);
}

int
main(int argc, char *argv[])
{
	int exit_code = CLOPTS_SUCCESS;
	struct clopts_control ctl;
	struct option opts[] = {
		{ 'l', "longopts",     ARG_REQUIRED },
		{ 'm', "parse-mode",   ARG_REQUIRED },
		{ 'n', "progname",     ARG_REQUIRED },
		{ 'o', "shortopts",    ARG_REQUIRED },
		{ 'q', "quiet-errors", ARG_NONE     },
		{0}
	};

	atexit(&cleanup);

	progname = (argv[0] && *argv[0]) ? argv[0] : progname_intl;
	user_progname = progname;
	add_option(NULL);

	clopts_init(&ctl, progname, argc, argv, opts, PARSE_KEEP_ORDER, 1);

	while (clopts_parse(&ctl)) {
		if (ctl.error)
			die_bad_usage(NULL);

		if (ctl.paramtype == PARAM_NONOPT)
			die_bad_usage("unexpected operand '%s'\n", ctl.optarg);

		switch (ctl.optcode) {
		case 'l':
			add_longopts(ctl.optarg);
			break;
		case 'm':
			set_parse_mode(ctl.optarg);
			break;
		case 'n':
			user_progname = ctl.optarg;
			break;
		case 'o':
			add_shortopts(ctl.optarg);
			break;
		case 'q':
			user_print_errors = 0;
			break;
		default:
			fprintf(stderr, "%s: internal error", progname);
			return CLOPTS_INTERNAL_ERROR;
		}
	}
	
	ctl.progname = user_progname;
	ctl.options = user_opts;
	ctl.mode = user_parse_mode;
	ctl.print_errors = user_print_errors;

	while (clopts_parse(&ctl)) {
		if (ctl.error) {
			exit_code = CLOPTS_PARSE_ERROR;
			continue;
		}

		if (ctl.paramtype == PARAM_SHORTOPT)
			printf(" -%c", ctl.optcode);
		else if (ctl.paramtype == PARAM_LONGOPT)
			printf(" --%s", user_opts[ctl.optind].name);

		if (ctl.paramtype == PARAM_NONOPT
		    || user_opts[ctl.optind].argtype != ARG_NONE)
			print_normalized(ctl.optarg ? ctl.optarg : "");
	}

	printf(" --");

	while (ctl.index < argc)
		print_normalized(argv[ctl.index++]);

	putchar('\n');

	return exit_code;
}

