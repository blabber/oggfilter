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

#define PERIOD_EXPRESSION       "^([[:digit:]]{1,})(:([0-5][[:digit:]]))?$"
#define PERIOD_GROUPS           4

static void     init_options(struct options *opts);
static long     parse_long(char *option);
static double   parse_period(char *period);
static void     print_usage();

void
parse_options(struct options *opts, int argc, char *argv[])
{
        struct option   longopts[] = {
                {"directory", required_argument, NULL, 'd'},
                {"min-length", required_argument, NULL, 'l'},
                {"max-length", required_argument, NULL, 'L'},
                {"expression", required_argument, NULL, 'x'},
                {"invert", no_argument, NULL, 'v'},
                {"help", no_argument, NULL, 'h'},
                {"min-bitrate", required_argument, NULL, 'b'},
                {"max-bitrate", required_argument, NULL, 'B'}
        };
        int             opt;

        assert(opts != NULL);
        assert(argc >= 0);
        assert(argv != NULL);

        init_options(opts);

        while ((opt = getopt_long(argc, argv, "hd:l:L:x:vb:B:", longopts, NULL)) != -1)
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
                        opts->expression = optarg;
                        break;
                case 'b':
                        opts->min_bitrate = parse_long(optarg) * 1000;
                        break;
                case 'B':
                        opts->max_bitrate = parse_long(optarg) * 1000;
                        break;
                case 'v':
                        opts->invert = 1;
                        break;
                case 'h':
                        print_usage();
                        exit(EX_USAGE);
                default:
                        print_usage();
                        exit(EX_OK);
                }
}

static void
print_usage()
{
        printf("oggfilter [-l|--min-length period] [-L|--max-length period]\n");
        printf("          [-b|--min-bitrate bitrate] [-B|--max-bitrate]\n");
        printf("          [-x|--expression expression] [-d|--directory directory]\n");
        printf("          [-v|--invert]\n\n");
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
        opts->expression = NULL;
        opts->pathprefix = NULL;
        opts->invert = 0;
}

static double
parse_period(char *option)
{
        int             errcode;
        double          period = 0;
        regex_t         regex;
        regmatch_t      groups[PERIOD_GROUPS];

        assert(option != NULL);

        if ((errcode = regcomp(&regex, PERIOD_EXPRESSION, REG_EXTENDED)) != 0) {
                char            errstr[128];
                regerror(errcode, &regex, errstr, sizeof(errstr));
                errx(EX_SOFTWARE, "could not compile regex: %s", PERIOD_EXPRESSION);
        }
        if (regexec(&regex, option, PERIOD_GROUPS, groups, 0) == 0) {
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
