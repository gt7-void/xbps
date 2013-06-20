/*-
 * Copyright (c) 2008-2013 Juan Romero Pardines.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_STRCASESTR
# define _GNU_SOURCE    /* for strcasestr(3) */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <fnmatch.h>
#include <assert.h>

#include <xbps.h>
#include "defs.h"

struct search_data {
	int npatterns;
	char **patterns;
	int maxcols;
	xbps_array_t results;
};

static void
print_results(struct xbps_handle *xhp, struct search_data *sd)
{
	const char *pkgver, *desc, *inststr;
	char tmp[256], *out;
	unsigned int i, j, tlen = 0, len = 0;

	/* Iterate over results array and find out largest pkgver string */
	for (i = 0; i < xbps_array_count(sd->results); i++) {
		xbps_array_get_cstring_nocopy(sd->results, i, &pkgver);
		len = strlen(pkgver);
		if (tlen == 0 || len > tlen)
			tlen = len;
		i++;
	}
	for (i = 0; i < xbps_array_count(sd->results); i++) {
		xbps_array_get_cstring_nocopy(sd->results, i, &pkgver);
		xbps_array_get_cstring_nocopy(sd->results, i+1, &desc);
		strncpy(tmp, pkgver, sizeof(tmp));
		for (j = strlen(tmp); j < tlen; j++)
			tmp[j] = ' ';

		tmp[j] = '\0';
		if (xbps_pkgdb_get_pkg(xhp, pkgver))
			inststr = "[*]";
		else
			inststr = "[-]";

		len = strlen(inststr) + strlen(tmp) + strlen(desc) + 3;
		if ((int)len > sd->maxcols) {
			out = malloc(sd->maxcols+1);
			assert(out);
			snprintf(out, sd->maxcols-3, "%s %s %s",
			    inststr, tmp, desc);
			strncat(out, "...\n", sd->maxcols);
			printf("%s", out);
			free(out);
		} else {
			printf("%s %s %s\n", inststr, tmp, desc);
		}
		i++;
	}
}

static int
search_pkgs_cb(struct xbps_repo *repo, void *arg, bool *done)
{
	xbps_array_t allkeys;
	xbps_dictionary_t pkgd;
	xbps_dictionary_keysym_t ksym;
	struct search_data *sd = arg;
	const char *pkgver, *desc;
	unsigned int i;
	int x;

	(void)done;

	allkeys = xbps_dictionary_all_keys(repo->idx);
	for (i = 0; i < xbps_array_count(allkeys); i++) {
		ksym = xbps_array_get(allkeys, i);
		pkgd = xbps_dictionary_get_keysym(repo->idx, ksym);

		xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		xbps_dictionary_get_cstring_nocopy(pkgd, "short_desc", &desc);

		for (x = 0; x < sd->npatterns; x++) {
			bool vpkgfound = false;

			if (xbps_match_virtual_pkg_in_dict(pkgd, sd->patterns[x], false))
				vpkgfound = true;

			if ((xbps_pkgpattern_match(pkgver, sd->patterns[x])) ||
			    (strcasestr(pkgver, sd->patterns[x])) ||
			    (strcasestr(desc, sd->patterns[x])) || vpkgfound) {
				xbps_array_add_cstring_nocopy(sd->results, pkgver);
				xbps_array_add_cstring_nocopy(sd->results, desc);
			}
		}
	}
	xbps_object_release(allkeys);

	return 0;
}

int
repo_search(struct xbps_handle *xhp, int npatterns, char **patterns)
{
	struct search_data sd;
	int rv;

	sd.npatterns = npatterns;
	sd.patterns = patterns;
	sd.maxcols = get_maxcols();
	sd.results = xbps_array_create();

	rv = xbps_rpool_foreach(xhp, search_pkgs_cb, &sd);
	if (rv != 0 && rv != ENOTSUP)
		fprintf(stderr, "Failed to initialize rpool: %s\n",
		    strerror(rv));

	print_results(xhp, &sd);

	return rv;
}
