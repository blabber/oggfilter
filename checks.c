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
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <vorbis/vorbisfile.h>

#include "checks.h"
#include "list.h"

enum {
	COMMENTLEN = LINE_MAX
};

struct chk_context {
	struct chk_conditions	*cond;
	struct element 		*regexlist;
	iconv_t 		 cd;
};

struct oggfile {
	char			*path;
	OggVorbis_File		 ovf;
};

struct regex {
	char			*pattern;
	regex_t			*regex;
	int			 invert;
};

static bool	check_bitrate(struct oggfile *of, struct chk_context *ctx);
static bool	check_comments(struct oggfile *of, struct chk_context *ctx);
static bool	check_time(struct oggfile *of, struct chk_context *ctx);
static int	oggfile_close(struct oggfile *of);
static int	oggfile_open(struct oggfile *of, char *path);

bool
chk_check_file(char *path, struct chk_context *ctx)
{
	assert(path != NULL);
	assert(ctx != NULL);

	struct oggfile of;
	if (oggfile_open(&of, path) != 0) {
		warn("oggfile_open \"%s\"", path);
		return (0);
	}

	bool match = check_time(&of, ctx);
	match = match && check_bitrate(&of, ctx);
	match = match && check_comments(&of, ctx);

	if (oggfile_close(&of) != 0)
		warn("oggfile_close \"%s\"", path);

	return (match);
}

void
chk_init_conditions(struct chk_conditions *cond)
{
	assert(cond != NULL);

	cond->min_length = -1;
	cond->max_length = -1;
	cond->min_bitrate = -1;
	cond->max_bitrate = -1;
	cond->regexlist = NULL;
}

struct chk_context *
chk_context_open(struct chk_conditions *cond)
{
	assert(cond != NULL);

	struct chk_context *ctx = malloc(sizeof(*ctx));
	if (ctx == NULL)
		err(EXIT_FAILURE, "malloc chk_context");
	if ((ctx->cd = iconv_open("", "UTF-8")) == (iconv_t) (-1))
		err(EXIT_FAILURE, "iconv_open");
	ctx->cond = cond;
	ctx->regexlist = NULL;

	for (struct element *e = cond->regexlist; e != NULL; e = e->next) {
		struct regex *re = malloc(sizeof(*re));
		if (re == NULL)
			err(EXIT_FAILURE, "malloc regex");

		struct chk_expression *cx = e->payload;
		re->invert = cx->invert;
		re->pattern = cx->expression;
		if ((re->regex = malloc(sizeof(*(re->regex))))== NULL)
			err(EXIT_FAILURE, "malloc regex_t");

		int flags = REG_EXTENDED;
		if (!cond->noignorecase)
			flags |= REG_ICASE;

		int errcode = regcomp(re->regex, re->pattern, flags);
		if (errcode != 0) {
			char errstr[128];
			regerror(errcode, re->regex, errstr, sizeof(errstr));
			errx(EXIT_FAILURE, "regcomp \"%s\": %s", re->pattern, errstr);
		}

		struct element *ne = create_element(re);
		if (ne == NULL)
			err(EXIT_FAILURE, "create_element regex");
		ctx->regexlist = prepend_element(ne, ctx->regexlist);
	}

	return (ctx);
}

void
chk_context_close(struct chk_context *ctx)
{
	assert(ctx != NULL);

	if (iconv_close(ctx->cd) == -1)
		warn("iconv_close");

	while (ctx->regexlist != NULL) {
		struct regex *r = ctx->regexlist->payload;
		regfree(r->regex);
		free(r->regex);
		ctx->regexlist = destroy_element(ctx->regexlist);
		free(r);
	}

	free(ctx);
}

static int
oggfile_open(struct oggfile *of, char *path)
{
	assert(of != NULL);
	assert(path != NULL);

	if (ov_fopen((char *)path, &of->ovf) != 0)
		return (1);

	of->path = path;

	return (0);
}

static int
oggfile_close(struct oggfile *of)
{
	assert(of != NULL);

	if (ov_clear(&of->ovf) != 0)
		return (1);

	of->path = NULL;

	return (0);
}

static bool
check_time(struct oggfile *of, struct chk_context *ctx)
{
	assert(of != NULL);
	assert(ctx != NULL);

	double time = ov_time_total(&of->ovf, -1);
	if (time == OV_EINVAL) {
		warnx("ov_time_total \"%s\"", of->path);
		return (false);
	}

	double min_length = ctx->cond->min_length;
	if (min_length >= 0 && min_length > time)
		return (false);

	double max_length = ctx->cond->max_length;
	if (max_length >= 0 && max_length < time)
		return (false);

	return (true);
}

static bool
check_bitrate(struct oggfile *of, struct chk_context *ctx)
{
	assert(of != NULL);
	assert(ctx != NULL);

	vorbis_info *ovi = ov_info(&of->ovf, -1);
	if (ovi == NULL) {
		warnx("ov_info \"%s\"", of->path);
		return (false);
	}

	long nominal = ovi->bitrate_nominal;

	long min_bitrate = ctx->cond->min_bitrate;
	if (min_bitrate >= 0 && min_bitrate > nominal)
		return (false);

	long max_bitrate = ctx->cond->max_bitrate;
	if (max_bitrate >= 0 && max_bitrate < nominal)
		return (false);

	return (true);
}

static bool
check_comments(struct oggfile *of, struct chk_context *ctx)
{
	assert(of != NULL);
	assert(ctx != NULL);

	vorbis_comment *ovc = NULL;
	if ((ovc = ov_comment(&of->ovf, -1)) == NULL) {
		warnx("ov_comment \"%s\"", of->path);
		return (false);
	}
	for (struct element *e = ctx->regexlist; e != NULL; e = e->next) {
		int		match = 0;
		struct regex *re = e->payload;

		for (int i = 0; i < ovc->comments; i++) {
			char           *comment, *conv_comment;
			if ((conv_comment = malloc(COMMENTLEN)) == NULL)
				err(EXIT_FAILURE, "malloc(%d)", COMMENTLEN);
			comment = ovc->user_comments[i];

			const char *from = comment;
			char *to = conv_comment;
			size_t fromlen = strlen(comment);
			size_t tolen = COMMENTLEN;

			if (iconv(ctx->cd, NULL, NULL, &to, &tolen) == (size_t) (-1))
				err(EXIT_FAILURE, "reset iconv descriptor");
			while (fromlen > 0)
				if (iconv(ctx->cd, &from, &fromlen, &to, &tolen) == (size_t) (-1)) {
					warn("iconv \"%s\"", ovc->user_comments[i]);
					free(conv_comment);
					return (0);
				}
			*to = '\0';

			if (regexec(re->regex, conv_comment, 0, NULL, 0)== 0)
				match = 1;

			free(conv_comment);
		}

		assert(match == 0 || match == 1);
		assert(re->invert == 0 || re->invert == 1);
		if (!match ^ re->invert)
			return (false);
	}

	return (true);
}
