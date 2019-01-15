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
#define ARGDEF_FORMAT	'f'
#define ARGDEF_HELP		'h'

static struct parg_option argdefs[] = {
	{"outdir",  PARG_REQARG,    nullptr,	ARGDEF_OUTDIR},
	{"prefix",  PARG_REQARG,    nullptr,	ARGDEF_PREFIX},
	{"method",  PARG_REQARG,    nullptr,	ARGDEF_METHOD},
	{"format",  PARG_REQARG,    nullptr,	ARGDEF_FORMAT},
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
"  -f, --format\n"
"                          The output file format. If unspecified, use \"bigtiff\".\n"
"                          Available formats are \"tiff\", \"bigtiff\".\n"
"";

ims::args_t::args_t() noexcept :
	bigtiff(true),
	method(conversion_method_t::chunked)
{}

int ims::parse_arguments(int argc, char **argv, FILE *out, FILE *err, args_t *args)
{
	parg_state ps;
	parg_init(&ps);

	auto usage = [&argv](int val, FILE *s){
		fprintf(s, "Usage: %s [OPTIONS] <file.ims> \nOptions:\n%s", argv[0], USAGE_OPTIONS);
		return val;
	};

	bool have_method = false;
	bool have_format = false;

	for(int c; (c = parg_getopt_long(&ps, argc, argv, "ho:p:m:f:", argdefs, nullptr)) != -1; )
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
				args->prefix = std::string(ps.optarg);
				break;

			case ARGDEF_METHOD:
				if(have_method)
					return usage(2, out);

				if(!strcmp(ps.optarg, "bigload"))
					args->method = conversion_method_t::bigload;
				else if(!strcmp(ps.optarg, "chunked"))
					args->method = conversion_method_t::chunked;
				else if(!strcmp(ps.optarg, "hyperslab"))
					args->method = conversion_method_t::hyperslab;
				else
					return usage(2, out);

				have_method = true;
				break;

			case ARGDEF_FORMAT:
				if(have_format)
					return usage(2, out);

				if(!strcmp(ps.optarg, "tiff"))
					args->bigtiff = false;
				else if(!strcmp(ps.optarg, "bigtiff"))
					args->bigtiff = true;
				else
					return usage(2, out);

				have_format = true;
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
		args->prefix = args->file.stem().u8string();
		args->prefix.append("_");
	}

	return 0;
}
