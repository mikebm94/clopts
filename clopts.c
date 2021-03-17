#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clopts.h"

struct option_node {
	const struct option *opt;
	struct option_node *next;
};

void
clopts_init(struct clopts_control *ctl, const char *progname, int argc,
            char **argv, const struct option *options, parse_mode mode,
            int print_errors)
{
	ctl->progname = progname ? progname : argv[0];
	ctl->argc = argc;
	ctl->argv = argv;
	ctl->options = options;
	ctl->mode = mode;
	ctl->print_errors = print_errors;

	ctl->index = 1;
	ctl->nextchar = NULL;
}

static void
parse_error(struct clopts_control *ctl, const char *fmt, ...)
{
	if (ctl->print_errors) {
		va_list args;
		va_start(args, fmt);

		fprintf(stderr, "%s%s", ctl->progname,
		        *ctl->progname ? ": " : "");
		vfprintf(stderr, fmt, args);

		va_end(args);
	}
}

static const struct option *
find_shortopt(struct clopts_control *ctl)
{
	const struct option *opt;
	for (opt = ctl->options; opt->code != 0 || opt->name != NULL; opt++) {
		if (opt->code == ctl->optcode) {
			ctl->optind = (int)(opt - ctl->options);
			return opt;
		}
	}

	ctl->error = CLOPTS_UNKNOWN_OPT;
	parse_error(ctl, "unrecognized option '%c'\n", ctl->optcode);
	return NULL;
}

static void
parse_shortopt(struct clopts_control *ctl)
{
	const struct option *opt;

	ctl->paramtype = PARAM_SHORTOPT;
	ctl->optcode = *ctl->nextchar++;

	if (*ctl->nextchar == '\0') {
		ctl->nextchar = NULL;
		ctl->index++;
	}

	opt = find_shortopt(ctl);
	if (opt == NULL || opt->argtype == ARG_NONE)
		return;

	if (ctl->nextchar != NULL) {
		ctl->optarg = ctl->nextchar;
		ctl->nextchar = NULL;
		ctl->index++;
	} else if (opt->argtype == ARG_REQUIRED) {
		if (ctl->index < ctl->argc) {
			ctl->optarg = ctl->argv[ctl->index++];
		} else {
			ctl->error = CLOPTS_MISSING_ARG;
			parse_error(ctl, "option '%c' requires an argument\n",
			            ctl->optcode);
		}
	}
}

static const struct option *
find_longopt(struct clopts_control *ctl, const char *name)
{
	int is_ambig = 0;
	int track_all_matches = 1;
	size_t name_len = strlen(name);
	struct option_node *matches = NULL;
	const struct option *last_match = NULL;
	const struct option *opt;

	for (opt = ctl->options; opt->name || opt->code; opt++) {
		if (!opt->name || strncmp(opt->name, name, name_len))
			continue;

		if (strlen(opt->name) == name_len) {
			is_ambig = 0;
			last_match = opt;
			break;
		} else if (last_match != NULL) {
			is_ambig = 1;
		}

		last_match = opt;

		if (track_all_matches) {
			struct option_node *match = malloc(sizeof(*match));

			if (match == NULL) {
				track_all_matches = 0;
			} else {
				match->opt = opt;
				match->next = matches;
				matches = match;
			}
		}
	}

	if (last_match == NULL) {
		ctl->error = CLOPTS_UNKNOWN_OPT;
		parse_error(ctl, "unknown option '--%s'\n", name);
		return NULL;
	} else if (is_ambig) {
		ctl->error = CLOPTS_AMBIGUOUS_OPT;
		parse_error(ctl, "option '--%s' is ambiguous%s", name,
			    track_all_matches ? "; possibilities:" : "");

		while (matches != NULL) {
			struct option_node *next_match = matches->next;
			if (track_all_matches)
				fprintf(stderr, " '--%s'", matches->opt->name);
			free(matches);
			matches = next_match;
		}

		fputc('\n', stderr);
		return NULL;
	}

	while (matches != NULL) {
		struct option_node *next_match = matches->next;
		free(matches);
		matches = next_match;
	}

	ctl->optcode = last_match->code;
	ctl->optind = (int)(last_match - ctl->options);
	return last_match;
}

static void
parse_longopt(struct clopts_control *ctl)
{
	char *name, *name_end;
	const struct option *opt;

	ctl->paramtype = PARAM_LONGOPT;
	name = ctl->argv[ctl->index++] + 2;
	name_end = strchr(name, '=');

	if (name_end != NULL) {
		*name_end = '\0';
		ctl->optarg = name_end + 1;
	}

	opt = find_longopt(ctl, name);
	if (opt == NULL)
		return;

	if (opt->argtype == ARG_NONE && ctl->optarg != NULL) {
		ctl->error = CLOPTS_UNEXPECTED_ARG;
		parse_error(ctl, "option '--%s' doesn't accept an argument\n",
		            name);
	} else if (opt->argtype == ARG_REQUIRED && ctl->optarg == NULL) {
		if (ctl->index < ctl->argc) {
			ctl->optarg = ctl->argv[ctl->index++];
		} else {
			ctl->error = CLOPTS_MISSING_ARG;
			parse_error(ctl, "option '--%s' requires "
			            "an argument\n", name);
		}
	}
}

static int
parse_nonopt(struct clopts_control *ctl)
{
	if (ctl->mode == PARSE_KEEP_ORDER) {
		ctl->paramtype = PARAM_NONOPT;
		ctl->optarg = ctl->argv[ctl->index++];
		return 1;
	} else {
		int nonopt_index = ctl->index++;
		char *nonopt = ctl->argv[nonopt_index];
		int r = clopts_parse(ctl);

		int i;
		for (i = nonopt_index; i < ctl->index - 1; i++)
			ctl->argv[i] = ctl->argv[i + 1];
		ctl->argv[--ctl->index] = nonopt;

		return r;
	}
}

int
clopts_parse(struct clopts_control *ctl)
{
	char *param = ctl->argv[ctl->index];

	ctl->optcode = 0;
	ctl->optind = -1;
	ctl->optarg = NULL;
	ctl->error = 0;

	if (ctl->index >= ctl->argc)
		return 0;

	if (ctl->nextchar == NULL) {
		if (strlen(param) > 1 && param[0] == '-' && param[1] != '-')
			ctl->nextchar = param + 1;
	}

	if (ctl->nextchar != NULL) {
		parse_shortopt(ctl);
	} else if (strncmp(param, "--", 2) == 0) {
		if (strlen(param) > 2) {
			parse_longopt(ctl);
		} else {
			ctl->index++;
			return 0;
		}
	} else {
		return parse_nonopt(ctl);
	}

	return 1;
}

