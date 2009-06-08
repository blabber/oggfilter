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

/* prototypes */
int             main(int argc, char **argv);

int
main(int argc, char *argv[])
{
        char           *path_buffer = NULL;
        char           *path = NULL;
        char           *in = NULL;
        char           *newline;
        struct options  opts;
        struct context *ctx;
        struct conditions cond;

        if (!setlocale(LC_ALL, ""))
                warnx("could not set locale");

        parse_options(&opts, argc, argv);

        /*
         * The strncpy(3) calls in this conditional construct copy one byte
         * more than needed to copy the string. This makes sure the string
         * will be null terminated.
         * 
         * I tried to write this in a more compact way, but the resulting code
         * was not very readable, so I'll keep it this way.
         */
        if (opts.pathprefix != NULL) {
                if (opts.pathprefix[strlen(opts.pathprefix) - 1] == '/') {
                        if ((path_buffer = malloc(strlen(opts.pathprefix) + MAXLINE)) == NULL)
                                err(EX_SOFTWARE, "could not allocate memory: path_buffer");
                        strncpy(path_buffer, opts.pathprefix, strlen(opts.pathprefix) + 1);
                        in = &path_buffer[strlen(opts.pathprefix)];
                } else {
                        if ((path_buffer = malloc(strlen(opts.pathprefix) + 1 + MAXLINE)) == NULL)
                                err(EX_SOFTWARE, "could not allocate memory: path_buffer");
                        strcpy(path_buffer, opts.pathprefix);
                        strncpy(&path_buffer[strlen(opts.pathprefix)], "/", 2);
                        in = &path_buffer[strlen(opts.pathprefix) + 1];
                }
        } else {
                if ((path_buffer = malloc(MAXLINE + 1)) == NULL)
                        err(EX_SOFTWARE, "could not allocate memory: path_buffer");
                in = path_buffer;
        }

        /* set up context */
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
                        path = in;
                else
                        path = path_buffer;

                if (check_file(path, ctx) ^ opts.invert)
                        printf("%s\n", path);
        }

        /* free all resources */
        if (path != NULL)
                free(path);
        if (ctx != NULL)
                context_close(ctx);

        return 0;
}
