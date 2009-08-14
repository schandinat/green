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
		
		if (Green_Open( &rtd, argv[i] ) < 0)
		{
			fprintf( stderr, "Failed to open: %s\n", argv[i] );
			continue;
		}
	}
	
	return Green_SDL_Main( &rtd );
}
