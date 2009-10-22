/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 *                                                              Tobias Rehbein
 */

/**
 * Required includes:
 *
 * iconv.h
 * regex.h
 */

/*
 * These conditions have to be set by the caller
 */
struct conditions {
        double          min_length;
        double          max_length;
        long            min_bitrate;
        long            max_bitrate;
        char           *expression;
};

struct context;

int             check_file(char *_path, struct context *_ctx);
struct context *context_open(struct conditions *_cond);
void            context_close(struct context *_ctx);
void            init_conditions(struct conditions *_cond);
