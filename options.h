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
 * list.h
 */

struct options {
        double          min_length;
        double          max_length;
        long            min_bitrate;
        long            max_bitrate;
        struct element *expressionlist;
        char           *pathprefix;
        int             invert;
        int             processes;
};

struct options *get_options(int _argc, char *_argv[]);
int             free_options(struct options *opts);
