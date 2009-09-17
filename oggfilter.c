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
#include "options.h"

enum {
        MAXLINE = 1024
};

struct buffers {
        char           *path;
        char           *in;
};

void            fork_you(struct options *opts, struct context *ctx, struct buffers *buffs);
void            free_buffers(struct buffers *buffs);
void            free_conditions(struct conditions *cond);
struct buffers *get_buffers(struct options *opts);
struct conditions *get_conditions(struct options *opts);
void            process_loop(struct options *opts, struct context *ctx, struct buffers *buffs);
int             use_pipe(int *p);
int             set_sigchld_handler(void);
void            sigchld_handler(int);

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

        /* enter main loop or fork away */
        if (opts.processes <= 1)
                process_loop(&opts, ctx, buffs);
        else
                fork_you(&opts, ctx, buffs);

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

void
process_loop(struct options *opts, struct context *ctx, struct buffers *buffs)
{
        assert(opts != NULL);
        assert(ctx != NULL);
        assert(buffs != NULL);

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
                if (check_result ^ opts->invert)
                        printf("%s\n", path);
        }
        if (ferror(stdin))
                err(EX_SOFTWARE, "could not completely read stdin");
}

void
fork_you(struct options *opts, struct context *ctx, struct buffers *buffs)
{
        int            *fds;
        int             i;

        assert(opts != NULL);
        assert(ctx != NULL);
        assert(buffs != NULL);

        if (set_sigchld_handler() == -1)
                warn("could not set sigchld handler");

        if ((fds = malloc(opts->processes * sizeof(int))) == NULL)
                err(EX_SOFTWARE, "could not malloc file descriptors");

        for (i = 0; i < opts->processes; i++) {
                int             p[2];
                pid_t           pid;

                if (pipe(p) == -1)
                        err(EX_OSERR, "could not create pipe (%d)", i);
                fds[i] = p[1];

                switch (pid = fork()) {
                case -1:
                        err(EX_OSERR, "could not fork (%d)", i);
                        break;
                case 0:
                        continue;
                        break;
                default:
                        if (use_pipe(p) == -1)
                                err(EX_OSERR, "could not setup IPC");
                        process_loop(opts, ctx, buffs);
                        goto out;
                }
        }

        i = 0;
        while (fgets(buffs->in, MAXLINE, stdin) != NULL) {
                if (write(fds[i++ % 2], buffs->in, strlen(buffs->in)) == -1)
                        warn("could not write to subprocess");
        }
        if (ferror(stdin))
                err(EX_SOFTWARE, "could not completely read stdin");
out:
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

int
set_sigchld_handler(void)
{
        struct sigaction sa;

        sa.sa_handler = &sigchld_handler;
        sa.sa_flags = SA_NOCLDSTOP;
        if (sigemptyset(&(sa.sa_mask)) == -1)
                return (-1);

        if (sigaction(SIGCHLD, &sa, NULL) == -1)
                return (-1);

        return (0);
}

void
sigchld_handler(int sig)
{
        pid_t           pid;
        int             sts;

        assert(sig == SIGCHLD);

        if ((pid = wait(&sts)) == -1) {
                warn("could not get child status");
                return;
        }
        if (WIFEXITED(sts) && WEXITSTATUS(sts) != 0)
                warn("child %d exited abnormally: %d", pid, sts);
}
