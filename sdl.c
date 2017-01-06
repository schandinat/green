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

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <linux/tty.h>
#include <linux/kd.h>
#include <SDL.h>
#include "green.h"


#define FLAG_QUIT	0x0001
#define FLAG_RENDER	0x0002


typedef enum
{
	NORMAL, GOTO, SEARCH, FIT, ROTATE, MIRROR
	
}	RState;

typedef struct
{
	char	buff[64];
	unsigned char	used, cur;
	
}	IBuffer;


const Uint32	live_interval = 40;


void	GetInput( IBuffer *input, SDL_Event *event )
{
	char	c;
	int i;
	
	switch (event->key.keysym.sym)
	{
		case SDLK_LEFT:
			if (input->cur)
				input->cur--;
			
			break;
		case SDLK_RIGHT:
			if (input->cur < input->used)
				input->cur++;
			
			break;
		case SDLK_BACKSPACE:
			if (!input->cur)
				break;
			
			input->cur--;
		case SDLK_DELETE:
			if (input->cur == input->used)
				break;
			
			for (i = input->cur; i < input->used-1; i++)
				input->buff[i] = input->buff[i+1];
			
			input->used--;
			break;
		case 'a'...'z':
			if (event->key.keysym.mod & KMOD_SHIFT)
				event->key.keysym.sym += 'A' - 'a';
		case '0'...'9':
		case '+':
		case '-':
		case '*':
		case '/':
		case '=':
		case '_':
		case '.':
		case ':':
		case ',':
		case ';':
		case '!':
		case '?':
		case '(':
		case ')':
		case '@':
		case ' ':
		case '$':
		case '&':
		case '#':
		case '\\':
			if (input->used == sizeof( input->buff ) - 1)
				break;
			
			for (i = input->cur; i < input->used; i++)
				input->buff[i+1] = input->buff[i];
			
			c = event->key.keysym.sym;
			input->buff[input->cur] = c;
			input->cur++;
			input->used++;
			break;
		default:
			break;
	}
	
	return;
}

void	CopyPixel( Uint32 *dst, Uint8 red, Uint8 green, Uint8 blue, SDL_PixelFormat *fmt )
{
	Uint32 tdst;

	tdst = SDL_MapRGB(fmt, red, green, blue);

	switch(fmt->BitsPerPixel) {
	case 8:
		* (Uint8 *) dst = tdst & 0xFF;
		break;
	case 16:
		* (Uint16 *) dst = tdst & 0xFFFF;
		break;
	case 32:
		*dst = tdst;
		break;
	}
}

void	RenderPage( Green_RTD *rtd, SDL_Rect dest, int xoff, int yoff, PopplerPage *page, double tscale )
{
	PopplerRectangle	*rect;
	Green_Document	*doc = rtd->docs[rtd->doc_cur];
	SDL_Surface	*display = SDL_GetVideoSurface();
	SDL_PixelFormat	fmt = *display->format;
	cairo_surface_t	*surface;
	cairo_t		*context;
	void	*pixels;
	unsigned short	ar, ag, ab, ia;
	gdouble	tmp_d;
	double	pwidth, pheight;
	guint	i, n;
	GList	*list = NULL;
	Uint32	*src, *dst;
	int	red, green, blue;
	int	x, y, rowstride, w, h, dir_x, dir_y;

	if (doc->palettehack && fmt.palette && fmt.BitsPerPixel == 8) {
		/* palette hack; wrong in general, but fast;
		   works with the palette as initialized by SDL */
		fmt.Rloss = 5;
		fmt.Gloss = 5;
		fmt.Bloss = 6;
		fmt.Rshift = 5;
		fmt.Gshift = 2;
		fmt.Bshift = 0;
		fmt.palette = NULL;
	}

	if (doc->search_str)
		list = poppler_page_find_text( page, doc->search_str );

	if (doc->cache.surface && doc->cache.page == doc->page_cur && doc->cache.tscale == tscale && doc->cache.rotation == doc->rotation)
		surface = doc->cache.surface;
	else
	{
		Green_GetDimension( page, &w, &h, tscale, doc->pixelheight, false );
		if (doc->rotation % 2)
			h *= doc->pixelheight + 1;
		surface = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, w, h);
		context = cairo_create( surface );
		cairo_save( context );
		if (doc->rotation % 2)
			cairo_scale( context, tscale / doc->pixelheight, tscale );
		else
			cairo_scale( context, tscale, tscale / doc->pixelheight );
		poppler_page_render( page, context );
		cairo_restore( context );
		cairo_set_operator( context, CAIRO_OPERATOR_DEST_OVER );
		cairo_set_source_rgb( context, 1., 1., 1. );
		cairo_paint( context );
		cairo_destroy( context );
		if (doc->cache.surface)
			cairo_surface_destroy( doc->cache.surface );

		doc->cache.surface = surface;
		doc->cache.page = doc->page_cur;
		doc->cache.tscale = tscale;
	}

	if (doc->rotation == 1)
	{
		dir_x = -1;
		dir_y = 1;
	}
	else if (doc->rotation == 2)
	{
		dir_x = -1;
		dir_y = -1;
	}
	else if (doc->rotation == 3)
	{
		dir_x = 1;
		dir_y = -1;
	}
	else
		dir_x = dir_y = 1;

	if (doc->mirrored)
		dir_y *= -1;

	pixels = cairo_image_surface_get_data( surface );
	rowstride = cairo_image_surface_get_stride( surface );
	SDL_LockSurface( display );
	if (doc->rotation % 2)
	{
		for (y = 0; y < dest.h; y++)
		{
			src = pixels + (xoff + dir_y * y + (dir_y < 0 ? dest.h - 1 : 0)) * 4 + (yoff + (dir_x < 0 ? dest.w - 1 : 0)) * rowstride;
			dst = display->pixels + (dest.y + y) * display->pitch
				+ dest.x * fmt.BytesPerPixel;
			for (x = 0; x < dest.w; x++)
			{
				red = (*src>>16)&0xFF;
				green = (*src>>8)&0xFF;
				blue = *src&0xFF;

				CopyPixel(dst, red, green, blue, &fmt);

				src = (void*)src + dir_x * rowstride;
				dst = (void*)dst + fmt.BytesPerPixel;
			}
		}
	}
	else
	{
		for (y = 0; y < dest.h; y++)
		{
			src = pixels + (yoff + dir_y * y + (dir_y < 0 ? dest.h - 1 : 0)) * rowstride + (xoff + (dir_x < 0 ? dest.w - 1 : 0)) * 4;
			dst = display->pixels + (dest.y + y) * display->pitch
				+ dest.x * fmt.BytesPerPixel;
			for (x = 0; x < dest.w; x++)
			{
				red = (*src>>16)&0xFF;
				green = (*src>>8)&0xFF;
				blue = *src&0xFF;

				CopyPixel(dst, red, green, blue, &fmt);
				
				src += dir_x;
				dst = (void*)dst + fmt.BytesPerPixel;
			}
		}
	}
	
	if (list)
	{
		poppler_page_get_size( page, &pwidth, &pheight );
		ar = rtd->c_highlight.a * rtd->c_highlight.r;
		ag = rtd->c_highlight.a * rtd->c_highlight.g;
		ab = rtd->c_highlight.a * rtd->c_highlight.b;
		ia = 0xFF - rtd->c_highlight.a;
		n = g_list_length( list );
		for (i = 0; i < n; i++)
		{
			rect = g_list_nth_data( list, i );
			tmp_d = pheight - rect->y2;
			rect->y2 = pheight - rect->y1;
			rect->y1 = tmp_d;
			rect->x1 *= tscale;
			rect->y1 *= tscale;
			rect->x2 *= tscale;
			rect->y2 *= tscale;
			rect->x1 -= xoff * (doc->rotation%2?doc->pixelheight:1);
			rect->y1 -= yoff * (doc->rotation%2?1:doc->pixelheight);
			rect->x2 -= xoff * (doc->rotation%2?doc->pixelheight:1);
			rect->y2 -= yoff * (doc->rotation%2?1:doc->pixelheight);
			if (doc->rotation % 2)
			{
				tmp_d = rect->x1;
				rect->x1 = rect->y1;
				rect->y1 = tmp_d;
				tmp_d = rect->x2;
				rect->x2 = rect->y2;
				rect->y2 = tmp_d;
			}
			rect->y1 /= doc->pixelheight;
			rect->y2 /= doc->pixelheight;

			if (doc->mirrored)
			{
				tmp_d = rect->y1;
				rect->y1 = dest.h - rect->y2;
				rect->y2 = dest.h - tmp_d;
			}

			if (doc->rotation == 1)
			{
				tmp_d = rect->x1;
				rect->x1 = dest.w - rect->x2;
				rect->x2 = dest.w - tmp_d;
			}
			else if (doc->rotation == 2)
			{
				tmp_d = rect->x1;
				rect->x1 = dest.w - rect->x2;
				rect->x2 = dest.w - tmp_d;
				tmp_d = rect->y1;
				rect->y1 = dest.h - rect->y2;
				rect->y2 = dest.h - tmp_d;
			}
			else if (doc->rotation == 3)
			{
				tmp_d = rect->y1;
				rect->y1 = dest.h - rect->y2;
				rect->y2 = dest.h - tmp_d;
			}

			if (rect->x1 > dest.w)
				continue;
			else if (rect->x1 < 0)
				rect->x1 = 0;
			
			if (rect->x2 < 0)
				continue;
			else if (rect->x2 > dest.w)
				rect->x2 = dest.w;
			
			if (rect->y1 > dest.h)
				continue;
			else if (rect->y1 < 0)
				rect->y1 = 0;
			
			if (rect->y2 < 0)
				continue;
			else if (rect->y2 > dest.h)
				rect->y2 = dest.h;
			
			if (doc->rotation % 2)
			{
				for (y = rect->y1; y < (int)rect->y2; y++)
				{
					src = pixels + (xoff + dir_y * y + (dir_y < 0 ? dest.h - 1 : 0)) * 4 + (yoff + dir_x * (int)rect->x1 + (dir_x < 0 ? dest.w - 1 : 0)) * rowstride;
					dst = display->pixels + (dest.y + y) * display->pitch
						+ (dest.x + (int)rect->x1) * fmt.BytesPerPixel;
					for (x = rect->x1; x < (int)rect->x2; x++)
					{
						red = (((*src>>16)&0xFF) * ia + ar) / 256;
						green = (((*src>>8)&0xFF) * ia + ag) / 256;
						blue = (((*src)&0xFF) * ia + ab) / 256;

						CopyPixel(dst, red, green, blue, &fmt);
						
						src = (void*)src + dir_x * rowstride;
						dst = (void*)dst + fmt.BytesPerPixel;
					}
				}
			}
			else
			{
				for (y = rect->y1; y < (int)rect->y2; y++)
				{
					src = pixels + (yoff + dir_y * y + (dir_y < 0 ? dest.h - 1 : 0)) * rowstride + (xoff + dir_x * (int)rect->x1 + (dir_x < 0 ? dest.w - 1 : 0)) * 4;
					dst = display->pixels + (dest.y + y) * display->pitch
						+ (dest.x + (int)rect->x1) * fmt.BytesPerPixel;
					for (x = rect->x1; x < (int)rect->x2; x++)
					{
						red = (((*src>>16)&0xFF) * ia + ar) / 256;
						green = (((*src>>8)&0xFF) * ia + ag) / 256;
						blue = (((*src)&0xFF) * ia + ab) / 256;

						CopyPixel(dst, red, green, blue, &fmt);

						src += dir_x;
						dst = (void*)dst + fmt.BytesPerPixel;
					}
				}
			}
		}
		
		g_list_free( list );
	}
	
	SDL_UnlockSurface( display );
	return;
}

void	Render( Green_RTD *rtd )
{
	Green_Document	*doc;
	PopplerPage	*page = NULL;
	SDL_Surface	*display = SDL_GetVideoSurface();
	SDL_Rect	rect;
	double	tscale;
	int	w, h;
	
	rect.x = rect.y = 0;
	rect.w = display->w;
	rect.h = display->h;
	SDL_FillRect( display, &rect, SDL_MapRGB( display->format, rtd->c_background.r, rtd->c_background.g, rtd->c_background.b ));
	if (!Green_IsDocValid( rtd, rtd->doc_cur ))
	{
		SDL_UpdateRect( display, 0, 0, 0, 0 );
		return;
	}
	
	doc = rtd->docs[rtd->doc_cur];
	tscale = Green_Fit( doc, display->w, display->h ) * doc->finescale;
	page = poppler_document_get_page( doc->doc, doc->page_cur );
	Green_GetDimension( page, &w, &h, tscale, doc->pixelheight, doc->rotation % 2 );
	rect.w = w > display->w ? display->w : w;
	rect.h = h > display->h ? display->h : h;
	rect.x = (display->w - rect.w) / 2;
	rect.y = (display->h - rect.h) / 2;
	RenderPage( rtd, rect, doc->xoffset, doc->yoffset, page, tscale );
	g_object_unref( G_OBJECT( page ) );
	SDL_UpdateRect( display, 0, 0, 0, 0 );
	return;
}

/*
 * switch from active console to next/previous (if increase=1/-1)
 * wait for coming back, so that we can re-render
 */

int inc_console(int increase) {
	int tty;
	int res;
	struct vt_stat st;
	int current;
	int i, step;

	// printf("inc_console(%d)\n", increase);

	tty = open("/dev/tty", O_RDONLY);
	if (tty == -1) {
		perror("/dev/tty");
		return -1;
	}

			/* find map and the currently active vt */

	res = ioctl(tty, VT_GETSTATE, &st);
	if (res) {
		perror("ioctl");
		return -1;
	}

			/* find next used vt */

	current = st.v_active;
	step = increase > 0 ? 1 : -1;
	for(i = current + increase; i != current; i += step) {

			/* rotation of consoles */

		if (i > MAX_NR_CONSOLES)
			i = 1;
		if (i < 1)
			i = MAX_NR_CONSOLES;

			/* map only covers the first 15 consoles */

		if (i < 8 * sizeof(st.v_state)) {
			if ((st.v_state >> i) & 0x01) {
				// printf("switching to %d\n", i);
				ioctl(tty, KDSETMODE, 0);
				res = ioctl(tty, VT_ACTIVATE, i);
				if (res == 0)
					ioctl(tty, VT_WAITACTIVE, i);
				ioctl(tty, VT_WAITACTIVE, current);
				ioctl(tty, KDSETMODE, 1);
				break;
			}
		}

			/* trick to check if the other consoles are used */

		else {
			if (ioctl(tty, VT_DISALLOCATE, i)) {
				// printf("switching to %d\n", i);
				ioctl(tty, KDSETMODE, 0);
				res = ioctl(tty, VT_ACTIVATE, i);
				if (res == 0)
					ioctl(tty, VT_WAITACTIVE, i);
				ioctl(tty, VT_WAITACTIVE, current);
				ioctl(tty, KDSETMODE, 1);
				break;
			}
		}
	}

	close(tty);
	return 0;
}

RState	NormalInput( Green_RTD *rtd, SDL_Event *event, unsigned short *flags )
{
	Green_Document	*doc = NULL;
	SDL_Surface	*display = SDL_GetVideoSurface();
	RState	state = NORMAL;
	int	f = 0;
	
	if (Green_IsDocValid( rtd, rtd->doc_cur ))
		doc = rtd->docs[rtd->doc_cur];
	
	switch (event->key.keysym.sym)
	{
		case 'q':
			*flags |= FLAG_QUIT;
			break;
		case 'c':
			Green_Close( rtd, rtd->doc_cur );
			*flags |= FLAG_RENDER;
			break;
		case 'g':
			state = GOTO;
			break;
		case SDLK_s:
			/* type s-SEARCHSTRING-<Enter> to search string */
			state = SEARCH;
			break;
		case 'n':
			if (!doc || !doc->search_str)
				break;
			
			doc->page_cur = Green_FindNext( doc, doc->page_cur + 1 );
			*flags |= FLAG_RENDER;
			break;
		case 'f':
			state = FIT;
			break;
		case SDLK_UP:
			if (!doc)
				break;
			
			Green_ScrollRelative( doc, 0, - display->h * rtd->step, display->w, display->h, 1 );
			*flags |= FLAG_RENDER;
			break;
		case SDLK_k:
			if (!doc)
				break;
			Green_ScrollRelative( doc, 0, - display->h * rtd->step, display->w, display->h, 1 );
			*flags |= FLAG_RENDER;
			break;
		case SDLK_DOWN:
			if (!doc)
				break;
			
			Green_ScrollRelative( doc, 0, display->h * rtd->step, display->w, display->h, 1 );
			*flags |= FLAG_RENDER;
			break;
                case SDLK_j:
			if (!doc)
				break;
			
			Green_ScrollRelative( doc, 0, display->h * rtd->step, display->w, display->h, 1 );
			*flags |= FLAG_RENDER;
			break;
		case SDLK_LEFT:
			if ((rtd->flags & GREEN_ALTVT) &&
			    (event->key.keysym.mod == KMOD_LALT ||
			     event->key.keysym.mod == KMOD_RALT)) {
				inc_console(-1);
				*flags |= FLAG_RENDER;
				break;
			}
			if (!doc)
				break;
			
			Green_ScrollRelative( doc, - display->w * rtd->step, 0, display->w, display->h, 1 );
			*flags |= FLAG_RENDER;
			break;
		case SDLK_h:
			if (!doc)
				break;
			
			Green_ScrollRelative( doc, - display->w * rtd->step, 0, display->w, display->h, 1 );
			*flags |= FLAG_RENDER;
			break;
		case SDLK_l:
			if (!doc)
				break;
			
			Green_ScrollRelative( doc, display->w * rtd->step, 0, display->w, display->h, 1 );
			*flags |= FLAG_RENDER;
			break;
 		case SDLK_RIGHT:
			if ((rtd->flags & GREEN_ALTVT) &&
			    (event->key.keysym.mod == KMOD_LALT ||
			     event->key.keysym.mod == KMOD_RALT)) {
				inc_console(1);
				*flags |= FLAG_RENDER;
				break;
			}
			if (!doc)
				break;
			
			Green_ScrollRelative( doc, display->w * rtd->step, 0, display->w, display->h, 1 );
			*flags |= FLAG_RENDER;
			break;
		case SDLK_PAGEUP:
			if (!doc || !Green_GotoPage( doc, doc->page_cur - 1, true ))
				break;
			
			*flags |= FLAG_RENDER;
			break;
		case SDLK_PAGEDOWN:
			if (!doc || !Green_GotoPage( doc, doc->page_cur + 1, true ))
				break;
			
			*flags |= FLAG_RENDER;
			break;
		case SDLK_m:
			if (!doc)
				break;
			
			state = MIRROR;
			break;
		case SDLK_r:
			if (!doc)
				break;
			
			state = ROTATE;
			break;
		case '+':
			if (!doc)
				break;
			
			Green_Zoom( doc, display->w,display->h, doc->finescale * rtd->zoomstep );
			*flags |= FLAG_RENDER;
			break;
		case '-':
			if (!doc)
				break;
			
			Green_Zoom( doc, display->w,display->h, doc->finescale / rtd->zoomstep );
			*flags |= FLAG_RENDER;
			break;
		case SDLK_F12:
			f++;
		case SDLK_F11:
			f++;
		case SDLK_F10:
			f++;
		case SDLK_F9:
			f++;
		case SDLK_F8:
			f++;
		case SDLK_F7:
			f++;
		case SDLK_F6:
			f++;
		case SDLK_F5:
			f++;
		case SDLK_F4:
			f++;
		case SDLK_F3:
			f++;
		case SDLK_F2:
			f++;
		case SDLK_F1:
			if (!Green_IsDocValid( rtd, f ))
				break;
			
			rtd->doc_cur = f;
			*flags |= FLAG_RENDER;
			break;
		case SDLK_TAB:
			Green_NextVaildDoc( rtd );
			*flags |= FLAG_RENDER;
			break;
		default:
			break;
	}
	
	return state;
}

Uint32	live_timer( Uint32 interval, void *param )
{
	SDL_Event	event;
	
	event.type = SDL_USEREVENT;
	SDL_PushEvent( &event );
	return interval;
}

int	Green_SDL_Main( Green_RTD *rtd )
{
	SDL_TimerID	timer = NULL;
	SDL_Surface	*display;
	SDL_Event	event;
	RState	state = NORMAL;
	IBuffer	input;
	Uint32	mouse_last = 0, mouse_cur;
	Uint16	left_x = 0, left_y = 0, right_x = 0, right_y = 0;
	unsigned short	flags = FLAG_RENDER;
	unsigned char	event_count;
	char	*str;
	long	tmp;
	int	x, y, width, height;
	
	if (!(rtd->mouse.flags & 0x01))
		setenv("SDL_NOMOUSE", "1", 1);

	if (SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER ))
	{
		fprintf( stderr, "SDL_Init failed: %s\n", SDL_GetError() );
		if (!strcmp(SDL_GetError(), "Unable to open mouse")) {
			printf("run with -nomouse or ");
			printf("add Mouse=0 to conf file\n");
		}
		return 1;
	}
	
	SDL_WM_SetCaption( "green - the PDF reader", NULL );
	display = SDL_SetVideoMode( rtd->width, rtd->height, 0, SDL_SWSURFACE | SDL_ANYFORMAT | SDL_RESIZABLE | (rtd->flags&GREEN_FULLSCREEN ? SDL_FULLSCREEN : 0) );
	if (!display)
	{
		SDL_Quit();
		fprintf( stderr, "SDL_SetVideoMode failed: %s\n", SDL_GetError() );
		return 2;
	}
	
	timer = SDL_AddTimer( live_interval, live_timer, NULL );
	mouse_last = SDL_GetTicks();
	if (!rtd->mouse.visibility || !(rtd->mouse.flags & 0x01))
 		SDL_ShowCursor( SDL_DISABLE );
	
	do
	{
		if (flags&FLAG_RENDER)
		{
			Render( rtd );
			flags ^= FLAG_RENDER;
		}
		
		event_count = 0;
		if (!SDL_WaitEvent( &event ))
		{
			SDL_Quit();
			return -1;
		}
		
		do
		{
			switch (event.type)
			{
				case SDL_QUIT:
					flags |= FLAG_QUIT;
					break;
				case SDL_KEYDOWN:
					if (event.key.keysym.sym == SDLK_ESCAPE)
						state = NORMAL;
					else if (event.key.keysym.sym == SDLK_RETURN)
					{
						if (!Green_IsDocValid( rtd, rtd->doc_cur ))
							break;
						
						input.buff[input.used] = 0;
						switch (state)
						{
							case GOTO:
								tmp = strtol( input.buff, &str, 10 );
								if (*str || tmp <=0 || tmp > rtd->docs[rtd->doc_cur]->page_count )
									break;
								
								rtd->docs[rtd->doc_cur]->page_cur = tmp - 1;
								rtd->docs[rtd->doc_cur]->xoffset = 0;
								rtd->docs[rtd->doc_cur]->yoffset = 0;
								flags |= FLAG_RENDER;
								break;
							case SEARCH:
								free( rtd->docs[rtd->doc_cur]->search_str );
								rtd->docs[rtd->doc_cur]->search_str = NULL;
								if (!strlen( input.buff ))
									break;
								
								rtd->docs[rtd->doc_cur]->search_str = strdup( input.buff );
								tmp = Green_FindNext( rtd->docs[rtd->doc_cur], rtd->docs[rtd->doc_cur]->page_cur );
								if (tmp < 0)
								{
									free( rtd->docs[rtd->doc_cur]->search_str );
									rtd->docs[rtd->doc_cur]->search_str = NULL;
									break;
								}
								
								rtd->docs[rtd->doc_cur]->page_cur = tmp;
								flags |= FLAG_RENDER;
								break;
							default:
								break;
						}
						
						state = NORMAL;
					}
					else if (state == NORMAL)
					{
						state = NormalInput( rtd, &event, &flags );
						if (state != NORMAL)
						{
							input.used = 0;
							input.cur = 0;
						}
					}
					else if (state == GOTO || state == SEARCH)
						GetInput( &input, &event );
					else if (state == FIT)
					{
						state = NORMAL;
						if (!Green_IsDocValid( rtd, rtd->doc_cur ))
							break;
						
						if (event.key.keysym.sym == 'n')
							rtd->docs[rtd->doc_cur]->fit_method = NATURAL;
						else if (event.key.keysym.sym == 'w')
							rtd->docs[rtd->doc_cur]->fit_method = WIDTH;
						else if (event.key.keysym.sym == 'h')
							rtd->docs[rtd->doc_cur]->fit_method = HEIGHT;
						else if (event.key.keysym.sym == 'p')
							rtd->docs[rtd->doc_cur]->fit_method = PAGE;
						
						if (event.key.keysym.sym == 'n'
							|| event.key.keysym.sym == 'w'
							|| event.key.keysym.sym == 'h'
							|| event.key.keysym.sym == 'p')
						{
							rtd->docs[rtd->doc_cur]->finescale = 1;
							rtd->docs[rtd->doc_cur]->xoffset = 0;
							rtd->docs[rtd->doc_cur]->yoffset = 0;
							flags |= FLAG_RENDER;
						}
					}
					else if (state == MIRROR)
					{
						state = NORMAL;
						if (!Green_IsDocValid( rtd, rtd->doc_cur ))
							break;
						
						if (event.key.keysym.sym == 'h')
						{
							Green_MirrorH( rtd->docs[rtd->doc_cur] );
							flags |= FLAG_RENDER;
						}
						else if (event.key.keysym.sym == 'v')
						{
							Green_MirrorV( rtd->docs[rtd->doc_cur] );
							flags |= FLAG_RENDER;
						}
					}
					else if (state == ROTATE)
					{
						state = NORMAL;
						if (!Green_IsDocValid( rtd, rtd->doc_cur ))
							break;
						
						if (event.key.keysym.sym == 'l')
						{
							Green_RotateLeft( rtd->docs[rtd->doc_cur] );
							Green_ValidateOffset( rtd->docs[rtd->doc_cur], display->w, display->h );
							flags |= FLAG_RENDER;
						}
						else if (event.key.keysym.sym == 'r')
						{
							Green_RotateRight( rtd->docs[rtd->doc_cur] );
							Green_ValidateOffset( rtd->docs[rtd->doc_cur], display->w, display->h );
							flags |= FLAG_RENDER;
						}
					}
					
					break;
				case SDL_VIDEORESIZE:
					display = SDL_SetVideoMode( event.resize.w, event.resize.h, 0, SDL_HWSURFACE | SDL_ANYFORMAT | SDL_RESIZABLE );
					if (!display)
					{
						SDL_Quit();
						fprintf( stderr, "SDL_SetVideoMode failed: %s\n", SDL_GetError() );
						return -5;
					}
					
					if (Green_IsDocValid( rtd, rtd->doc_cur ))
						Green_ValidateOffset( rtd->docs[rtd->doc_cur], display->w, display->h );
					
					flags |= FLAG_RENDER;
					break;
				case SDL_MOUSEMOTION:
					if (rtd->mouse.visibility > 0)
					{
						mouse_last = SDL_GetTicks();
						SDL_ShowCursor( SDL_ENABLE );
					}
					
					break;
				case SDL_MOUSEBUTTONDOWN:
					if (rtd->mouse.visibility > 0)
					{
						mouse_last = SDL_GetTicks();
						SDL_ShowCursor( SDL_ENABLE );
					}
					
					if (!(rtd->mouse.flags&0x01) || !Green_IsDocValid( rtd, rtd->doc_cur ))
						break;
					
					switch (event.button.button)
					{
						case SDL_BUTTON_LEFT:
							left_x = event.button.x;
							left_y = event.button.y;
							break;
						case SDL_BUTTON_RIGHT:
							right_x = event.button.x;
							right_y = event.button.y;
							break;
						case SDL_BUTTON_WHEELDOWN:
							Green_Zoom( rtd->docs[rtd->doc_cur], display->w, display->h, rtd->docs[rtd->doc_cur]->finescale * rtd->zoomstep );
							Render( rtd );
							break;
						case SDL_BUTTON_WHEELUP:
							Green_Zoom( rtd->docs[rtd->doc_cur], display->w, display->h, rtd->docs[rtd->doc_cur]->finescale / rtd->zoomstep );
							Render( rtd );
							break;
					}
					
					break;
				case SDL_MOUSEBUTTONUP:
					if (rtd->mouse.visibility)
					{
						mouse_last = SDL_GetTicks();
						SDL_ShowCursor( SDL_ENABLE );
					}
					
					if (!(rtd->mouse.flags&0x01) || !Green_IsDocValid( rtd, rtd->doc_cur ))
						break;
					
					if (event.button.button == SDL_BUTTON_RIGHT)
					{
						Green_ScrollRelative( rtd->docs[rtd->doc_cur], right_x - event.button.x, right_y - event.button.y, display->w, display->h, 1 );
						flags |= FLAG_RENDER;
					}
					
					break;
				case SDL_USEREVENT:
					if (rtd->mouse.visibility > 0)
					{
						mouse_cur = SDL_GetTicks();
						if ((Uint32)(mouse_cur - mouse_last) > rtd->mouse.visibility)
							SDL_ShowCursor( SDL_DISABLE );
					}
					
					SDL_GetMouseState( &x, &y );
					if (rtd->mouse.border_size && Green_IsDocValid( rtd, rtd->doc_cur ) && x >= 0 && y >= 0 && x <= display->w && y <= display->h)
					{
						width = display->w * rtd->mouse.border_size / 100;
						height = display->h * rtd->mouse.border_size / 100;
						
						if (x < width)
							width = -((width - x) * display->w / width * rtd->mouse.border_speed * live_interval / 1000);
						else if (x > display->w - width)
							width = (x + width - display->w) * display->w / width * rtd->mouse.border_speed * live_interval / 1000;
						else
							width = 0;
						
						if (y < height)
							height = -((height - y) * display->h / height * rtd->mouse.border_speed * live_interval / 1000);
						else if (y > display->h - height)
							height = (y + height - display->h) * display->h / height * rtd->mouse.border_speed * live_interval / 1000;
						else
							height = 0;
						
						if (width || height)
						{
							Green_ScrollRelative( rtd->docs[rtd->doc_cur], width, height, display->w, display->h, 0 );
							flags |= FLAG_RENDER;
						}
					}
					
					break;
			}
			
			event_count++;
			
		}	while (event_count && SDL_PollEvent( &event ));
		
	}	while (!(flags&FLAG_QUIT));
	
	SDL_RemoveTimer( timer );
	SDL_Quit();
	return 0;
}
