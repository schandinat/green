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


#define	SCHEME_WIDTH		1
#define	SCHEME_HEIGHT		2
#define	SCHEME_FULLSCREEN	3


struct SchemeData
{
	char	*name, *data;
	unsigned int	data_size;
	unsigned char	processed;
};

struct SchemeArray
{
	struct SchemeData	**scheme;
	unsigned int	n;
};

struct SchemeProperty
{
	const char	*name;
	unsigned int	id;
	unsigned char	processed;
};


int	Green_SDL_Main( Green_RTD *rtd );


struct SchemeProperty	scheme_property[] =
{
	{"Width", SCHEME_WIDTH, 0},
	{"Height", SCHEME_HEIGHT, 0},
	{"Fullscreen", SCHEME_FULLSCREEN, 0}
};


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

char*	ScanIdentifier( char **ptr )
{
	char	*start = *ptr, *id;
	
	while ((**ptr >= 'a' && **ptr <= 'z') || (**ptr >= '0' && **ptr <= '9')
		|| **ptr == '_')
		(*ptr)++;
	
	if (*ptr == start)
		return NULL;
	
	id = malloc( *ptr - start + 1 );
	if (!id)
		return NULL;
	
	memcpy( id, start, *ptr - start );
	id[*ptr - start] = 0;
	return id;
}

int	EvalProperty( Green_RTD *rtd, int id, char *arg )
{
	char	*tmpc;
	long	tmpl;
	int	res = 0;
	
	switch (id)
	{
		case SCHEME_WIDTH:
			tmpl = strtol( arg, &tmpc, 10 );
			if (*tmpc || tmpl < 0)
				res = -1;
			else
				rtd->width = tmpl;
			
			break;
		case SCHEME_HEIGHT:
			tmpl = strtol( arg, &tmpc, 10 );
			if (*tmpc || tmpl < 0)
				res = -1;
			else
				rtd->height = tmpl;
			
			break;
		case SCHEME_FULLSCREEN:
			if (!strcasecmp( arg, "yes" ))
				rtd->flags |= GREEN_FULLSCREEN;
			else if (!strcasecmp( arg, "no" ))
				rtd->flags &= ~GREEN_FULLSCREEN;
			else
				res = -1;
			
			break;
	}
	
	return res;
}

int	ProcessSingleScheme( Green_RTD *rtd, struct SchemeArray *schemes, char *name )
{
	struct SchemeData	*data = NULL;
	char	*str, *tmp, *id, *arg;
	int	i, res = 0;
	
	for (i = 0; i < schemes->n; i++)
	{
		if (!strcmp( name, schemes->scheme[i]->name ))
		{
			data = schemes->scheme[i];
			break;
		}
	}
	
	if (!data)
	{
		fprintf( stderr, "Unknown SCHEME '%s'\n", name );
		return -1;
	}
	
	if (data->processed)
		return 0;
	
	data->processed = 1;
	if (!data->data || !data->data_size)
		return 0;
	
	str = data->data + data->data_size - 1;
	do
	{
		while (str > data->data && *(str - 1))
			str--;
		
		if (str[0] == '!')
		{
			tmp = str + 1;
			id = ScanIdentifier( &tmp );
			if (!id || *tmp)
			{
				fprintf( stderr, "Invalid execute indentifier in SCHEME '%s'\n", name );
				free( id );
				res = -1;
				break;
			}
			
			res = ProcessSingleScheme( rtd, schemes, id );
			free( id );
			continue;
		}
		else if ((tmp = strchr( str, '=' )))
		{
			arg = tmp + 1;
			while (tmp > str && *(tmp - 1) == ' ')
				tmp--;
			
			if (tmp == str)
			{
				fprintf( stderr, "Invalid empty property in SCHEME '%s'\n", name );
				res = -1;
				break;
			}
			
			id = malloc( tmp - str + 1 );
			if (!id)
			{
				fprintf( stderr, "Out of memory!\n" );
				res = -1;
				break;
			}
			
			memcpy( id, str, tmp - str );
			id[tmp-str] = 0;
			while (*arg == ' ')
				arg++;
			
			for (i = 0; i < sizeof( scheme_property ) / sizeof( scheme_property[0] ); i++)
			{
				if (!strcasecmp( id, scheme_property[i].name ))
				{
					if (scheme_property[i].processed)
						break;
					
					scheme_property[i].processed = 1;
					res = EvalProperty( rtd, scheme_property[i].id, arg );
					if (res)
						printf( "Invalid property '%s' in SCHEME '%s'\n", id, name );
					
					break;
				}
			}
			
			if (i == sizeof( scheme_property ) / sizeof( scheme_property[0] ))
				printf( "Ignoring unknown property '%s' with value '%s'\n", id, arg );
			
			free( id );
		}
		else
			printf( "Ignoring unknown construct '%s' in SCHEME '%s'\n", str, name );
		
	}	while ((str -= 2) > data->data && !res);
	
	return res;
}

int	ProcessSchemes( Green_RTD *rtd, struct SchemeArray *schemes, char *current )
{
	int	res = 0;
	char	*str, *id, *tmp;
	
	do
	{
		str = strrchr( current, ',' );
		if (str)
		{
			*str = 0;
			str++;
		}
		else
			str = current;
		
		tmp = str;
		id = ScanIdentifier( &tmp );
		if (!id || *tmp)
		{
			fprintf( stderr, "Invalid SCHEME indentifier '%s'\n", str );
			free( id );
			res = -1;
			break;
		}
		
		res = ProcessSingleScheme( rtd, schemes, id );
		free( id );
		if (res)
			break;
		
	}	while (str != current);
	
	return res;
}

int	ReadConfigLine( FILE *file, char **line )
{
	char	*tmp, *buff = malloc( 80 );
	int	c, comment = 0, used = 0, size = 80;
	
	if (!buff)
	{
		fprintf( stderr, "Out of memory!\n" );
		return -1;
	}
	
	while ((c = fgetc( file )) == ' ' || c == '\t' );
	do
	{
		if (c == '\n' || c == EOF)
			break;
		
		if (comment)
			continue;
		
		if (c == '\t')
			c = ' ';
		
		if (c == '#')
		{
			comment = 1;
			continue;
		}
		
		buff[used] = c;
		used++;
		if (used < size)
			continue;
		
		size += 80;
		tmp = realloc( buff, size );
		if (tmp)
			buff = tmp;
		else
		{
			free( buff );
			fprintf( stderr, "Out of memory!\n" );
			return -1;
		}
	}	while ((c = fgetc( file )) != EOF);
	
	while (used && buff[used-1] == ' ')
		used--;
	
	buff[used] = 0;
	*line = buff;
	return used || c != EOF ? 0 : 1;
}

int	ParseConfig( char *filename, struct SchemeArray *schemes, char **default_scheme )
{
	struct SchemeData	*scheme = NULL;
	FILE	*file = fopen( filename, "r" );
	char	*line, *tmp;
	void	*ptr;
	int	i, res = 0, state = 0, line_count = 1;
	
	if (!file)
		return 1;
	
	while (!res && !(res = ReadConfigLine( file, &line )))
	{
		line_count++;
		if (!*line)
		{
			free( line );
			continue;
		}
		
		tmp = line;
		switch (state)
		{
			case 0:
				strtok( line, " " );
				tmp = strtok( NULL, "" );
				while (*tmp == ' ')
					tmp++;
				
				if (!strcmp( line, "SCHEME" ))
				{
					if (!*tmp)
					{
						fprintf( stderr, "Missing argument to DEFAULT_SCHEME on line %d\n", line_count );
						res = -1;
						break;
					}
					
					scheme = malloc( sizeof( *scheme ) );
					if (!scheme)
					{
						fprintf( stderr, "Out of memory!\n" );
						res = -1;
						break;
					}				
					
					scheme->data = NULL;
					scheme->data_size = 0;
					scheme->processed = 0;
					scheme->name = ScanIdentifier( &tmp );
					if (!scheme->name)
					{
						fprintf( stderr, "Missing identifier to DEFAULT_SCHEME on line %d\n", line_count );
						free( scheme );
						res = -1;
						break;
					}
					
					if (*tmp)
					{
						fprintf( stderr, "Invalid extra argument to SCHEME on line %d\n", line_count );
						free( scheme->name );
						free( scheme );
						res = -1;
						break;
					}
					
					for (i = 0; i < schemes->n; i++)
					{
						if (!strcmp( scheme->name, schemes->scheme[i]->name ))
						{
							fprintf( stderr, "Duplicated SCHEME '%s' on line %d\n", scheme->name, line_count );
							free( scheme->name );
							free( scheme );
							res = -1;
							break;
						}
					}
					
					if (res)
						break;
					
					state = 1;
					break;
				}
				else if (!strcmp( line, "DEFAULT_SCHEME" ))
				{
					if (!*tmp)
					{
						fprintf( stderr, "Missing argument to DEFAULT_SCHEME on line %d\n", line_count );
						res = -1;
						break;
					}
					
					if (strchr( tmp, ' ' ))
					{
						fprintf( stderr, "Invalid extra argument to DEFAULT_SCHEME on line %d\n", line_count );
						res = -1;
						break;
					}
					
					if (*default_scheme)
					{
						fprintf( stderr, "Forbidden duplicated DEFAULT_SCHEME on line %d\n", line_count );
						res = -1;
						break;
					}
					
					*default_scheme = strdup( tmp );
					if (!*default_scheme)
					{
						fprintf( stderr, "Out of memory!\n" );
						res = -1;
						break;
					}
				}
				else
				{
					fprintf( stderr, "Unknown command '%s' on line %d\n", line, line_count );
					res = -1;
					break;
				}
				
				break;
			case 1:
				if (tmp[0] == '{')
				{
					tmp++;
					state = 2;
					
					while (*tmp == ' ')
						tmp++;
					
					if (*tmp)
						continue;
					else
						break;
				}
				else
				{
					fprintf( stderr, "Expected '{' but got '%c' during SCHEME on line %d\n", tmp[0], line_count );
					free( scheme->name );
					free( scheme );
					res = -1;
					break;
				}
			case 2:
				if (!strcmp( tmp, "}"))
				{
					ptr = realloc( schemes->scheme, (schemes->n + 1) * sizeof( scheme ) );
					if (!ptr)
					{
						fprintf( stderr, "Out of memory!\n" );
						free( scheme->data );
						free( scheme->name );
						free( scheme );
						res = -1;
						break;
					}
					
					schemes->scheme = ptr;
					schemes->scheme[schemes->n] = scheme;
					schemes->n++;
					state = 0;
					break;
				}
				else
				{
					ptr = realloc( scheme->data, scheme->data_size + strlen( tmp ) + 1 );
					if (!ptr)
					{
						fprintf( stderr, "Out of memory!\n" );
						free( scheme->data );
						free( scheme->name );
						free( scheme );
						res = -1;
						break;
					}
					
					memcpy( ptr + scheme->data_size, tmp, strlen( tmp ) + 1 );
					scheme->data = ptr;
					scheme->data_size += strlen( tmp ) + 1;
					break;
				}
		}
		
		free( line );
	}
	
	if (res > 0)
		res = 0;
	
	if (!res && state)
		res = -1;
	
	fclose( file );
	return res;
}

int	main( int argc, char *argv[] )
{
	Green_RTD	rtd;
	struct SchemeArray	schemes;
	char	*opt, *config_file = NULL, *default_scheme = NULL, *current_scheme = NULL;
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
	schemes.scheme = NULL;
	schemes.n = 0;
	g_type_init();
	
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] != '-')
			continue;
		
		opt = argv[i][1] == '-' ? &argv[i][2] : &argv[i][1];
		if (!strncmp( opt, "config=", 7 ))
		{
			opt += 7;
			
			if (config_file)
			{
				fprintf( stderr, "Multiple configuration files are forbidden!\n" );
				return -1;
			}
			
			config_file = opt;
		}
		else if (!strncmp( opt, "scheme=", 7 ))
		{
			opt += 7;
			
			if (current_scheme)
			{
				fprintf( stderr, "Multiple scheme statements are forbidden!\n" );
				return -1;
			}
			
			current_scheme = opt;
		}
	}
	
	if (config_file)
	{
		err = ParseConfig( config_file, &schemes, &default_scheme );
		if (err > 0)
		{
			fprintf( stderr, "Specified configuration file not found!\n" );
			return -1;
		}
	}
	else
	{
#ifdef	GREEN_USERCONFIG_FILE
		opt = getenv( "HOME" );
		if (opt && (config_file = malloc( strlen( opt ) + strlen( GREEN_USERCONFIG_FILE ) + 2 )))
		{
			sprintf( config_file, "%s/%s", opt, GREEN_USERCONFIG_FILE );
			err = ParseConfig( config_file, &schemes, &default_scheme );
			if (err > 0)
			{
				free( config_file );
				config_file = NULL;
				err = 0;
			}
		}
#endif
#ifdef	GREEN_SYSCONFIG_FILE
		if (!config_file)
		{
			config_file = GREEN_SYSCONFIG_FILE;
			err = ParseConfig( config_file, &schemes, &default_scheme );
			if (err > 0)
			{
				config_file = NULL;
				err = 0;
			}
		}
#endif
	}
	
	if (err < 0)
	{
		fprintf( stderr, "Malformed configuration file '%s'!\n", config_file );
		return err;
	}
	
	if (!current_scheme && default_scheme)
		current_scheme = default_scheme;
	
	if (current_scheme)
	{
		if ((err = ProcessSchemes( &rtd, &schemes, current_scheme )))
			return err;
	}
	
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
		else if (!strncmp( opt, "config=", 7 ) || !strncmp( opt, "scheme=", 7 ))
		{
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
