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


char*	FilenameToURI( char *filename )
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
	int	i;
	
	if (!doc)
		return -1;
	
	if (!strncmp( uri, "file:", 5 ))
		doc->uri = strdup( uri );
	else
		doc->uri = FilenameToURI( uri );
	
	if (!doc->uri)
	{
		free( doc );
		return -1;
	}
	
	doc->doc = poppler_document_new_from_file( doc->uri, NULL, NULL );
	if (!doc->doc)
	{
		free( doc->uri );
		free( doc );
		return -2;
	}
	
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
		free( doc->uri );
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

void	Green_ScrollRelative( Green_Document *doc, int x, int y, int w, int h, int bb_flag )
{
	unsigned char	bb_mode;
	int	abs_x = x < 0 ? -x : x,
		abs_y = y < 0 ? -y : y,
		max_x, max_y;
	
	GetScrollRegion( doc, w, h, &max_x, &max_y );
	if (bb_flag)
	{
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
