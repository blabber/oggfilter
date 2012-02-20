/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 *                                                              Tobias Rehbein
 */

#include <assert.h>
#include <err.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "checks.h"
#include "list.h"
#include "options.h"

static void			 fork_you(struct opt_options *opts, struct chk_context *ctx);
static void			 free_conditions(struct chk_conditions *cond);
static struct chk_conditions 	*get_conditions(struct opt_options *opts);
static void			 process_loop(struct opt_options *opts, struct chk_context *ctx, bool doflush);
static void			 remove_trailing_slashes(char *path);
static int			 use_pipe(int *p);
static void			 wait_for_childs(void);

int
main(int argc, char **argv)
{

	if (setlocale(LC_ALL, "") == NULL)
		errx(EXIT_FAILURE, "setlocale LC_ALL");

	/* setup environment */
	struct opt_options *opts = opt_get_options(argc, argv);
	if (opts == NULL)
		err(EXIT_FAILURE, "opt_get_options");
	struct chk_conditions *cond = get_conditions(opts);
	if (cond == NULL)
		err(EXIT_FAILURE, "get_conditions");
	struct chk_context *ctx = chk_context_open(cond);
	if (ctx == NULL)
		err(EXIT_FAILURE, "chk_context_open");

	/* enter main loop or fork away */
	if (opts->processes <= 1)
		process_loop(opts, ctx, false);
	else
		fork_you(opts, ctx);

	/* free all resources */
	chk_context_close(ctx);
	free_conditions(cond);
	opt_free_options(opts);

	return (0);
}

static struct chk_conditions *
get_conditions(struct opt_options *opts)
{
	assert(opts != NULL);

	struct chk_conditions *cond = malloc(sizeof(*cond));
	if (cond == NULL)
		return (NULL);

	chk_init_conditions(cond);

	cond->min_length = opts->min_length;
	cond->max_length = opts->max_length;
	cond->min_bitrate = opts->min_bitrate;
	cond->max_bitrate = opts->max_bitrate;
	cond->noignorecase = opts->noignorecase;

	for (struct element *oe = opts->expressionlist; oe != NULL; oe = oe->next) {
		struct chk_expression *cx = malloc(sizeof(*cx));
		if (cx == NULL)
			err(EXIT_FAILURE, "malloc chk_expression");

		struct opt_expression *ox = oe->payload;
		cx->expression = ox->expression;
		cx->invert = ox->invert;

		struct element *ce;
		if ((ce = create_element(cx)) == NULL)
			err(EXIT_FAILURE, "create_element");

		cond->regexlist = prepend_element(ce, cond->regexlist);
	}

	return (cond);
}

static void
free_conditions(struct chk_conditions *cond)
{
	assert(cond != NULL);

	for (struct element *e = cond->regexlist; e != NULL; e = destroy_element(e))
		free(e->payload);

	free(cond);
}

static void
process_loop(struct opt_options *opts, struct chk_context *ctx, bool doflush)
{
	assert(opts != NULL);
	assert(ctx != NULL);

	char pathread[LINE_MAX];
	while (fgets(pathread, LINE_MAX, stdin) != NULL) {
		char *newline = NULL;
		if ((newline = strchr(pathread, '\n')) != NULL)
			newline[0] = '\0';

		char *path = malloc(LINE_MAX);
		if (path == NULL)
			err(EXIT_FAILURE, "malloc path");

		char *pathprefix = NULL;
		if (opts->pathprefix != NULL) {
			pathprefix = strdup(opts->pathprefix);
			if (pathprefix == NULL)
				err(EXIT_FAILURE, "strdup pathprefix");
			remove_trailing_slashes(pathprefix);
		}
		if (pathprefix != NULL && pathread[0] != '/')
			snprintf(path, LINE_MAX, "%s/%s", pathprefix, pathread);
		else {
			strncpy(path, pathread, LINE_MAX - 2);
			path[LINE_MAX - 1] = '\0';
		}
		free(pathprefix);

		bool check_result = chk_check_file(path, ctx);
		if (check_result ^ opts->invert) {
			fputs(path, stdout);
			if (opts->print0)
				putc('\0', stdout);
			else
				putc('\n', stdout);

			if (doflush)
				fflush(stdout);
		}
		free(path);
	}
	if (ferror(stdin))
		err(EXIT_FAILURE, "fgets stdin");
}

static void
remove_trailing_slashes(char *path)
{
	assert(path != NULL);

	char *n = strchr(path, (int)'\0');
	assert(n != NULL);
	while (*n != path[0] && *(--n) == '/')
		*n = '\0';
}

static void
fork_you(struct opt_options *opts, struct chk_context *ctx)
{
	assert(opts != NULL);
	assert(ctx != NULL);

	int *fds = malloc(opts->processes * sizeof(int));
	if (fds == NULL)
		err(EXIT_FAILURE, "malloc fds");

	for (int i = 0; i < opts->processes; i++) {
		int p[2];
		if (pipe(p) == -1)
			err(EXIT_FAILURE, "pipe %d", i);

		pid_t pid = fork();
		switch (pid) {
		case -1:
			err(EXIT_FAILURE, "fork %d", i);
			break;
		case 0:
			/* close all write ends opened until now */
			for (int j = 0; j < i; j++)
				if (close(fds[j]) == -1)
					warn("close %d, %d", i, j);
			free(fds);

			if (use_pipe(p) == -1)
				errx(EXIT_FAILURE, "use_pipe %d", i);

			process_loop(opts, ctx, true);

			return;
		default:
			/*
			 * close the read end of pipe and store write end for
			 * later use
			 */
			if (close(p[0]) == -1)
				warn("close %d", i);
			fds[i] = p[1];
		}
	}

	char pathread[LINE_MAX];
	int fd = 0;
	while (fgets(pathread, LINE_MAX, stdin) != NULL)
		if (write(fds[fd++ % opts->processes], pathread, strlen(pathread)) == -1)
			warn("write");
	if (ferror(stdin))
		err(EXIT_FAILURE, "fgets stdin");

	for (int i = 0; i < opts->processes; i++)
		if (close(fds[i]) == -1)
			warn("close %d", i);

	wait_for_childs();

	free(fds);
}

static int
use_pipe(int *p)
{
	assert(p != NULL);

	/* close write end of pipe and stdin of process */
	if (close(p[1]) == -1)
		return (-1);
	if (fclose(stdin) == EOF)
		return (-1);

	/* use the read end of pipe as stdin */
	if ((dup2(p[0], STDIN_FILENO)) == -1)
		return (-1);
	if ((stdin = fdopen(STDIN_FILENO, "r")) == NULL)
		return (-1);

	return (0);
}

static void
wait_for_childs(void)
{
	pid_t pid;
	int sts;
	while ((pid = wait(&sts)) != -1)
		if (WIFEXITED(sts) && WEXITSTATUS(sts) != 0)
			warn("child process %d exited abnormally: %d", pid, sts);
}
