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

void	Green_GetScrollRegion( Green_Document *doc, int w, int h, int *scroll_w, int *scroll_h )
{
	PopplerPage	*page;
	
	page = poppler_document_get_page( doc->doc, doc->page_cur );
	Green_GetDimension( page, scroll_w, scroll_h, Green_Fit( doc, w, h ) * doc->finescale, doc->rotation % 2 );
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
	doc->mirrored = false;
	doc->rotation = 0;
	doc->fit_method = rtd->fit_method;
	doc->finescale = 1;
	doc->search_str = NULL;
	doc->bb = rtd->bb;
	doc->cache.page = -1;
	doc->cache.tscale = 0;
	doc->cache.surface = NULL;
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
	if (doc->rotation % 2)
		poppler_page_get_size( page, &pheight, &pwidth );
	else
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
	int	doc_dx, doc_dy, doc_max_x, doc_max_y,
		abs_x = x < 0 ? -x : x,
		abs_y = y < 0 ? -y : y,
		max_x, max_y;
	bool	left_border, right_border, top_border, bottom_border, bb_done = false;
		
	Green_GetScrollRegion( doc, w, h, &max_x, &max_y );
	if (doc->rotation % 2)
	{
		doc_max_x = max_y;
		doc_max_y = max_x;
	}
	else
	{
		doc_max_x = max_x;
		doc_max_y = max_y;
	}

	if (doc->rotation == 1)
	{
		doc_dx = doc->mirrored ? -y : y;
		doc_dy = -x;
		left_border = doc->yoffset == doc_max_y;
		right_border = doc->yoffset == 0;
		if (doc->mirrored)
		{
			top_border = doc->xoffset == doc_max_x;
			bottom_border = doc->xoffset == 0;
		}
		else
		{
			top_border = doc->xoffset == 0;
			bottom_border = doc->xoffset == doc_max_x;
		}
	}
	else if (doc->rotation == 2)
	{
		doc_dx = -x;
		doc_dy = doc->mirrored ? y : -y;
		left_border = doc->xoffset == doc_max_x;
		right_border = doc->xoffset == 0;
		if (doc->mirrored)
		{
			top_border = doc->yoffset == 0;
			bottom_border = doc->yoffset == doc_max_y;
		}
		else
		{
			top_border = doc->yoffset == doc_max_y;
			bottom_border = doc->yoffset == 0;
		}
	}
	else if (doc->rotation == 3)
	{
		doc_dx = doc->mirrored ? y : -y;
		doc_dy = x;
		left_border = doc->yoffset == 0;
		right_border = doc->yoffset == doc_max_y;
		if (doc->mirrored)
		{
			top_border = doc->xoffset == 0;
			bottom_border = doc->xoffset == doc_max_x;
		}
		else
		{
			top_border = doc->xoffset == doc_max_x;
			bottom_border = doc->xoffset == 0;
		}
	}
	else
	{
		doc_dx = x;
		doc_dy = doc->mirrored ? -y : y;
		left_border = doc->xoffset == 0;
		right_border = doc->xoffset == doc_max_x;
		if (doc->mirrored)
		{
			top_border = doc->yoffset == doc_max_y;
			bottom_border = doc->yoffset == 0;
		}
		else
		{
			top_border = doc->yoffset == 0;
			bottom_border = doc->yoffset == doc_max_y;
		}
	}
	
	if (bb_flag)
	{
		if (abs_x > abs_y && x > 0 && right_border)
		{
			if (!(doc->bb&0xF0) && (doc->bb&0x03))
			{
				bb_mode = doc->bb&0x03;
				if (bb_mode == 1)
					bb_done = Green_GotoPage( doc, doc->page_cur + 1, false );
				else if (bb_mode == 2)
					bb_done = Green_GotoPage( doc, doc->page_cur - 1, false );
				
				if (bb_done)
				{
					Green_GetScrollRegion( doc, w, h, &max_x, &max_y );
					if (doc->rotation == 1)
						doc->yoffset = 0;
					else if (doc->rotation == 2)
						doc->xoffset = max_x;
					else if (doc->rotation == 3)
						doc->yoffset = max_x;
					else
						doc->xoffset = 0;
					
					Green_ValidateOffset( doc, w, h );
				}
			}
			
			return;
		}
		else if (abs_x > abs_y && x < 0 && left_border)
		{
			if (!(doc->bb&0xF0) && (doc->bb&0x03))
			{
				bb_mode = doc->bb&0x03;
				if (bb_mode == 1)
					bb_done = Green_GotoPage( doc, doc->page_cur - 1, false );
				else if (bb_mode == 2)
					bb_done = Green_GotoPage( doc, doc->page_cur + 1, false );
				
				if (bb_done)
				{
					Green_GetScrollRegion( doc, w, h, &max_x, &max_y );
					if (doc->rotation == 1)
						doc->yoffset = max_x;
					else if (doc->rotation == 2)
						doc->xoffset = 0;
					else if (doc->rotation == 3)
						doc->yoffset = 0;
					else
						doc->xoffset = max_x;
					
					Green_ValidateOffset( doc, w, h );
				}

			}
			
			return;
		}
		else if (abs_x < abs_y && y > 0 && bottom_border)
		{
			if (!(doc->bb&0xF0) && (doc->bb&0x0C))
			{
				bb_mode = (doc->bb>>2)&0x03;
				if (bb_mode == 1)
					bb_done = Green_GotoPage( doc, doc->page_cur + 1, false );
				else if (bb_mode == 2)
					bb_done = Green_GotoPage( doc, doc->page_cur - 1, false );
				
				if (bb_done)
				{
					Green_GetScrollRegion( doc, w, h, &max_x, &max_y );
					if (doc->rotation == 1)
						doc->xoffset = doc->mirrored ? max_y : 0;
					else if (doc->rotation == 2)
						doc->yoffset = doc->mirrored ? 0 : max_y;
					else if (doc->rotation == 3)
						doc->xoffset = doc->mirrored ? 0 : max_y;
					else
						doc->yoffset = doc->mirrored ? max_y : 0;
					
					Green_ValidateOffset( doc, w, h );
				}
			}
			
			return;
		}
		else if (abs_x < abs_y && y < 0 && top_border)
		{
			if (!(doc->bb&0xF0) && (doc->bb&0x0C))
			{
				bb_mode = (doc->bb>>2)&0x03;
				if (bb_mode == 1)
					bb_done = Green_GotoPage( doc, doc->page_cur - 1, false );
				else if (bb_mode == 2)
					bb_done = Green_GotoPage( doc, doc->page_cur + 1, false );
				
				if (bb_done)
				{
					Green_GetScrollRegion( doc, w, h, &max_x, &max_y );
					if (doc->rotation == 1)
						doc->xoffset = doc->mirrored ? 0 : max_y;
					else if (doc->rotation == 2)
						doc->yoffset = doc->mirrored ? max_y : 0;
					else if (doc->rotation == 3)
						doc->xoffset = doc->mirrored ? max_y : 0;
					else
						doc->yoffset = doc->mirrored ? 0 : max_y;
					
					Green_ValidateOffset( doc, w, h );
				}
			}
			
			return;
		}
	}
	
	doc->xoffset += doc_dx;
	doc->yoffset += doc_dy;
	Green_ValidateOffset( doc, w, h );
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
	Green_GetDimension( page, &old_w, &old_h, old_tscale, doc->rotation % 2 );
	Green_GetDimension( page, &new_w, &new_h, new_tscale, doc->rotation % 2 );
	g_object_unref( G_OBJECT( page ) );
	
	if (doc->rotation % 2 == 0)
		doc->xoffset = doc->xoffset * new_tscale / old_tscale
			+ (new_tscale - old_tscale) * width / 2 / new_tscale;
	else
		doc->xoffset = doc->xoffset * new_tscale / old_tscale
			+ (new_tscale - old_tscale) * height / 2 / new_tscale;
	
	if (doc->rotation % 2 == 0)
		doc->yoffset = doc->yoffset * new_tscale / old_tscale
			+ (new_tscale - old_tscale) * height / 2 / new_tscale;
	else
		doc->yoffset = doc->yoffset * new_tscale / old_tscale
			+ (new_tscale - old_tscale) * width / 2 / new_tscale;
	
	Green_ValidateOffset( doc, width, height );
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
