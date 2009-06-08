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
#include <locale.h>
#include <iconv.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include "options.h"

#define MAXLINE         1024
#define BUFFLEN         256
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
int             convert(char *from, char *to, size_t tolen);
double          parse_period(char *option);

/* Global variables */
static iconv_t  cd;

int
main(int argc, char *argv[])
{
        char           *filename_buffer = NULL;
        char           *filename = NULL;
        char           *in = NULL;
        char           *newline;
        struct options  opts;

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

        if (!setlocale(LC_ALL, ""))
                warnx("could not set locale");
        if ((cd = iconv_open("char", "UTF-8")) == (iconv_t) (-1))
                warnx("could not open conversion descriptor");

        parse_options(&opts, argc, argv);

        if (opts.pathprefix != NULL) {
                if (opts.pathprefix[strlen(opts.pathprefix) - 1] == '/') {
                        TRY_MALLOC(filename_buffer, strlen(opts.pathprefix) * sizeof(char) + MAXLINE + 1);
                        strncpy(filename_buffer, opts.pathprefix, strlen(opts.pathprefix) + 1);
                        in = &filename_buffer[strlen(opts.pathprefix)];
                } else {
                        TRY_MALLOC(filename_buffer,
                                   strlen(opts.pathprefix) * sizeof(char) + 1 + MAXLINE + 1);
                        strncpy(filename_buffer, opts.pathprefix, strlen(opts.pathprefix));
                        strncpy(&filename_buffer[strlen(opts.pathprefix)], "/", 2);
                        in = &filename_buffer[strlen(opts.pathprefix) + 1];
                }
        } else {
                TRY_MALLOC(filename_buffer, MAXLINE + 1);
                in = filename_buffer;
        }

        filter.min_length_flag = (opts.min_length != -1);
        filter.min_length = opts.min_length;
        filter.max_length_flag = (opts.max_length != -1);
        filter.max_length = opts.max_length;
        filter.min_bitrate_flag = (opts.min_bitrate != -1);
        filter.min_bitrate = opts.min_bitrate;
        filter.max_bitrate_flag = (opts.max_bitrate != -1);
        filter.max_bitrate = opts.max_bitrate;
        filter.expression_flag = compile_regex(&filter.expression, opts.expression, REG_ICASE | REG_EXTENDED);

        /* main loop */
        while (fgets(in, MAXLINE, stdin) != NULL) {
                if ((newline = strchr(in, '\n')) != NULL)
                        newline[0] = '\0';

                if (in[0] == '/')
                        filename = in;
                else
                        filename = filename_buffer;

                if (check_file(filename, &filter) ^ opts.invert)
                        printf("%s\n", filename);
        }

        /* free all resources */
        if (filename != NULL)
                free(filename);
        if (filter.expression_flag)
                regfree(&filter.expression);
        if (cd != (iconv_t) (-1))
                iconv_close(cd);

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
        static char     converted_comment[BUFFLEN];

        if (!filter->expression_flag)
                return 1;

        if ((ovc = ov_comment(&ovf, -1)) == NULL) {
                warnx("could not read vorbiscomments: %s", filename);
                return 0;
        }
        for (i = 0; i < ovc->comments; i++) {
                if (convert(ovc->user_comments[i], converted_comment, BUFFLEN) == 0)
                        if (regexec(&(filter->expression), converted_comment, 0, NULL, 0) == 0)
                                return 1;
        }

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

int
convert(char *from, char *to, size_t tolen)
{
        char          **fromp;
        char          **top;
        size_t          fromlen;

        if (cd == (iconv_t) (-1))
                return 1;

        fromlen = strlen(from);
        fromp = &from;
        top = &to;

        while (fromlen > 0)
                if (iconv(cd, (const char **)fromp, &fromlen, top, &tolen) == (size_t) (-1)) {
                        warnx("could not convert: %s", from);
                        return 1;
                }
        *top[0] = '\0';

        return 0;
}
