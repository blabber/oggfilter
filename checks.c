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
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <vorbis/vorbisfile.h>

#include "checks.h"
#include "list.h"

struct chk_context {
        struct chk_conditions *cond;
        struct element *regexlist;
        iconv_t         cd;
};

struct oggfile {
        char           *path;
        OggVorbis_File  ovf;
};

struct regex {
        char           *pattern;
        regex_t        *regex;
        int             invert;
};

static int      check_bitrate(struct oggfile *of, struct chk_context *ctx);
static int      check_comments(struct oggfile *of, struct chk_context *ctx);
static int      check_time(struct oggfile *of, struct chk_context *ctx);
static int      oggfile_close(struct oggfile *of);
static int      oggfile_open(struct oggfile *of, char *path);

int
chk_check_file(char *path, struct chk_context *ctx)
{
        struct oggfile  of;
        int             match;

        assert(path != NULL);
        assert(ctx != NULL);

        if (oggfile_open(&of, path) != 0) {
                warn("oggfile_open \"%s\"", path);
                return (0);
        }
        match = check_time(&of, ctx);
        match = match && check_bitrate(&of, ctx);
        match = match && check_comments(&of, ctx);
        assert(match == 0 || match == 1);

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
        struct chk_context *ctx;
        struct element *e;

        assert(cond != NULL);

        if ((ctx = malloc(sizeof(*ctx))) == NULL)
                err(EX_SOFTWARE, "malloc chk_context");
        if ((ctx->cd = iconv_open("", "UTF-8")) == (iconv_t) (-1))
                err(EX_SOFTWARE, "iconv_open");
        ctx->cond = cond;
        ctx->regexlist = NULL;

        for (e = cond->regexlist; e != NULL; e = e->next) {
                struct regex   *re = NULL;
                struct element *ne = NULL;
                struct chk_expression *cx = NULL;
                int             errcode;
                int             flags;

                if ((re = malloc(sizeof(*re))) == NULL)
                        err(EX_SOFTWARE, "malloc regex");

                cx = e->payload;
                re->invert = cx->invert;
                re->pattern = cx->expression;
                if ((re->regex = malloc(sizeof(*(re->regex))))== NULL)
                        err(EX_SOFTWARE, "malloc regex_t");

                flags = REG_EXTENDED;
                if (!cond->noignorecase)
                        flags |= REG_ICASE;

                if ((errcode = regcomp(re->regex, re->pattern, flags)) != 0) {
                        char            errstr[128];

                        regerror(errcode, re->regex, errstr, sizeof(errstr));
                        errx(EX_USAGE, "regcomp \"%s\": %s", re->pattern, errstr);
                }
                if ((ne = create_element(re)) == NULL)
                        err(EX_SOFTWARE, "create_element regex");
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
                struct regex   *r;

                r = ctx->regexlist->payload;
                regfree(r->regex);
                free(r->regex);
                ctx->regexlist = destroy_element(ctx->regexlist);
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

static int
check_time(struct oggfile *of, struct chk_context *ctx)
{
        double          time;
        double          min_length;
        double          max_length;

        assert(of != NULL);
        assert(ctx != NULL);

        if ((time = ov_time_total(&of->ovf, -1)) == OV_EINVAL) {
                warnx("ov_time_total \"%s\"", of->path);
                return (0);
        }
        min_length = ctx->cond->min_length;
        if (min_length >= 0 && min_length > time)
                return (0);

        max_length = ctx->cond->max_length;
        if (max_length >= 0 && max_length < time)
                return (0);

        return (1);
}

static int
check_bitrate(struct oggfile *of, struct chk_context *ctx)
{
        vorbis_info    *ovi = NULL;
        long            nominal;
        long            min_bitrate;
        long            max_bitrate;

        assert(of != NULL);
        assert(ctx != NULL);

        if ((ovi = ov_info(&of->ovf, -1)) == NULL) {
                warnx("ov_info \"%s\"", of->path);
                return (0);
        }
        nominal = ovi->bitrate_nominal;

        min_bitrate = ctx->cond->min_bitrate;
        if (min_bitrate >= 0 && min_bitrate > nominal)
                return (0);

        max_bitrate = ctx->cond->max_bitrate;
        if (max_bitrate >= 0 && max_bitrate < nominal)
                return (0);

        return (1);
}

static int
check_comments(struct oggfile *of, struct chk_context *ctx)
{
        vorbis_comment *ovc = NULL;
        struct element *e = NULL;

        assert(of != NULL);
        assert(ctx != NULL);

        if ((ovc = ov_comment(&of->ovf, -1)) == NULL) {
                warnx("ov_comment \"%s\"", of->path);
                return (0);
        }
        for (e = ctx->regexlist; e != NULL; e = e->next) {
                struct regex   *re;
                int             i;
                int             match = 0;

                re = e->payload;

                for (i = 0; i < ovc->comments; i++) {
                        char            conv_comment_buffer[256];
                        char           *comment, *conv_comment;
                        char          **from, **to;
                        size_t          fromlen, tolen;

                        conv_comment = &conv_comment_buffer[0];
                        comment = ovc->user_comments[i];
                        from = &comment;
                        to = &conv_comment;
                        fromlen = strlen(comment);
                        tolen = sizeof(conv_comment_buffer);

                        while (fromlen > 0)
                                if (iconv(ctx->cd, (const char **)from, &fromlen, to, &tolen) == (size_t) (-1)) {
                                        warnx("iconv \"%s\"", ovc->user_comments[i]);
                                        return (0);
                                }
                        *to[0] = '\0';

                        if (regexec(re->regex, conv_comment_buffer, 0, NULL, 0)== 0)
                                match = 1;
                }

                assert(match == 0 || match == 1);
                assert(re->invert == 0 || re->invert == 1);
                if (!match ^ re->invert)
                        return (0);
        }

        return (1);
}
