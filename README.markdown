oggfilter
=========

Documentation
-------------
        OGGFILTER(1)            FreeBSD General Commands Manual           OGGFILTER(1)
        
        NAME
             oggfilter -- filter a list of ogg/vorbis files using various citeria
        
        SYNOPSIS
             oggfilter [-l | --min-length period] [-L | --max-length period]
                       [-b | --min-bitrate bitrate] [-B | --max-bitrate bitrate]
                       [-x | --expression regexp] [-d | --directory directory]
                       [-E | --extended] [-v | --invert]
        
             oggfilter {-h | --help}
        
        DESCRIPTION
             The oggfilter utility reads sequentially a list of ogg/vorbis files from
             standard input  and filters this list using various criteria defined via
             command line options. All ogg/vorbis files matching these criteria are
             written to standard output.
        
             All specified criteria are combined using logical AND.
        
             These are the available command line options:
        
             -l | --min-length period
                         Matches every ogg/vorbis file with a play time longer than
                         the specified period.  period may be expressed as seconds or
                         in minutes:seconds syntax.
        
             -L | --max-length period
                         Matches every ogg/vorbis file with a play time shorter than
                         the specified period.  period may be expressed as seconds or
                         in minutes:seconds syntax.
        
             -b | --min-bitrate bitrate
                         Matches every ogg/vorbis file with a nominal bitrate higher
                         than bitrate kbps
        
             -B | --max-bitrate bitrate
                         Matches every ogg/vorbis file with a nominal bitrate lower
                         than bitrate kbps
        
             -x | --expression regexp
                         Matches every ogg/vorbis file containing at least one vorbis-
                         comment matching the regular expression regexp.  The regular
                         expression matching is always case-insensitive.
        
             -d | --directory directory
                         Prepends every line read from standard input with directory
                         if the first character of the line is not a slash.
        
             -E | --extended
                         Interpret regexp as an extended regular expression rather
                         than basic regular expressions.  See re_format(7) for a com-
                         plete discussion of regular expression formats.
        
             -v | --invert
                         Invert the result set - return all ogg/vorbis files not
                         matching the specified criteria.
        
             -h | --help
                         Print the synopsis of oggfilter and exit. This overrides any
                         other options.
        
        EXAMPLES
             To get a list of all your ogg/vorbis files tagged with genre ``Thrash
             Metal'' use the following command line:
        
                   find /my/music -type f -name '*.ogg' | oggfilter -x '^genre=thrash
                   metal$'
        
             To filter a list of ogg/vorbis files for files not tagged as ``Neo Folk''
             or ``Power Metal'' you may use:
        
                   oggfilter -v -E -x '^genre=(neo folk|power metal)$' < playlist.m3u
        
             To get a list of all ogg/vorbis files with a maximum playtime of 5 min-
             utes and a minimum playtime of 3 minutes you may use:
        
                   oggfilter -l 180 -L 5:00 < playlist.m3u
        
             To get a list of ogg/vorbis files encoded with a minimal nominal bitrate
             of 120 kbps use:
        
                   oggfilter -b 120 < playlist.m3u
        
             If you are piping from a playlist containing relative paths you can tell
             oggfilter to prepend a base path to the read ogg/vorbis files:
        
                   oggfilter -d /my/music -x '^genre=.*metal$' < relative.m3u
        
        DIAGNOSTICS
             Messages should be self-explanatory.
        
             The oggfilter utility exits 0 on success, and >0 if an error occurs.
        
        SEE ALSO
             re_format(7), vorbiscomment(1), ogginfo(1)
        
        AUTHORS
             Tobias Rehbein <tobias.rehbein@web.de>
        
        BUGS
             The code parsing period behaves funny when parsing a period formatted
             like `1:120' which is interpreted as `3 minutes'.
        
             Expect some rough edges as this was my first take on a C program.
        
        FreeBSD 7.1                     March 25, 2009                     FreeBSD 7.1

Portability issues
------------------
This program was written in ANSI/C so it should compile on every platform
supporting an ANSI/C compiler and libvorbis. But this was written on a FreeBSD
system an has not been tested on other platforms. 

The supplied Makefile is a BSD-style Makefile an uses the FreeBSD-make
infrastructure. This is not portable, but as oggfilter is pretty simple there
should be no problem to write a simple Makefile for other platforms.

Contributing
------------
The code for oggfilter is hosted at [github]("http://www.github.com/tobreh/oggfilter").
Feel free to fork.  

Comments, patches and pull request are highly appreciated.

License
-------
        /*-
         * "THE BEER-WARE LICENSE" (Revision 42):
         * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice 
         * you can do whatever you want with this stuff. If we meet some day, and you 
         * think this stuff is worth it, you can buy me a beer in return.   
         *                                                              Tobias Rehbein
         */
