/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 *                                                              Tobias Rehbein
 */

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <err.h>
#include <regex.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#define MAXLINE         1024
#define BUFFLEN         256
#define R_PERIOD        "^([[:digit:]]{1,})(:([0-5][[:digit:]]))?$"
#define TRY_MALLOC(v,s) if ((v = malloc(s)) == NULL) err(1 ,"%s" ,#v)


typedef struct {
        int             min_length_flag;
        double          min_length;
        int             max_length_flag;
        double          max_length;
        int             min_bitrate_flag;
        long            min_bitrate;
        int             max_bitrate_flag;
        long            max_bitrate;
        int             expression_flag;
        regex_t         expression;
} filter_t;

/* prototypes */
int             main(int argc, char **argv);
int             check_bitrate(OggVorbis_File ovf, filter_t *filter, char *filename);
int             check_comments(OggVorbis_File ovf, filter_t *filter, char *filename);
int             check_file(char *filename, filter_t *filter);
int             check_time(OggVorbis_File ovf, filter_t *filter);
int             compile_regex(regex_t * preg, char *expression, int expr_flags);
double          parse_period(char *option);

/* options descriptor */
static struct option longopts[] = {
        {"directory", required_argument, NULL, 'd'},
        {"min-length", required_argument, NULL, 'l'},
        {"max-length", required_argument, NULL, 'L'},
        {"expression", required_argument, NULL, 'x'},
        {"extended", no_argument, NULL, 'E'},
        {"invert", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'h'},
        {"min-bitrate", required_argument, NULL, 'b'},
        {"max-bitrate", required_argument, NULL, 'B'}
};

int
main(int argc, char **argv)
{
        char           *filename_buffer = NULL;
        char           *filename = NULL;
        char           *expression = NULL;
        char           *in = NULL;
        char           *newline;
        char            option;
        int             invert = 0;
        int             expr_flags = REG_ICASE;

        filter_t        filter = {
                0,              /* min_length_flag */
                0.0,            /* min_length */
                0,              /* max_length_flag */
                0.0,            /* max_length */
                0,              /* min_bitrate_flag */
                0L,             /* min_bitrate */
                0,              /* max_bitrate_flag */
                0L,             /* max_bitrate */
                0               /* expression_flag */
                /* expression */
        };

        /* parse command line switches */
        while ((option = getopt_long(argc, argv, "hd:l:L:x:Evb:B:", longopts, NULL)) != -1)
                switch (option) {
                case 'd':
                        if (optarg[strlen(optarg) - 1] == '/') {
                                TRY_MALLOC(filename_buffer, strlen(optarg) * sizeof(char) + MAXLINE + 1);
                                strncpy(filename_buffer, optarg, strlen(optarg) + 1);
                                in = &filename_buffer[strlen(optarg)];
                        } else {
                                TRY_MALLOC(filename_buffer, strlen(optarg) * sizeof(char) + 1 + MAXLINE + 1);
                                strncpy(filename_buffer, optarg, strlen(optarg));
                                strncpy(&filename_buffer[strlen(optarg)], "/", 2);
                                in = &filename_buffer[strlen(optarg) + 1];
                        }
                        break;
                case 'l':
                        filter.min_length_flag = 1;
                        filter.min_length = parse_period(optarg);
                        break;
                case 'L':
                        filter.max_length_flag = 1;
                        filter.max_length = parse_period(optarg);
                        break;
                case 'x':
                        expression = optarg;
                        break;
                case 'E':
                        expr_flags |= REG_EXTENDED;
                        break;
                case 'b':
                        filter.min_bitrate_flag = 1;
                        filter.min_bitrate = strtol(optarg, (char **)NULL, 10) * 1000;
                        break;
                case 'B':
                        filter.max_bitrate_flag = 1;
                        filter.max_bitrate = strtol(optarg, (char **)NULL, 10) * 1000;
                        break;
                case 'v':
                        invert = 1;
                        break;
                case 'h':
                default:
                        printf("oggfilter [-l|--min-length period] [-L|--max-length period]\n");
                        printf("          [-b|--min-bitrate bitrate] [-B|--max-bitrate]\n");
                        printf("          [-x|--expression expression] [-d|--directory directory]\n");
                        printf("          [-E|--extended] [-v|--invert]\n\n");
                        printf("oggfilter {-h|--help}\n");
                        return 0;
                }
        filter.expression_flag = compile_regex(&filter.expression, expression, expr_flags);

        if (filename_buffer == NULL) {
                TRY_MALLOC(filename_buffer, MAXLINE + 1);
                in = filename_buffer;
        }
        /* main loop */
        while (fgets(in, MAXLINE, stdin) != NULL) {
                if ((newline = strchr(in, '\n')) != NULL)
                        newline[0] = '\0';

                if (in[0] == '/')
                        filename = in;
                else
                        filename = filename_buffer;

                if (check_file(filename, &filter) ^ invert)
                        printf("%s\n", filename);
        }

        /* free all resources */
        if (filename != NULL)
                free(filename);
        if (filter.expression_flag)
                regfree(&filter.expression);

        return 0;
}

int
check_file(char *filename, filter_t *filter)
{
        OggVorbis_File  ovf;
        int             match;

        if (ov_fopen(filename, &ovf) != 0) {
                warnx("could not open file: %s", filename);
                return 0;
        }
        match = check_time(ovf, filter);
        match = match && check_bitrate(ovf, filter, filename);
        match = match && check_comments(ovf, filter, filename);

        ov_clear(&ovf);

        return match;
}

double
parse_period(char *option)
{
        static int      regex_compiled;
        static regex_t  period_regexp;
        char           *minutes = NULL;
        char           *seconds;
        double          parsed = 0;
        size_t          nmatch = 4;
        regmatch_t      pmatch[4];

        if (!regex_compiled)
                regex_compiled = compile_regex(&period_regexp, R_PERIOD, REG_EXTENDED);

        if (regexec(&period_regexp, option, nmatch, pmatch, 0) == 0) {
                if (pmatch[2].rm_so == -1)
                        seconds = option;
                else {
                        minutes = option;
                        option[pmatch[2].rm_so] = '\0';
                        seconds = &option[pmatch[3].rm_so];
                }

                if (minutes != NULL)
                        parsed = strtol(minutes, (char **)NULL, 10) * 60;
                parsed += strtol(seconds, (char **)NULL, 10);
        } else {
                errx(1, "could not parse time format: '%s'", option);
        }

        return parsed;
}

int
check_time(OggVorbis_File ovf, filter_t *filter)
{
        double          time;

        time = ov_time_total(&ovf, -1);

        if (filter->min_length_flag && filter->min_length >= time)
                return 0;
        if (filter->max_length_flag && filter->max_length <= time)
                return 0;

        return 1;
}

int
check_comments(OggVorbis_File ovf, filter_t *filter, char *filename)
{
        int             i;
        vorbis_comment *ovc;

        if (!filter->expression_flag)
                return 1;

        if ((ovc = ov_comment(&ovf, -1)) == NULL) {
                warnx("could not read vorbiscomments: %s", filename);
                return 0;
        }
        for (i = 0; i < ovc->comments; i++)
                if (regexec(&(filter->expression), ovc->user_comments[i], 0, NULL, 0) == 0)
                        return 1;

        return 0;
}

int
check_bitrate(OggVorbis_File ovf, filter_t *filter, char *filename)
{
        long            nominal;
        vorbis_info    *ovi;

        if ((ovi = ov_info(&ovf, -1)) == NULL) {
                warnx("could not read vorbis info: %s", filename);
                return 0;
        }
        nominal = ovi->bitrate_nominal;

        if (filter->min_bitrate_flag && filter->min_bitrate >= nominal)
                return 0;
        if (filter->max_bitrate_flag && filter->max_bitrate <= nominal)
                return 0;

        return 1;
}

int
compile_regex(regex_t * preg, char *expression, int expr_flags)
{
        int             errc;
        char            errstr[BUFFLEN + 1];

        if (expression == NULL)
                return 0;

        if ((errc = regcomp(preg, expression, expr_flags))) {
                regerror(errc, preg, errstr, BUFFLEN);
                errx(1, "can't compile regex '%s': %s", expression, errstr);
        }
        return 1;
}
