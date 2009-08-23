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
#include "SDL.h"
#include "green.h"


#define FLAG_QUIT	0x0001
#define FLAG_RENDER	0x0002


typedef enum
{
	NORMAL, GOTO, SEARCH, FIT
	
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
		case '{':
		case '}':
		case '@':
		case ' ':
		case '$':
		case '§':
		case '%':
		case '&':
		case '#':
		case '~':
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

void	RenderPage( Green_RTD *rtd, SDL_Rect dest, int xoff, int yoff, PopplerPage *page, double tscale )
{
	PopplerRectangle	*rect;
	Green_Document	*doc = rtd->docs[rtd->doc_cur];
	SDL_Surface	*display = SDL_GetVideoSurface();
	SDL_PixelFormat	fmt = *display->format;
	GdkPixbuf	*pb = gdk_pixbuf_new( GDK_COLORSPACE_RGB, 0, 8, dest.w, dest.h );
	unsigned char	*pixels, *src;
	unsigned short	ar, ag, ab, ia;
	gdouble	tmp_d;
	double	pwidth, pheight;
	guint	i, n;
	GList	*list = NULL;
	Uint32	*dst;
	int	x, y, rowstride, channels;
	
	if (!pb)
		return;
	
	if (doc->search_str)
		list = poppler_page_find_text( page, doc->search_str );
	
	poppler_page_render_to_pixbuf( page, xoff, yoff, 0, 0, tscale, 0, pb );
	pixels = gdk_pixbuf_get_pixels( pb );
	rowstride = gdk_pixbuf_get_rowstride( pb );
	channels = gdk_pixbuf_get_n_channels( pb );
	SDL_LockSurface( display );
	for (y = 0; y < dest.h; y++)
	{
		src = pixels + y * rowstride;
		dst = display->pixels + (dest.y + y) * display->pitch
			+ dest.x * fmt.BytesPerPixel;
		for (x = 0; x < dest.w; x++)
		{
			*dst = ((src[0]>>fmt.Rloss)<<fmt.Rshift)
				| ((src[1]>>fmt.Gloss)<<fmt.Gshift)
				| ((src[2]>>fmt.Bloss)<<fmt.Bshift);
			
			src += channels;
			dst = (void*)dst + fmt.BytesPerPixel;
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
			if (rect->x1 > xoff + dest.w)
				continue;
			else if (rect->x1 < xoff)
				rect->x1 = xoff;
			
			if (rect->x2 < xoff)
				continue;
			else if (rect->x2 > xoff + dest.w)
				rect->x2 = xoff + dest.w;
			
			if (rect->y1 > yoff + dest.h)
				continue;
			else if (rect->y1 < yoff)
				rect->y1 = yoff;
			
			if (rect->y2 < yoff)
				continue;
			else if (rect->y2 > yoff + dest.h)
				rect->y2 = yoff + dest.h;
			
			rect->x1 -= xoff;
			rect->x2 -= xoff;
			rect->y1 -= yoff;
			rect->y2 -= yoff;
			for (y = rect->y1; y < (int)rect->y2; y++)
			{
				src = pixels + y * rowstride + (int)rect->x1 * channels;
				dst = display->pixels + (dest.y + y) * display->pitch + (dest.x + (int)rect->x1) * fmt.BytesPerPixel;
				for (x = rect->x1; x <  (int)rect->x2; x++)
				{
					*dst = ((((src[0] * ia + ar) / 256)>>fmt.Rloss)<<fmt.Rshift)
						| ((((src[1] * ia + ag) / 256)>>fmt.Gloss)<<fmt.Gshift)
						| ((((src[2] * ia + ab) / 256)>>fmt.Bloss)<<fmt.Bshift);
					
					src += channels;
					dst = (void*)dst + fmt.BytesPerPixel;
				}
			}
		}
		
		g_list_free( list );
	}
	
	SDL_UnlockSurface( display );
	g_object_unref( G_OBJECT( pb ) );
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
	Green_GetDimension( page, &w, &h, tscale );
	rect.w = w > display->w ? display->w : w;
	rect.h = h > display->h ? display->h : h;
	rect.x = (display->w - rect.w) / 2;
	rect.y = (display->h - rect.h) / 2;
	RenderPage( rtd, rect, doc->xoffset, doc->yoffset, page, tscale );
	g_object_unref( G_OBJECT( page ) );
	SDL_UpdateRect( display, 0, 0, 0, 0 );
	return;
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
		case 's':
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
		case SDLK_DOWN:
			if (!doc)
				break;
			
			Green_ScrollRelative( doc, 0, display->h * rtd->step, display->w, display->h, 1 );
			*flags |= FLAG_RENDER;
			break;
		case SDLK_LEFT:
			if (!doc)
				break;
			
			Green_ScrollRelative( doc, - display->w * rtd->step, 0, display->w, display->h, 1 );
			*flags |= FLAG_RENDER;
			break;
		case SDLK_RIGHT:
			if (!doc)
				break;
			
			Green_ScrollRelative( doc, display->w * rtd->step, 0, display->w, display->h, 1 );
			*flags |= FLAG_RENDER;
			break;
		case SDLK_PAGEUP:
			if (!doc || !Green_GotoPage( doc, doc->page_cur - 1  ))
				break;
			
			*flags |= FLAG_RENDER;
			break;
		case SDLK_PAGEDOWN:
			if (!doc || !Green_GotoPage( doc, doc->page_cur + 1  ))
				break;
			
			*flags |= FLAG_RENDER;
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
	
	if (SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER ))
	{
		fprintf( stderr, "SDL_Init failed: %s\n", SDL_GetError() );
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
	
	if (display->format->palette)
	{
		SDL_Quit();
		fprintf( stderr, "Palettes are not supported!\n" );
                return 3;
	}
	
	timer = SDL_AddTimer( live_interval, live_timer, NULL );
	mouse_last = SDL_GetTicks();
	if (!rtd->mouse.visibility)
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
						Green_ScrollRelative( rtd->docs[rtd->doc_cur], 0, 0, display->w, display->h, 1 );
					
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
