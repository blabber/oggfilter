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
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include "options.h"
#include "list.h"

static const char *VERSION = "v1.2.0";
static const char *PERIOD_EXPRESSION = "^([[:digit:]]{1,})(:([0-5][[:digit:]]))?$";
enum {
        PERIOD_GROUPS = 4
};

static void     init_options(struct options *opts);
static long     parse_long(char *option);
static double   parse_period(char *period);
static int      prepend_expression(struct options *opts, char *expression, int invert);
static void     print_usage(void);

struct options *
get_options(int argc, char *argv[])
{
        struct option   longopts[] = {
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
                {"no-ignorecase", no_argument, NULL, 'I'}
        };
        struct options *opts;
        int             opt;

        assert(argc >= 0);
        assert(argv != NULL);

        if ((opts = malloc(sizeof(struct options))) == NULL)
                err(EX_SOFTWARE, "could not malloc options struct");

        init_options(opts);

        while ((opt = getopt_long(argc, argv, "hd:l:L:x:X:vb:B:P:I", longopts, NULL)) != -1)
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
                        opts->invert = 1;
                        break;
                case 'I':
                        opts->noignorecase = 1;
                        break;
                case 'h':
                        print_usage();
                        exit(EX_USAGE);
                default:
                        print_usage();
                        exit(EX_OK);
                }

        return (opts);
}

int
free_options(struct options *opts)
{
        struct element *e;

        assert(opts != NULL);

        e = opts->expressionlist;
        while (e != NULL) {
                free(e->payload);
                e = destroy_element(e);
        }

        return (0);
}

static void
print_usage()
{
        printf("This is oggfilter %s\n\n", VERSION);
        printf("oggfilter [-l|--min-length period] [-L|--max-length period]\n");
        printf("          [-b|--min-bitrate bitrate] [-B|--max-bitrate bitrate]\n");
        printf("          [-x|--expression regexp] [-X|--exclude-expression regexp]\n");
        printf("          [-d|--directory directory] [-P|--processes count]\n");
        printf("          [-I|--no-ignorecase] [-v|--invert]\n\n");
        printf("oggfilter {-h|--help}\n");
}

static void
init_options(struct options *opts)
{
        assert(opts != NULL);

        opts->min_length = -1;
        opts->max_length = -1;
        opts->min_bitrate = -1;
        opts->max_bitrate = -1;
        opts->expressionlist = NULL;
        opts->pathprefix = NULL;
        opts->invert = 0;
        opts->processes = 1;
        opts->noignorecase = 0;
}

static double
parse_period(char *option)
{
        int             errcode;
        double          period = 0;
        regex_t regex;
        regmatch_t      groups[PERIOD_GROUPS];

        assert(option != NULL);

        if ((errcode = regcomp(&regex, PERIOD_EXPRESSION, REG_EXTENDED)) != 0) {
                char            errstr[128];
                regerror(errcode, &regex, errstr, sizeof(errstr));
                errx(EX_SOFTWARE, "could not compile regex: %s", PERIOD_EXPRESSION);
        }
        if (regexec(&regex, option, PERIOD_GROUPS, groups, 0)== 0) {
                char           *minutes = NULL;
                char           *seconds;
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
                errx(EX_USAGE, "could not parse time format: '%s'", option);

        regfree(&regex);

        return period;
}

static long
parse_long(char *option)
{
        long            parsed;
        char           *endptr;

        assert(option != NULL);

        parsed = strtol(option, &endptr, 10);
        if (endptr[0] != '\0')
                errx(EX_USAGE, "could not parse numeric value: %s", option);

        return parsed;
}

static int
prepend_expression(struct options *opts, char *expression, int invert)
{
        struct element *e;
        struct expression *x;

        assert(opts != NULL);
        assert(expression !=NULL);
        assert(invert == 0 || invert == 1);

        if ((x = malloc(sizeof(*x))) == NULL)
                err(EX_SOFTWARE, "could not malloc expression");

        x->expression = expression;
        x->invert = invert;

        if ((e = create_element(x)) == NULL)
                err(EX_SOFTWARE, "could not create expression element");
        opts->expressionlist = prepend_element(e, opts->expressionlist);

        return (0);
}
