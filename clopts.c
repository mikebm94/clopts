#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clopts.h"

void
parse_error(struct clopts_control *ctl, const char *fmt, ...)
{
	if (ctl->print_errors) {
		va_list argp;
		fprintf(stderr, "%s: ", ctl->progname);
		va_start(argp, fmt);
		vfprintf(stderr, fmt, argp);
		va_end(argp);
		fputc('\n', stderr);
	}
}

const struct option *
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
	parse_error(ctl, "unrecognized option '%c'", ctl->optcode);
	return NULL;
}

const struct option *
find_longopt(struct clopts_control *ctl, const char *name, size_t name_len)
{
	const struct option **matches = NULL;
	int match_count = 0;
	int exact_match = 0;

	const struct option *opt;
	for (opt = ctl->options; opt->code != 0 || opt->name != NULL; opt++) {
		if (opt->name == NULL || strncmp(opt->name, name, name_len) != 0)
			continue;

		matches = realloc(matches, sizeof(struct option *) * ++match_count);
		matches[match_count - 1] = opt;

		if (strlen(opt->name) == name_len) {
			exact_match = 1;
			break;
		}
	}

	if (match_count < 1) {
		ctl->error = CLOPTS_UNKNOWN_OPT;
		parse_error(ctl, "unrecognized option '--%.*s'", (int)name_len, name);
		return NULL;
	} else if (exact_match || match_count == 1) {
		opt = matches[match_count - 1];
		ctl->optcode = opt->code;
		ctl->optind = (int)(opt - ctl->options);
		free(matches);
		return opt;
	}

	ctl->error = CLOPTS_AMBIGUOUS_OPT;

	if (ctl->print_errors) {
		int i;
		fprintf(stderr, "%s: option '--%.*s' is amgiguous; possibilities:",
		        ctl->progname, (int)name_len, name);
		for (i = 0; i < match_count; i++)
			fprintf(stderr, " '--%s'", matches[i]->name);
		fputc('\n', stderr);
	}

	free(matches);
	return NULL;
}

void
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
			parse_error(ctl, "option '%c' requires an argument", ctl->optcode);
		}
	}
}

void
parse_longopt(struct clopts_control *ctl)
{
	size_t name_len;
	char *name_begin;
	char *name_end;
	const struct option *opt;

	ctl->paramtype = PARAM_LONGOPT;

	name_begin = ctl->argv[ctl->index++] + 2;
	name_end = strchr(name_begin, '=');
	name_len = name_end ? (size_t)(name_end - name_begin) : strlen(name_begin);

	opt = find_longopt(ctl, name_begin, name_len);
	if (opt == NULL)
		return;

	ctl->optarg = name_end ? name_end + 1 : NULL;

	if (opt->argtype == ARG_NONE && ctl->optarg != NULL) {
		ctl->error = CLOPTS_UNEXPECTED_ARG;
		parse_error(ctl, "option '--%.*s' doesn't accept an argument",
		            (int)name_len, name_begin);
	} else if (opt->argtype == ARG_REQUIRED && ctl->optarg == NULL) {
		if (ctl->index < ctl->argc) {
			ctl->optarg = ctl->argv[ctl->index++];
		} else {
			ctl->error = CLOPTS_MISSING_ARG;
			parse_error(ctl, "option '--%.*s' requires an argument",
			            (int)name_len, name_begin);
		}
	}
}

void
parse_nonopt(struct clopts_control *ctl)
{
	ctl->optarg = ctl->argv[ctl->index++];
	ctl->paramtype = PARAM_NONOPT;
}

void
clopts_init(struct clopts_control *ctl, const char *progname, int argc,
            char **argv, const struct option *options, int print_errors)
{
	ctl->progname = progname ? progname : argv[0];
	ctl->argc = argc;
	ctl->argv = argv;
	ctl->options = options;
	ctl->print_errors = print_errors;

	ctl->index = 1;
	ctl->nextchar = NULL;
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
	} else if (strlen(param) > 2 && strncmp(param, "--", 2) == 0) {
		parse_longopt(ctl);
	} else if (strcmp(param, "--") == 0) {
		ctl->index++;
		return 0;
	} else {
		parse_nonopt(ctl);
	}

	return 1;
}

