oggfilter
=========

What is it?
-----------
oggfilter is a commandline tool used to filter a set of ogg/vorbis files. It
checks certain properties defined via command line switches an returns all files
matching the filter criteria.

How can I use it?
-----------------

### Filter songs using playtime
Filter all ogg/files for file playing longer than 2 minutes and shorter than 300
seconds:

`find . -type f -name '*.ogg' | oggfilter -l 2:00 -L 300`

As you can see the playtime can be given in the form `minutes:seconds` or
`seconds`.

### Filter files with relative paths
If you are piping the content of a m3u playlist using relative paths to
oggfilter you can provide a base directory using the `-d` parameter.

`oggfilter -d /my/music -l 2:30 < relative.m3u`

The value passed using the `-d` parameter will be prepended to any filename not
starting with a slash '/'.

Why was it written?
-------------------
I haven't checked for existing code that's doing what I'm doing here. Instead I
thought this would be a tangible project to make my first practical experiences
in C. 

Portability issues?
-------------------
This program was written in ANSI/C so it should compile on every platform
supporting an ANSI/C compiler and libvorbis. But this was written on a FreeBSD
system an has not been tested on other platforms. 

The supplied Makefile is a BSD-style Makefile an uses the FreeBSD-make
infrastructure. This is not portable, but as oggfilter is pretty simple there
should be no problem to write a simple Makefile for other platforms.

Todo
----
This is a first version. The only filter implemented so far are

 * minimum play-length
 * maximum play-length

Others will follow, e.g. vorbiscomments or quality.

License
-------
        /*-
         * "THE BEER-WARE LICENSE" (Revision 42):
         * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice 
         * you can do whatever you want with this stuff. If we meet some day, and you 
         * think this stuff is worth it, you can buy me a beer in return.   
         *                                                              Tobias Rehbein
         */

Last words
----------
As this is a learning-by-doing project any help, patches, pull-requests  or
comments regarding the code will be greatly appreciated.
