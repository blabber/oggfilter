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
        char           *pathprefix;
        char           *pathread;
        size_t          pathsize;
        char           *path;
};

void            fork_you(struct opt_options *opts, struct chk_context *ctx, struct buffers *buffs);
void            free_buffers(struct buffers *buffs);
void            free_conditions(struct chk_conditions *cond);
struct buffers *get_buffers(struct opt_options *opts);
struct chk_conditions *get_conditions(struct opt_options *opts);
void            process_loop(struct opt_options *opts, struct chk_context *ctx, struct buffers *buffs, int doflush);
int             use_pipe(int *p);
void            wait_for_childs(void);

int
main(int argc, char **argv)
{
        struct opt_options *opts = NULL;
        struct chk_conditions *cond = NULL;
        struct chk_context *ctx = NULL;
        struct buffers *buffs = NULL;

        if (setlocale(LC_ALL, "") == NULL)
                errx(EX_SOFTWARE, "setlocale LC_ALL");

        /* setup environment */
        if ((opts = opt_get_options(argc, argv)) == NULL)
                err(EX_SOFTWARE, "opt_get_options");
        if ((buffs = get_buffers(opts)) == NULL)
                err(EX_SOFTWARE, "get_buffers");
        if ((cond = get_conditions(opts)) == NULL)
                err(EX_SOFTWARE, "get_conditions");
        if ((ctx = chk_context_open(cond)) == NULL)
                err(EX_SOFTWARE, "chk_context_open");

        /* enter main loop or fork away */
        if (opts->processes <= 1)
                process_loop(opts, ctx, buffs, 0);
        else
                fork_you(opts, ctx, buffs);

        /* free all resources */
        free_buffers(buffs);
        free_conditions(cond);
        chk_context_close(ctx);
        opt_free_options(opts);

        return (0);
}

struct buffers *
get_buffers(struct opt_options *opts)
{
        struct buffers *buffs;

        assert(opts != NULL);

        if ((buffs = malloc(sizeof(*buffs))) == NULL)
                return (NULL);

        if ((buffs->pathread = malloc(MAXLINE)) == NULL)
                err(EX_OSERR, "malloc buffs->pathread");
        buffs->pathsize = MAXLINE;

        if (opts->pathprefix != NULL) {
                size_t          len = strlen(opts->pathprefix);
                if (opts->pathprefix[len - 1] == '/') {
                        if ((buffs->pathprefix = malloc(len + 1)) == NULL)
                                err(EX_OSERR, "malloc buffs->pathprefix");
                        strncpy(buffs->pathprefix, opts->pathprefix, len);
                        buffs->pathprefix[len] = '\0';

                        buffs->pathsize += len;
                } else {
                        if ((buffs->pathprefix = malloc(len + 2)) == NULL)
                                err(EX_OSERR, "malloc buffs->pathprefix");
                        strncpy(buffs->pathprefix, opts->pathprefix, len);
                        buffs->pathprefix[len] = '/';
                        buffs->pathprefix[len + 1] = '\0';

                        buffs->pathsize += len + 1;
                }
        } else
                buffs->pathprefix = NULL;

        if ((buffs->path = malloc(buffs->pathsize)) == NULL)
                err(EX_OSERR, "malloc buffs->path");

        return (buffs);
}

void
free_buffers(struct buffers *buffs)
{
        assert(buffs != NULL);

        free(buffs->pathprefix);
        free(buffs->pathread);
}

struct chk_conditions *
get_conditions(struct opt_options *opts)
{
        struct chk_conditions *cond;
        struct element *oe;

        assert(opts != NULL);

        if ((cond = malloc(sizeof(*cond))) == NULL)
                return (NULL);

        chk_init_conditions(cond);

        cond->min_length = opts->min_length;
        cond->max_length = opts->max_length;
        cond->min_bitrate = opts->min_bitrate;
        cond->max_bitrate = opts->max_bitrate;
        cond->noignorecase = opts->noignorecase;

        for (oe = opts->expressionlist; oe != NULL; oe = oe->next) {
                struct element *ce;
                struct opt_expression *ox;
                struct chk_expression *cx;

                if ((cx = malloc(sizeof(*cx))) == NULL)
                        err(EX_OSERR, "malloc chk_expression");

                ox = oe->payload;
                cx->expression = ox->expression;
                cx->invert = ox->invert;
                if ((ce = create_element(cx)) == NULL)
                        err(EX_SOFTWARE, "create_element");

                cond->regexlist = prepend_element(ce, cond->regexlist);
        }

        return (cond);
}

void
free_conditions(struct chk_conditions *cond)
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
process_loop(struct opt_options *opts, struct chk_context *ctx, struct buffers *buffs, int doflush)
{
        assert(opts != NULL);
        assert(ctx != NULL);
        assert(buffs != NULL);
        assert(doflush == 0 || doflush == 1);

        while (fgets(buffs->pathread, MAXLINE, stdin) != NULL) {
                int             check_result;
                int             use_prefix;
                char           *newline = NULL;

                if ((newline = strchr(buffs->pathread, '\n')) != NULL)
                        newline[0] = '\0';

                if (buffs->pathprefix != NULL)
                        if (buffs->pathread[0] == '/')
                                use_prefix = 0;
                        else
                                use_prefix = 1;
                else
                        use_prefix = 0;

                if (use_prefix) {
                        snprintf(buffs->path, buffs->pathsize, "%s%s", buffs->pathprefix, buffs->pathread);
                } else {
                        strncpy(buffs->path, buffs->pathread, buffs->pathsize - 1);
                        buffs->path[buffs->pathsize] = '\0';
                }

                check_result = chk_check_file(buffs->path, ctx);
                assert(check_result == 0 || check_result == 1);
                assert(opts->invert == 0 || opts->invert == 1);
                if (check_result ^ opts->invert) {
                        puts(buffs->path);
                        if (doflush)
                                fflush(stdout);
                }
        }
        if (ferror(stdin))
                err(EX_SOFTWARE, "fgets stdin");
}

void
fork_you(struct opt_options *opts, struct chk_context *ctx, struct buffers *buffs)
{
        int            *fds;
        int             i;

        assert(opts != NULL);
        assert(ctx != NULL);
        assert(buffs != NULL);

        if ((fds = malloc(opts->processes * sizeof(int))) == NULL)
                err(EX_OSERR, "malloc fds");

        for (i = 0; i < opts->processes; i++) {
                int             j;
                int             p[2];
                pid_t           pid;

                if (pipe(p) == -1)
                        err(EX_OSERR, "pipe %d", i);

                switch (pid = fork()) {
                case -1:
                        err(EX_OSERR, "fork %d", i);
                        break;
                case 0:
                        for (j = 0; j < i; j++)
                                if (close(fds[j]) == -1)
                                        warn("close %d, %d", i, j);
                        free(fds);

                        if (use_pipe(p) == -1)
                                errx(EX_OSERR, "use_pipe %d", i);

                        process_loop(opts, ctx, buffs, 1);

                        return;
                default:
                        if (close(p[0]) == -1)
                                warn("close %d", i);
                        fds[i] = p[1];
                }
        }

        i = 0;
        while (fgets(buffs->pathread, MAXLINE, stdin) != NULL)
                if (write(fds[i++ % opts->processes], buffs->pathread, strlen(buffs->pathread)) == -1)
                        warn("write");
        if (ferror(stdin))
                err(EX_SOFTWARE, "fgets stdin");

        for (i = 0; i < opts->processes; i++)
                if (close(fds[i]) == -1)
                        warn("close %d", i);

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
                        warn("child process %d exited abnormally: %d", pid, sts);
        }
}
