/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 *                                                              Tobias Rehbein
 */

struct options {
        double          min_length;
        double          max_length;
        long            min_bitrate;
        long            max_bitrate;
        char           *expression;
        char           *pathprefix;
        int             invert;
        int             processes;
};

void            parse_options(struct options *_opts, int _argc, char *_argv[]);
