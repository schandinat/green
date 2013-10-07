GREEN - A light weight PDF reader
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

`-fit=`
  with one of *none, width, height* or *page* tp select the program wide page fitting mode.
`-width=` 
  with an integer greate equal zero (in pixels) to specify the startup width of the window.
`-height=` 
  with an integer greate equal zero (in pixels) to specify the startup height of the window.
`-fullscreen=`
  startup in fullscreen mode.
`-nofullscreen=`
  startup in window mode.
`-config=`
  with a file name of a configuration file.
`-scheme=`
  with an `<id list>` (see below) to select a different scheme.

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
`c` - close document.

### FITTING
`fn` - disable page fitting mode.
`fw` - fit page width.
`fh` - fit page height.
`fp` - fit whole page.

### SEARCHING 
`s<X><RETURN> - Start search for string X.`
`n` - Show next result.

### OTHER STUFF
When starting green in Framebuffer console you might see an error regarding the mouse. 
If you don't need mouse in the console:

    SDL_NOMOUSE=1 ./green 

Should work around the problem. Other wise you should be able to use the mouse in the 
Framebuffer as none root user. 


FILES
-----
*$(HOME)/.green.conf*     
  Per user configuration file.  

*/usr/local/etc/green.conf*  
  The system wide configuration file.   


AUTHOR
------
The Green source code may be downloaded from <http://github.com/schandinat/green/>.   
Green is Licensed under GNU GPL version 3.  
This man page was written for the Debian GNU / Linux System by Oz Nahum <nahumoz@gmail.com>.

