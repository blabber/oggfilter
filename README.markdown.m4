dnl !!! This is the template used to generate the README file for oggfilter(1).
dnl !!! If you are looking for the actual README: it's named "README.markdown".

oggfilter
=========

Documentation
-------------
include(`oggfilter.1.txt')
Portability issues
------------------
This program was written in ANSI/C so it should compile on every unix-like
platform supporting an ANSI/C compiler and libvorbis. But this was written on a
FreeBSD system and has not been tested on other platforms. 

The supplied Makefile is a BSD-style Makefile and uses the FreeBSD-make
infrastructure. This is not portable, but as oggfilter is pretty simple there
should be no problem to write a simple Makefile for other platforms.

Contributing
------------
The code for oggfilter is hosted at [github][1].  Feel free to fork.  Comments,
patches and pull request are highly appreciated.

[1]:    http://www.github.com/tobreh/oggfilter
         
Found a bug?
------------
Drop me a mail or file an issue at [github][2].

[2]:    http://github.com/tobreh/oggfilter/issues

License
-------
        /*-
         * "THE BEER-WARE LICENSE" (Revision 42):
         * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice 
         * you can do whatever you want with this stuff. If we meet some day, and you 
         * think this stuff is worth it, you can buy me a beer in return.   
         *                                                              Tobias Rehbein
         */
