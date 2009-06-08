/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 *                                                              Tobias Rehbein
 */

#include <err.h>
#include <iconv.h>
#include <locale.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "checks.h"
#include "options.h"

#define MAXLINE         1024
#define TRY_MALLOC(v,s) if ((v = malloc(s)) == NULL) err(1 ,"%s" ,#v)


/* prototypes */
int             main(int argc, char **argv);

int
main(int argc, char *argv[])
{
        char           *filename_buffer = NULL;
        char           *filename = NULL;
        char           *in = NULL;
        char           *newline;
        struct options  opts;
        struct conditions cond;
        struct context *ctx;


        if (!setlocale(LC_ALL, ""))
                warnx("could not set locale");

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

        init_conditions(&cond);
        cond.min_length = opts.min_length;
        cond.max_length = opts.max_length;
        cond.min_bitrate = opts.min_bitrate;
        cond.max_bitrate = opts.max_bitrate;
        cond.expression = opts.expression;

        if ((ctx = context_open(&cond)) == NULL)
                errx(EX_SOFTWARE, "could not open context");

        /* main loop */
        while (fgets(in, MAXLINE, stdin) != NULL) {
                if ((newline = strchr(in, '\n')) != NULL)
                        newline[0] = '\0';

                if (in[0] == '/')
                        filename = in;
                else
                        filename = filename_buffer;

                if (check_file(filename, ctx) ^ opts.invert)
                        printf("%s\n", filename);
        }

        /* free all resources */
        if (filename != NULL)
                free(filename);
        if (ctx != NULL)
                context_close(ctx);

        return 0;
}
