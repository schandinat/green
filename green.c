/* green - the PDF reader
 * Copyright (C) 2009 Florian Tobias Schandinat
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "green.h"


int	Green_SDL_Main( Green_RTD *rtd );


char*	Green_FilenameToURI( char *filename )
{
	const char	*prefix = "file:";
	char	abs = filename[0] == '/',
		*wd = abs ? NULL : getcwd( NULL, 0 ),
		*uri = malloc( strlen( prefix ) + strlen( filename ) + 1
			+ (abs ? 0 : strlen( wd ) + 1) );
	
	if (!uri || !(abs || wd))
	{
		free( uri );
		free( wd );
		return NULL;
	}
	
	if (abs)
		sprintf( uri, "%s%s", prefix, filename ); 
	else
	{
		sprintf( uri, "%s%s/%s", prefix, wd, filename );
		free( wd );
	}
	
	return uri;
}

inline static
void	GetScrollRegion( Green_Document *doc, int w, int h, int *scroll_w, int *scroll_h )
{
	PopplerPage	*page;
	
	page = poppler_document_get_page( doc->doc, doc->page_cur );
	Green_GetDimension( page, scroll_w, scroll_h, Green_Fit( doc, w, h ) * doc->finescale );
	g_object_unref( G_OBJECT( page ) );
	if (*scroll_w < w)
		*scroll_w = 0;
	else	
		*scroll_w -= w;
	
	if (*scroll_h < h)
		*scroll_h = 0;
	else	
		*scroll_h -= h;
	
	return;
}

int	Green_Open( Green_RTD *rtd, char *uri )
{
	Green_Document	**tmp, *doc = malloc( sizeof( *doc ) );
	PopplerPage	*page;
	int	i;
	
	if (!doc)
		return -1;
	
	doc->doc = poppler_document_new_from_file( uri, NULL, NULL );
	if (!doc->doc)
	{
		free( doc );
		return -2;
	}
	
	doc->uri = uri;
	doc->page_count = poppler_document_get_n_pages( doc->doc );
	doc->page_cur = 0;
	doc->xoffset = 0;
	doc->yoffset = 0;
	doc->fit_method = rtd->fit_method;
	doc->finescale = 1;
	doc->search_str = NULL;
	doc->bb = rtd->bb;
	for (i = 0; i < rtd->doc_count; i++)
	{
		if (rtd->docs[i])
			continue;
		
		rtd->docs[i] = doc;
		return i;
	}
	
	i = rtd->doc_count;
	tmp = realloc( rtd->docs, (rtd->doc_count + 1) * sizeof( *tmp ) );
	if (!tmp)
	{
		g_object_unref( G_OBJECT( doc->doc ) );
		free( doc );
		return -1;
	}
	
	tmp[i] = doc;
	rtd->docs = tmp;
	rtd->doc_count++;
	return i;
}

void	Green_Close( Green_RTD *rtd, int id )
{
	int	n;
	
	if (id < 0 || id >= rtd->doc_count || !rtd->docs[id])
		return;
	
	g_object_unref( G_OBJECT( rtd->docs[id]->doc ) );
	free( rtd->docs[id]->search_str );
	free( rtd->docs[id] );
	rtd->docs[id] = NULL;
	if (id < rtd->doc_count - 1)
		return;
	
	for (n = id; n > 0 && !rtd->docs[n-1]; n-- );
	rtd->doc_count = n;
	rtd->docs = realloc( rtd->docs, n * sizeof( *rtd->docs ) );
	return;
}

double	Green_Fit( Green_Document *doc, int w, int h )
{
	PopplerPage	*page;
	double	pwidth, pheight;
	
	if (doc->fit_method == NATURAL)
		return 1;
	
	page = poppler_document_get_page( doc->doc, doc->page_cur );
	poppler_page_get_size( page, &pwidth, &pheight );
	if (doc->fit_method == WIDTH)
		return w / pwidth;
	else if (doc->fit_method == HEIGHT)
		return h / pheight;
	else if (doc->fit_method == PAGE)
		return (w / pwidth <= h / pheight) ? w / pwidth : h / pheight;
	
	return 1;
}

void	Green_ScrollRelative( Green_Document *doc, int x, int y, int w, int h )
{
	unsigned char	bb_mode;
	int	abs_x = x < 0 ? -x : x,
		abs_y = y < 0 ? -y : y,
		max_x, max_y;
	
	GetScrollRegion( doc, w, h, &max_x, &max_y );
	if (abs_x > abs_y && x > 0 && doc->xoffset >= max_x)
	{
		if (!(doc->bb&0xF0) && (doc->bb&0x03))
		{
			bb_mode = doc->bb&0x03;
			if (bb_mode == 1)
				Green_GotoPage( doc, doc->page_cur + 1 );
			else if (bb_mode == 2)
				Green_GotoPage( doc, doc->page_cur - 1 );
		}
		
		return;
	}
	else if (abs_x > abs_y && x < 0 && doc->xoffset <= 0)
	{
		if (!(doc->bb&0xF0) && (doc->bb&0x03))
		{
			bb_mode = doc->bb&0x03;
			if (bb_mode == 1)
			{
				if (Green_GotoPage( doc, doc->page_cur - 1 ))
				{
					GetScrollRegion( doc, w, h, &max_x, &max_y );
					doc->xoffset = max_x;
				}
			}
			else if (bb_mode == 2)
			{
				if (Green_GotoPage( doc, doc->page_cur + 1 ))
				{
					GetScrollRegion( doc, w, h, &max_x, &max_y );
					doc->xoffset = max_x;
				}
			}
		}
		
		return;
	}
	else if (abs_x < abs_y && y > 0 && doc->yoffset >= max_y)
	{
		if (!(doc->bb&0xF0) && (doc->bb&0x0C))
		{
			bb_mode = (doc->bb>>2)&0x03;
			if (bb_mode == 1)
				Green_GotoPage( doc, doc->page_cur + 1 );
			else if (bb_mode == 2)
				Green_GotoPage( doc, doc->page_cur - 1 );
		}
		
		return;
	}
	else if (abs_x < abs_y && y < 0 && doc->yoffset <= 0)
	{
		if (!(doc->bb&0xF0) && (doc->bb&0x0C))
		{
			bb_mode = (doc->bb>>2)&0x03;
			if (bb_mode == 1)
			{
				if (Green_GotoPage( doc, doc->page_cur - 1 ))
				{
					GetScrollRegion( doc, w, h, &max_x, &max_y );
					doc->yoffset = max_y;
				}
			}
			else if (bb_mode == 2)
			{
				if (Green_GotoPage( doc, doc->page_cur + 1 ))
				{
					GetScrollRegion( doc, w, h, &max_x, &max_y );
					doc->yoffset = max_y;
				}
			}
		}
		
		return;
	}
	
	if (x <= 0 && abs_x > doc->xoffset )
		doc->xoffset = 0;
	else if (x >= 0 && abs_x > max_x - doc->xoffset)
		doc->xoffset = max_x;
	else
		doc->xoffset += x;
	
	if (y <= 0 && abs_y > doc->yoffset )
		doc->yoffset = 0;
	else if (y >= 0 && abs_y > max_y - doc->yoffset)
		doc->yoffset = max_y;
	else
		doc->yoffset += y;
	
	return;
}

void	Green_Zoom( Green_Document *doc, int width, int height, double new_fs )
{
	PopplerPage	*page;
	double	old_tscale, new_tscale;
	int	old_w, old_h, new_w, new_h;
	
	page = poppler_document_get_page( doc->doc, doc->page_cur );
	old_tscale = Green_Fit(doc, width, height) * doc->finescale;
	doc->finescale = new_fs;
	new_tscale = Green_Fit(doc, width, height) * new_fs;
	Green_GetDimension( page, &old_w, &old_h, old_tscale );
	Green_GetDimension( page, &new_w, &new_h, new_tscale );
	g_object_unref( G_OBJECT( page ) );
	
	if (new_w < width)
		doc->xoffset = 0;
	else if (old_w < width)
		doc->xoffset = doc->xoffset * new_tscale / old_tscale;
	else
		doc->xoffset = doc->xoffset * new_tscale / old_tscale
			+ (new_tscale - old_tscale) * width / 2 / new_tscale;
	
	if (new_h < height )
		doc->yoffset = 0;
	else if (old_h < height)
		doc->yoffset = doc->yoffset * new_tscale / old_tscale;
	else
		doc->yoffset = doc->yoffset * new_tscale / old_tscale
			+ (new_tscale - old_tscale) * height / 2 / new_tscale;
	
	return;
}	

int	Green_FindNext( Green_Document *doc, int start )
{
	PopplerPage	*page;
	GList	*list;
	int	i, res = -1;
	
	for (i = 0; i < doc->page_count; i++)
	{
		page = poppler_document_get_page( doc->doc, (start + i) % doc->page_count );
		list = poppler_page_find_text( page, doc->search_str );
		g_object_unref( G_OBJECT( page ) );
		if (list)
		{
			g_list_free( list );
			res = (start + i) % doc->page_count;
			break;
		}
	}
	
	return res;
}

void	PrintHelp()
{
	printf( "Help is currently only available online:\n\thttp://github.com/schandinat/green\n" );
	return;
}

void	PrintVersion()
{
	printf( "green - the PDF reader\nversion: %s\nlicense: GNU GPL version 3\n", "0.1" );
	return;
}

double	ReadFraction( char *str )
{
	double	res;
	long	tmp;
	char	*str2;
	
	res = strtol( str, &str2, 10 );
	if (!*str2)
		return res;
	
	if (*str2 != '/')
		return 0;
	
	str2++;
	tmp = strtol( str2, &str2, 10 );
	if (!tmp || *str2)
		return 0;
	
	res /= tmp;
	return res;
}

int	main( int argc, char *argv[] )
{
	Green_RTD	rtd;
	char	*opt;
	int i, err = 0;
	
	rtd.flags = 0;
	rtd.width = 0;
	rtd.height = 0;	
	rtd.docs = NULL;
	rtd.doc_count = 0;
	rtd.doc_cur = 0;
	rtd.c_background = 0x30D030;
	rtd.c_highlight = 0x8080FF80;
	rtd.fit_method = NATURAL;
	rtd.step = 1;
	rtd.zoomstep = 1.1;
	rtd.bb = 0x04;
	rtd.mouse.flags = 1;
	rtd.mouse.visibility = 5;
	g_type_init();
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] != '-')
			continue;
		
		opt = argv[i][1] == '-' ? &argv[i][2] : &argv[i][1];
		if (!strcmp( opt, "h" ) || !strcmp( opt, "help" ))
		{
			PrintHelp();
			return 0;
		}
		else if (!strcmp( opt, "v" ) || !strcmp( opt, "version" ))
		{
			PrintVersion();
			return 0;
		}
		else if (!strncmp( opt, "fit=", 4 ))
		{
			opt += 4;
			if (!strcmp( opt, "width" ))
				rtd.fit_method = WIDTH;
			else if (!strcmp( opt, "height" ))
				rtd.fit_method = HEIGHT;
			else if (!strcmp( opt, "page" ))
				rtd.fit_method = PAGE;
			else if (!strcmp( opt, "none" ))
				rtd.fit_method = NATURAL;
			else
				err = -1;
		}
		else if (!strncmp( opt, "step=", 5 ))
		{
			opt += 5;
			rtd.step = ReadFraction( opt );
			if (rtd.step <= 0 || rtd.step > 1)
				err = -1;
		}
		else if (!strncmp( opt, "zoomstep=", 9 ))
		{
			opt += 9;
			rtd.zoomstep = ReadFraction( opt );
			if (rtd.zoomstep <= 0)
				err = -1;
			
			rtd.zoomstep += 1;
		}
		else if (!strncmp( opt, "width=", 6 ))
		{
			opt += 6;
			rtd.width = strtol( opt, &opt, 10 );
			if (*opt)
				err = -1;
		}
		else if (!strncmp( opt, "height=", 7 ))
		{
			opt += 7;
			rtd.height = strtol( opt, &opt, 10 );
			if (*opt)
				err = -1;
		}
		else if (!strcmp( opt, "fullscreen" ))
			rtd.flags |= GREEN_FULLSCREEN;
		else
			err = -1;
		
		if (err)
		{
			fprintf( stderr, "Unknown option!\n" );
			PrintHelp();
			return err;
		}
	}
	
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '-')
			continue;
		
		opt = Green_FilenameToURI( argv[i] );
		if (Green_Open( &rtd, opt ) < 0)
		{
			fprintf( stderr, "Failed to open: %s\n", opt );
			free( opt );
			continue;
		}
	}
	
	return Green_SDL_Main( &rtd );
}
