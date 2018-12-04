/*
Dragonfly IMS to TiFF converter

https://github.com/UQ-RCC/ims2tif

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2018 The University of Queensland.

Permission is hereby  granted, free of charge, to any  person obtaining a copy
of this software and associated  documentation files (the "Software"), to deal
in the Software  without restriction, including without  limitation the rights
to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <cstdio>
#include <cstring>
#include "parg/parg.h"
#include "ims2tif.hpp"

#define ARGDEF_OUTDIR	'o'
#define ARGDEF_PREFIX	'p'
#define ARGDEF_METHOD	'm'
#define ARGDEF_HELP		'h'

static struct parg_option argdefs[] = {
	{"outdir",  PARG_REQARG,    nullptr,	ARGDEF_OUTDIR},
	{"prefix",  PARG_REQARG,    nullptr,	ARGDEF_PREFIX},
	{"method",  PARG_REQARG,    nullptr,	ARGDEF_METHOD},
	{"help",	PARG_NOARG,		nullptr,	ARGDEF_HELP},
	{nullptr,	0,			    nullptr,	0}
};

static const char *USAGE_OPTIONS = 
"  -h, --help\n"
"                          Display this message.\n"
"  -o, --outdir\n"
"                          Write TIFFs to the specified directory.\n"
"                          If unspecified, use the current directory.\n"
"  -p, --prefix\n"
"                          The prefix of the output file. If unspecified,\n"
"                          use the base name of the input file plus a trailing _.\n"
"  -m, --method\n"
"                          The conversion method to use. If unspecified, use \"chunked\".\n"
"                          Available methods are \"bigload\", \"chunked\", and \"hyperslab\".\n"
"";

int ims::parse_arguments(int argc, char **argv, FILE *out, FILE *err, args_t *args)
{
	parg_state ps;
	parg_init(&ps);

	memset(args, 0, sizeof(args_t));

	auto usage = [&argv](int val, FILE *s){
		fprintf(s, "Usage: %s [OPTIONS] <file.ims> \nOptions:\n%s", argv[0], USAGE_OPTIONS);
		return val;
	};

	for(int c; (c = parg_getopt_long(&ps, argc, argv, "ho:p:m:", argdefs, nullptr)) != -1; )
	{
		switch(c)
		{
			case ARGDEF_HELP:
				return usage(2, out);

			case ARGDEF_OUTDIR:
				if(!args->outdir.empty())
					return usage(2, out);
				args->outdir = ps.optarg;
				break;

			case ARGDEF_PREFIX:
				if(!args->prefix.empty())
					return usage(2, out);
				args->prefix = ps.optarg;
				break;

			case ARGDEF_METHOD:
				if(!args->method.empty())
					return usage(2, out);
				args->method = ps.optarg;
				break;

			case 1:
				if(!args->file.empty())
					return usage(2, out);
				args->file = ps.optarg;
				break;

			case '?':
			case ':':
			default:
				return usage(2, out);
		}
	}

	if(args->file.empty())
		return usage(2, out);
	
	if(args->outdir.empty())
		args->outdir = ".";

	if(args->prefix.empty())
	{
		args->prefix = args->file.stem();
		args->prefix.append("_");
	}

	if(args->method.empty())
		args->method = "chunked";

	return 0;
}
