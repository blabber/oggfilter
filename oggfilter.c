/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 *                                                              Tobias Rehbein
 */

#include <assert.h>
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

struct buffers {
        char           *path;
        char           *in;
};

void            free_buffers(struct buffers *buffs);
void            free_conditions(struct conditions *cond);
struct buffers *get_buffers(struct options *opts);
struct conditions *get_conditions(struct options *opts);

int
main(int argc, char **argv)
{
        struct options  opts;
        struct conditions *cond = NULL;
        struct context *ctx = NULL;
        struct buffers *buffs = NULL;

        if (!setlocale(LC_ALL, ""))
                warnx("could not set locale");

        /* setup environment */
        parse_options(&opts, argc, argv);
        if ((buffs = get_buffers(&opts)) == NULL)
                err(EX_SOFTWARE, "could note obtain buffs");
        if ((cond = get_conditions(&opts)) == NULL)
                err(EX_SOFTWARE, "could note obtain conditions");
        if ((ctx = context_open(cond)) == NULL)
                err(EX_SOFTWARE, "could not open context");

        /* main loop */
        while (fgets(buffs->in, MAXLINE, stdin) != NULL) {
                int             check_result;
                char           *path = NULL;
                char           *newline;

                if ((newline = strchr(buffs->in, '\n')) != NULL)
                        newline[0] = '\0';

                if (buffs->in[0] == '/')
                        path = buffs->in;
                else
                        path = buffs->path;

                check_result = check_file(path, ctx);
                assert(check_result == 0 || check_result == 1);
                assert(opts.invert == 0 || opts.invert == 1);
                if (check_result ^ opts.invert)
                        printf("%s\n", path);
        }

        /* free all resources */
        if (buffs != NULL)
                free_buffers(buffs);
        if (cond != NULL)
                free_conditions(cond);
        if (ctx != NULL)
                context_close(ctx);

        return (0);
}

struct buffers *
get_buffers(struct options *opts)
{
        struct buffers *buffs;

        assert(opts != NULL);

        if ((buffs = malloc(sizeof(*buffs))) == NULL)
                return (NULL);

        /*
         * The strncpy(3) calls in this conditional construct copy one byte
         * more than needed to copy the string. This makes sure the string
         * will be null terminated.
         * 
         * I tried to write this in a more compact way, but the resulting code
         * was not very readable, so I'll keep it this way.
         */
        if (opts->pathprefix != NULL) {
                if (opts->pathprefix[strlen(opts->pathprefix) - 1] == '/') {
                        if ((buffs->path = malloc(strlen(opts->pathprefix) + MAXLINE)) == NULL)
                                err(EX_SOFTWARE, "could not allocate memory: path_buffer");
                        strncpy(buffs->path, opts->pathprefix, strlen(opts->pathprefix) + 1);
                        buffs->in = &buffs->path[strlen(opts->pathprefix)];
                } else {
                        if ((buffs->path = malloc(strlen(opts->pathprefix) + 1 + MAXLINE)) == NULL)
                                err(EX_SOFTWARE, "could not allocate memory: path_buffer");
                        strcpy(buffs->path, opts->pathprefix);
                        strncpy(&buffs->path[strlen(opts->pathprefix)], "/", 2);
                        buffs->in = &buffs->path[strlen(opts->pathprefix) + 1];
                }
        } else {
                if ((buffs->path = malloc(MAXLINE + 1)) == NULL)
                        err(EX_SOFTWARE, "could not allocate memory: path_buffer");
                buffs->in = buffs->path;
        }

        return (buffs);
}

void
free_buffers(struct buffers *buffs)
{
        assert(buffs != NULL);

        if (buffs->path != NULL)
                free(buffs->path);
        buffs->path = NULL;
        buffs->in = NULL;
}

struct conditions *
get_conditions(struct options *opts)
{
        struct conditions *cond;

        assert(opts != NULL);

        if ((cond = malloc(sizeof(*cond))) == NULL)
                return (NULL);

        init_conditions(cond);

        cond->min_length = opts->min_length;
        cond->max_length = opts->max_length;
        cond->min_bitrate = opts->min_bitrate;
        cond->max_bitrate = opts->max_bitrate;
        cond->expression = opts->expression;

        return cond;
}

void
free_conditions(struct conditions *cond)
{
        assert(cond != NULL);

        free(cond);
}
