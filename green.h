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

#ifndef __GREEN_H__
#define __GREEN_H__


#include <stdbool.h>
#include "glib/poppler.h"


#define GREEN_FULLSCREEN	0x0001
#define GREEN_ALTVT		0x0010


typedef enum
{
	NATURAL, WIDTH, HEIGHT, PAGE
	
}	Green_FitMethod;

typedef struct
{
	unsigned char	r, g, b, a;
	
}	Green_RGBA;

typedef struct
{
	int	page;
	double	tscale;
	int	rotation;
	cairo_surface_t	*surface;
	
}	Green_PageBuffer;

typedef struct
{
	PopplerDocument	*doc;
	char	*uri;
	int	page_count, page_cur,
		xoffset, yoffset;
	bool	mirrored;	// is the document mirrored horizontally?
	int	rotation;
		// 0: not rotated
		// 1: rotated right by 90°
		// 2: rotated right by 180°
		// 3: rotated right by 270°
	Green_FitMethod	fit_method;
	double	pixelheight;
	double	finescale;
	int	palettehack;
	char	*search_str;
	unsigned char	bb;
	Green_PageBuffer	cache;
	
}	Green_Document;

typedef struct
{
	unsigned short	flags, width, height;
	Green_Document	**docs;
	int	doc_count, doc_cur, doc_help, doc_last;
	Green_RGBA	c_background, c_highlight;
	Green_FitMethod	fit_method;
	double	step, zoomstep;
	double	pixelheight;
	int	palettehack;
	unsigned char	bb;
	
	struct
	{
		unsigned char	flags;
			//	0x01:	react on mouse events?
		int	visibility;
			// -1: always visible
			//  0: always invisible
			// >0: timeout in ms
		unsigned char	border_size;
		double	border_speed;
		
	}	mouse;
	
}	Green_RTD;


int	Green_Open( Green_RTD *rtd, char *uri );
void	Green_Close( Green_RTD *rtd, int id );
double	Green_Fit( Green_Document *doc, int width, int height );
void	Green_ScrollRelative( Green_Document *doc, int x, int y, int w, int h, int bb_flag );
void	Green_GetScrollRegion( Green_Document *doc, int w, int h, int *scroll_w, int *scroll_h );
void	Green_Zoom( Green_Document *doc, int width, int height, double new_fs );
int	Green_FindNext( Green_Document *doc, int start );


inline static
int	Green_IsDocValid( Green_RTD *rtd, int id )
{
	return id >= 0 && id < rtd->doc_count && rtd->docs[id];
}

inline static
void	Green_GetDimension( PopplerPage *page, int *w, int *h, double tscale, double pixelheight, bool rotated )
{
	double	pwidth, pheight;
	
	poppler_page_get_size( page, &pwidth, &pheight );
	if (rotated)
	{
		*w = pheight * tscale;
		*h = pwidth * tscale / pixelheight;
	}
	else
	{
		*w = pwidth * tscale;
		*h = pheight * tscale / pixelheight;
	}
	
	return;
}

inline static
void	Green_ValidateOffset( Green_Document *doc, int width, int height )
{
	int doc_max_x, doc_max_y;
	
	if (doc->rotation % 2)
		Green_GetScrollRegion( doc, width, height, &doc_max_y, &doc_max_x );
	else
		Green_GetScrollRegion( doc, width, height, &doc_max_x, &doc_max_y );
	
	if (doc->xoffset < 0)
		doc->xoffset = 0;
	else if (doc->xoffset > doc_max_x)
		doc->xoffset = doc_max_x;
	
	if (doc->yoffset < 0)
		doc->yoffset = 0;
	else if (doc->yoffset > doc_max_y)
		doc->yoffset = doc_max_y;
}

inline static
void	Green_NextVaildDoc( Green_RTD *rtd )
{
	int i;
	int num = rtd->doc_help == -1 ? rtd->doc_count : rtd->doc_count - 1;
	
	for (i = 1; i < num; i++)
		if (rtd->docs[(rtd->doc_cur+i)%num])
		{
			rtd->doc_cur = (rtd->doc_cur + i) % num;
			break;
		}
	
	return;
}

inline static
int	Green_GotoPage( Green_Document *doc, int page, bool set_offset )
{
	if (page < 0 || page >= doc->page_count)
		return 0;
	
	doc->page_cur = page;
	if (set_offset)
	{
		doc->xoffset = 0;
		doc->yoffset = 0;
	}
	
	return 1;
}

inline static
void	Green_RotateRight( Green_Document *doc )
{
	if (doc->mirrored)
		doc->rotation = (doc->rotation + 3) % 4;
	else
		doc->rotation = (doc->rotation + 1) % 4;
}

inline static
void	Green_RotateLeft( Green_Document *doc )
{
	if (doc->mirrored)
		doc->rotation = (doc->rotation + 1) % 4;
	else
		doc->rotation = (doc->rotation + 3) % 4;
}

inline static
void	Green_MirrorH( Green_Document *doc )
{
	doc->mirrored = !doc->mirrored;
}

inline static
void	Green_MirrorV( Green_Document *doc )
{
	doc->mirrored = !doc->mirrored;
	doc->rotation = (doc->rotation + 2) % 4;
}


#endif /* __GREEN_H__ */
