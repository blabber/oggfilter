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
 * stdbool.h
 * iconv.h
 * regex.h
 * list.h
 */

/*
 * These conditions have to be set by the caller
 */
struct chk_conditions {
	double		 min_length;
	double		 max_length;
	long		 min_bitrate;
	long		 max_bitrate;
	struct element	*regexlist;
	bool		 noignorecase;
};

struct chk_expression {
	char	*expression;
	bool	 invert;
};

struct chk_context;

bool			 chk_check_file(char *_path, struct chk_context *_ctx);
struct chk_context	*chk_context_open(struct chk_conditions *_cond);
void			 chk_context_close(struct chk_context *_ctx);
void			 chk_init_conditions(struct chk_conditions *_cond);
