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
#include "vorbis/vorbisfile.h"
#include "vorbis/codec.h"

#define MAXLINE 1024

/* prototypes */
typedef struct {
        int             min_length_flag;
        double          min_length;
        int             max_length_flag;
        double          max_length;
        char           *expression;
        int             expr_flags;
}               filter;
int             main(int argc, char **argv);
int             check_comments(OggVorbis_File ovf, filter filter);
int             check_file(char *filename, filter filter);
int             check_time(OggVorbis_File ovf, filter);
double          option_parse_double(char *option);

/* options descriptor */
static struct option longopts[] = {
        {"directory", required_argument, NULL, 'd'},
        {"min-length", required_argument, NULL, 'l'},
        {"max-length", required_argument, NULL, 'L'},
        {"expression", required_argument, NULL, 'x'},
        {"extended", no_argument, NULL, 'E'},
        {"invert", no_argument, NULL, 'v'}
};

int
main(int argc, char **argv)
{
        char            in[MAXLINE];
        char           *filename = NULL;
        char            option;
        char           *option_directory = NULL;
        size_t          size;
        int             invert = 0;
        filter          filter = {
                0,              /* min_length_flag */
                0.0,            /* min_length */
                0,              /* max_length_flag */
                0.0,            /* max_length */
                NULL,           /* expression */
                REG_ICASE       /* expression flags */
        };

        while ((option = getopt_long(argc, argv, "hd:l:L:x:Ev", longopts, NULL)) != -1)
                switch (option) {
                case 'd':
                        if (optarg[strlen(optarg) - 1] == '/') {
                                size = strlen(optarg) * sizeof(char);
                                option_directory = malloc(size + 1);
                                strncpy(option_directory, optarg, size + 1);
                        } else {
                                size = (strlen(optarg) + 1) * sizeof(char);
                                option_directory = malloc(size + 1);
                                strncpy(option_directory, optarg, size + 1);
                                strncat(option_directory, "/", 1);
                        }
                        break;
                case 'l':
                        filter.min_length_flag = 1;
                        filter.min_length = option_parse_double(optarg);
                        break;
                case 'L':
                        filter.max_length_flag = 1;
                        filter.max_length = option_parse_double(optarg);
                        break;
                case 'x':
                        filter.expression = optarg;
                        break;
                case 'E':
                        filter.expr_flags |= REG_EXTENDED;
                        break;
                case 'v':
                        invert = 1;
                        break;
                case 'h':
                default:
                        printf("oggfilter [-l|--min-length length] [-L|--max-length length] [-d directory]\n");
                        printf("          [-x|--expression expression] [-E|--extended] [-v|--invert] [-h]");
                        return 0;
                }

        while (fgets(in, MAXLINE, stdin) != NULL) {
                in[strlen(in) - 1] = '\0';
                if (in[0] == '/' || option_directory == NULL) {
                        size = strlen(in) * sizeof(char);
                        filename = malloc(size + 1);
                        strncpy(filename, in, size + 1);
                } else {
                        size = (strlen(in) + strlen(option_directory)) * sizeof(char);
                        filename = malloc(size + 1);
                        strncpy(filename, option_directory, size + 1);
                        strncat(filename, in, size - strlen(filename));
                }

                if (check_file(filename, filter) ^ invert)
                        printf("%s\n", filename);
        }

        /* free all buffers */
        if (option_directory != NULL)
                free(option_directory);
        if (filename != NULL)
                free(filename);

        return 0;
}

int
check_file(char *filename, filter filter)
{
        OggVorbis_File  ovf;
        int             match = 0;

        if (ov_fopen(filename, &ovf) != 0) {
                warnx("Ooops... couldnt open '%s'. Is this really an ogg/vorbis file?", filename);
        } else {
                match = (check_time(ovf, filter));
                if (match && filter.expression != NULL)
                        match = check_comments(ovf, filter);
                ov_clear(&ovf);
        }
        return match;
}

double
option_parse_double(char *option)
{
        char           *minutes, *seconds;
        double          parsed;

        if (strchr(option, ':') == NULL)
                parsed = strtod(option, (char **)NULL);
        else {
                minutes = strsep(&option, ":");
                seconds = strsep(&option, ":");
                parsed = (int)strtol(minutes, (char **)NULL, 10) * 60;
                parsed += (int)strtol(seconds, (char **)NULL, 10);
        }
        return parsed;
}

int
check_time(OggVorbis_File ovf, filter filter)
{
        double          time;
        time = ov_time_total(&ovf, -1);
        if (filter.min_length_flag && filter.min_length >= time)
                return 0;
        if (filter.max_length_flag && filter.max_length <= time)
                return 0;
        return 1;
}

int
check_comments(OggVorbis_File ovf, filter filter)
{
        int             i;
        vorbis_comment *ovc;
        regex_t         preg;

        if (regcomp(&preg, filter.expression, filter.expr_flags) == 0) {
                if ((ovc = ov_comment(&ovf, -1)) != NULL) {
                        for (i = 0; i < (*ovc).comments; i++)
                                if (regexec(&preg, (*ovc).user_comments[i], 0, NULL, 0) == 0)
                                        return 1;
                }
                regfree(&preg);
        }
        return 0;
}
