FOO 1 "MARCH 1995" Linux "User Manuals"
=======================================

NAME
----

green - a lightweight PDF reader for the Framebuffer using libpoppler

SYNOPSIS
--------

`green` [`options`] <`PDF file 1`> *[PDF file 2]* ...

DESCRIPTION
-----------

`green` is meant to be a light PDF reader for the Linux Framebuffer. 
However, it can also be used inside a graphical X11 Session like GNOME or
MATE. 

`green` features:

 - uses libpoppler for PDF reading
 - uses SDL to support various frontends (including framebuffer)
 - multiple documents
 - single page mode
 - fit width, height or page
 - zooming
 - goto page
 - search function
 - scheme support


OPTIONS
-------

`-b`
  Do not write "busy" to stdout while processing.

`-c` *config-file*
  Use the alternate system wide *config-file* instead of */etc/foo.conf*. This
  overrides any `FOOCONF` environment variable.

`-a`
  In addition to the baz segments, also parse the blurfl headers.

`-r`
  Recursive mode. Operates as fast as lightning at the expense of a megabyte
  of virtual memory.

PROGRAM OPERATION
------------------
`<TAB>` - Go to the next open document.  
`<F<n>>` - Go to the n-th document.  
`ESC` - Escape current input mode.    
`q` - Quit


NAVIGATION INSIDE A DOCUMENT
----------------------------
`<h, left arrow>` - Scroll left.  
`<l, right arrow>` - Scroll right.  
`<j, down arrow>` - Scroll down.  
`<k, up arrow>` - Scroll up.  
`<pg up>` - Go to previous page.  
`<pg dn>` - Go to next page.  
`<g<n>RETURN>` - Go to page n.  
`<+,->` - Zoom in, Zoom out.  

### SEARCHING 

FILES
-----
*$(HOME)/.green.conf* 
  Per user configuration file.
*/usr/local/etc/green.conf*
  The system wide configuration file. 


AUTHOR
------
The Green source code may be downloaded
from <http://github.com/schandinat/green/>. 
Green is Licensed under GNU GPL version 3.
This man page was written for the Debian Distribution by Oz Nahum <nahumoz@gmail.com>.


