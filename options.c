/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 *                                                              Tobias Rehbein
 */

#include <assert.h>
#include <err.h>
#include <getopt.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "options.h"
#include "list.h"

static const char *VERSION = "v1.4.0";
static const char *PERIOD_EXPRESSION = "^([[:digit:]]{1,})(:([0-5][[:digit:]]))?$";
enum {
	PERIOD_GROUPS = 4
};

static struct option longopts[] = {
	{"directory", required_argument, NULL, 'd'},
	{"min-length", required_argument, NULL, 'l'},
	{"max-length", required_argument, NULL, 'L'},
	{"expression", required_argument, NULL, 'x'},
	{"exclude-expression", required_argument, NULL, 'X'},
	{"invert", no_argument, NULL, 'v'},
	{"help", no_argument, NULL, 'h'},
	{"min-bitrate", required_argument, NULL, 'b'},
	{"max-bitrate", required_argument, NULL, 'B'},
	{"processes", required_argument, NULL, 'P'},
	{"no-ignorecase", no_argument, NULL, 'I'},
	{"print0", no_argument, NULL, '0'}
};

static void	init_options(struct opt_options *opts);
static long	parse_long(char *option);
static double	parse_period(char *period);
static void	prepend_expression(struct opt_options *opts, char *expression, int invert);
static void	print_usage(void);

struct opt_options *
opt_get_options(int argc, char *argv[])
{
	assert(argc >= 0);
	assert(argv != NULL);

	struct opt_options *opts = malloc(sizeof(struct opt_options));
	if (opts == NULL)
		err(EXIT_FAILURE, "malloc struct opt_options");

	init_options(opts);

	int opt;
	while ((opt = getopt_long(argc, argv, "hd:l:L:x:X:vb:B:P:I0", longopts, NULL)) != -1)
		switch (opt) {
		case 'd':
			opts->pathprefix = optarg;
			break;
		case 'l':
			opts->min_length = parse_period(optarg);
			break;
		case 'L':
			opts->max_length = parse_period(optarg);
			break;
		case 'x':
			prepend_expression(opts, optarg, 0);
			break;
		case 'X':
			prepend_expression(opts, optarg, 1);
			break;
		case 'b':
			opts->min_bitrate = parse_long(optarg) * 1000;
			break;
		case 'B':
			opts->max_bitrate = parse_long(optarg) * 1000;
			break;
		case 'P':
			opts->processes = (int)parse_long(optarg);
			break;
		case 'v':
			opts->invert = true;
			break;
		case 'I':
			opts->noignorecase = true;
			break;
		case '0':
			opts->print0 = true;
			break;
		case 'h':
			print_usage();
			exit(EXIT_FAILURE);
		default:
			print_usage();
			exit(EXIT_SUCCESS);
		}

	return (opts);
}

int
opt_free_options(struct opt_options *opts)
{
	assert(opts != NULL);

	struct element *e = opts->expressionlist;
	while (e != NULL) {
		free(e->payload);
		e = destroy_element(e);
	}

	free(opts);

	return (0);
}

static void
print_usage()
{
	printf("This is oggfilter %s\n\n", VERSION);
	puts("oggfilter [-l|--min-length period] [-L|--max-length period]");
	puts("          [-b|--min-bitrate bitrate] [-B|--max-bitrate bitrate]");
	puts("          [-x|--expression regexp] [-X|--exclude-expression regexp]");
	puts("          [-d|--directory directory] [-P|--processes count]");
	puts("          [-v|--invert] [-I|--no-ignorecase] [-0|--print0]\n");
	puts("oggfilter {-h|--help}");
}

static void
init_options(struct opt_options *opts)
{
	assert(opts != NULL);

	opts->min_length = -1;
	opts->max_length = -1;
	opts->min_bitrate = -1;
	opts->max_bitrate = -1;
	opts->expressionlist = NULL;
	opts->pathprefix = NULL;
	opts->invert = false;
	opts->processes = 1;
	opts->noignorecase = false;
	opts->print0 = false;
}

static double
parse_period(char *option)
{
	assert(option != NULL);

	regex_t regex;

	int e = regcomp(&regex, PERIOD_EXPRESSION, REG_EXTENDED);
	if (e != 0) {
		char errstr[128];
		regerror(e, &regex, errstr, sizeof(errstr));
		errx(EXIT_FAILURE, "regcomp \"%s\": %s", PERIOD_EXPRESSION, errstr);
	}

	double period = 0;
	regmatch_t groups[PERIOD_GROUPS];
	if (regexec(&regex, option, PERIOD_GROUPS, groups, 0) == 0) {
		char *minutes = NULL;
		char *seconds = NULL;
		if (groups[2].rm_so == -1)
			seconds = option;
		else {
			minutes = option;
			option[groups[2].rm_so] = '\0';
			seconds = &option[groups[3].rm_so];
		}

		if (minutes != NULL)
			period = parse_long(minutes) * 60;
		period += parse_long(seconds);
	} else
		errx(EXIT_FAILURE, "invalid period \"%s\"", option);

	regfree(&regex);

	return (period);
}

static long
parse_long(char *option)
{
	assert(option != NULL);

	char *endptr;
	long parsed = strtol(option, &endptr, 10);
	if (endptr[0] != '\0')
		errx(EXIT_FAILURE, "illegal number \"%s\"", option);

	return (parsed);
}

static void
prepend_expression(struct opt_options *opts, char *expression, int invert)
{
	assert(opts != NULL);
	assert(expression != NULL);
	assert(invert == 0 || invert == 1);

	struct opt_expression *x = malloc(sizeof(*x));
	if (x == NULL)
		err(EXIT_FAILURE, "malloc opt_expression");

	x->expression = expression;
	x->invert = invert;

	struct element *e = create_element(x);
	if (e == NULL)
		err(EXIT_FAILURE, "create_element");

	opts->expressionlist = prepend_element(e, opts->expressionlist);
}
