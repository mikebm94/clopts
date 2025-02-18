#ifndef CLOPTS_H
#define CLOPTS_H 1

#ifdef __cplusplus
extern "C" { //}
#endif

enum {
	CLOPTS_OPT_UNKNOWN = 1,
	CLOPTS_OPT_AMBIGUOUS,
	CLOPTS_ARG_MISSING,
	CLOPTS_ARG_UNEXPECTED
};

typedef enum {
	ARG_NONE,
	ARG_REQUIRED,
	ARG_OPTIONAL
} argument_type;

typedef enum {
	PARAM_NONOPT,
	PARAM_SHORTOPT,
	PARAM_LONGOPT
} parameter_type;

typedef enum {
	PARSE_PERMUTE,
	PARSE_KEEP_ORDER
} parse_mode;

struct option {
	int code;
	const char *name;
	argument_type argtype;
};

struct clopts_control {
	const char *progname;
	int argc;
	char **argv;
	const struct option *options;
	parse_mode mode;
	int print_errors;

	int index;
	char *nextchar;

	int optcode;
	int optind;
	char *optname;
	char *optarg;
	parameter_type paramtype;
	int error;
};

void clopts_init(struct clopts_control *ctl, const char *progname, int argc,
                 char **argv, const struct option *options, parse_mode mode,
                 int print_errors);

int clopts_parse(struct clopts_control *ctl);

#ifdef __cplusplus
}
#endif

#endif /* ! CLOPTS_H */

