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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "checks.h"
#include "list.h"
#include "options.h"

enum {
        MAXLINE = 1024
};

struct buffers {
        char           *path;
        char           *in;
};

void            fork_you(struct opt_options *opts, struct context *ctx, struct buffers *buffs);
void            free_buffers(struct buffers *buffs);
void            free_conditions(struct conditions *cond);
struct buffers *get_buffers(struct opt_options *opts);
struct conditions *get_conditions(struct opt_options *opts);
void            process_loop(struct opt_options *opts, struct context *ctx, struct buffers *buffs, int doflush);
int             use_pipe(int *p);
void            wait_for_childs(void);

int
main(int argc, char **argv)
{
        struct opt_options *opts = NULL;
        struct conditions *cond = NULL;
        struct context *ctx = NULL;
        struct buffers *buffs = NULL;

        if (!setlocale(LC_ALL, ""))
                warnx("could not set locale");

        /* setup environment */
        if ((opts = opt_get_options(argc, argv)) == NULL)
                err(EX_SOFTWARE, "could not obtain options");
        if ((buffs = get_buffers(opts)) == NULL)
                err(EX_SOFTWARE, "could note obtain buffs");
        if ((cond = get_conditions(opts)) == NULL)
                err(EX_SOFTWARE, "could note obtain conditions");
        if ((ctx = context_open(cond)) == NULL)
                err(EX_SOFTWARE, "could not open context");

        /* enter main loop or fork away */
        if (opts->processes <= 1)
                process_loop(opts, ctx, buffs, 0);
        else
                fork_you(opts, ctx, buffs);

        /* free all resources */
        free_buffers(buffs);
        free_conditions(cond);
        context_close(ctx);
        opt_free_options(opts);

        return (0);
}

struct buffers *
get_buffers(struct opt_options *opts)
{
        const char     *errstr = "could not allocate memory: buffs->path";
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
                                err(EX_SOFTWARE, "%s", errstr);
                        strncpy(buffs->path, opts->pathprefix, strlen(opts->pathprefix) + 1);
                        buffs->in = &buffs->path[strlen(opts->pathprefix)];
                } else {
                        if ((buffs->path = malloc(strlen(opts->pathprefix) + 1 + MAXLINE)) == NULL)
                                err(EX_SOFTWARE, "%s", errstr);
                        strcpy(buffs->path, opts->pathprefix);
                        strncpy(&buffs->path[strlen(opts->pathprefix)], "/", 2);
                        buffs->in = &buffs->path[strlen(opts->pathprefix) + 1];
                }
        } else {
                if ((buffs->path = malloc(MAXLINE + 1)) == NULL)
                        err(EX_SOFTWARE, "%s", errstr);
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
get_conditions(struct opt_options *opts)
{
        struct conditions *cond;
        struct element *oe;

        assert(opts != NULL);

        if ((cond = malloc(sizeof(*cond))) == NULL)
                return (NULL);

        init_conditions(cond);

        cond->min_length = opts->min_length;
        cond->max_length = opts->max_length;
        cond->min_bitrate = opts->min_bitrate;
        cond->max_bitrate = opts->max_bitrate;
        cond->noignorecase = opts->noignorecase;

        for (oe = opts->expressionlist; oe != NULL; oe = oe->next) {
                struct element *ce;
                struct opt_expression *ox;
                struct cond_expression *cx;

                if ((cx = malloc(sizeof(*cx))) == NULL)
                        err(EX_SOFTWARE, "could not allocate cond_expression");

                ox = oe->payload;
                cx->expression = ox->expression;
                cx->invert = ox->invert;
                if ((ce = create_element(cx)) == NULL)
                        err(EX_SOFTWARE, "could not create regex element");

                cond->regexlist = prepend_element(ce, cond->regexlist);
        }

        return cond;
}

void
free_conditions(struct conditions *cond)
{
        struct element *e;

        assert(cond != NULL);

        e = cond->regexlist;
        while (e != NULL) {
                free(e->payload);
                e = destroy_element(e);
        }

        free(cond);
}

void
process_loop(struct opt_options *opts, struct context *ctx, struct buffers *buffs, int doflush)
{
        assert(opts != NULL);
        assert(ctx != NULL);
        assert(buffs != NULL);
        assert(doflush == 0 || doflush == 1);

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
                assert(opts->invert == 0 || opts->invert == 1);
                if (check_result ^ opts->invert) {
                        printf("%s\n", path);
                        if (doflush)
                                fflush(stdout);
                }
        }
        if (ferror(stdin))
                err(EX_SOFTWARE, "could not completely read stdin");
}

void
fork_you(struct opt_options *opts, struct context *ctx, struct buffers *buffs)
{
        int            *fds;
        int             i;

        assert(opts != NULL);
        assert(ctx != NULL);
        assert(buffs != NULL);

        if ((fds = malloc(opts->processes * sizeof(int))) == NULL)
                err(EX_SOFTWARE, "could not malloc file descriptors");

        for (i = 0; i < opts->processes; i++) {
                int             j;
                int             p[2];
                pid_t           pid;

                if (pipe(p) == -1)
                        err(EX_OSERR, "could not create pipe (%d)", i);

                switch (pid = fork()) {
                case -1:
                        err(EX_OSERR, "could not fork (%d)", i);
                        break;
                case 0:
                        for (j = 0; j < i; j++)
                                if (close(fds[j]) == -1)
                                        warn("child could not close write end of pipe: %d, %d", i, j);
                        free(fds);

                        if (use_pipe(p) == -1)
                                err(EX_OSERR, "child could not setup IPC");
                        process_loop(opts, ctx, buffs, 1);
                        /*
                         * Do not explicitely close the read end. You won't
                         * close stdin either
                         */
                        return;
                default:
                        if (close(p[0]) == -1)
                                warn("could not close read end of pipe: %d", i);
                        fds[i] = p[1];
                }
        }

        i = 0;
        while (fgets(buffs->in, MAXLINE, stdin) != NULL)
                if (write(fds[i++ % opts->processes], buffs->in, strlen(buffs->in)) == -1)
                        warn("could not write to subprocess");
        if (ferror(stdin))
                err(EX_SOFTWARE, "could not completely read stdin");

        for (i = 0; i < opts->processes; i++)
                if (close(fds[i]) == -1)
                        warn("could not close write end of pipe: %d", i);

        wait_for_childs();

        free(fds);
}

int
use_pipe(int *p)
{
        assert(p != NULL);

        if (close(p[1]) == -1)
                return (-1);
        if (fclose(stdin) == EOF)
                return (-1);
        if ((dup2(p[0], STDIN_FILENO)) == -1)
                return (-1);
        if ((stdin = fdopen(STDIN_FILENO, "r")) == NULL)
                return (-1);

        return (0);
}


void
wait_for_childs()
{
        pid_t           pid;
        int             sts;

        while ((pid = wait(&sts)) != -1) {
                if (WIFEXITED(sts) && WEXITSTATUS(sts) != 0)
                        warn("child %d exited abnormally: %d", pid, sts);
        }
}
