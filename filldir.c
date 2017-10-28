#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <grass/gis.h>
#include <grass/raster.h>
#include "tinf.h"

/* get the slope between two cells and return a slope direction */
void check(CELL newdir, CELL * dir, void *center, void *edge, double cnst,
	   double *oldslope)
{
    double newslope;

    /* always discharge to a null boundary */
    if (is_null(edge)) {
	*oldslope = DBL_MAX;
	*dir = newdir;
    }
    else {
	newslope = slope(center, edge, cnst);
	if (newslope == *oldslope) {
	    *dir += newdir;
	}
	else if (newslope > *oldslope) {
	    *oldslope = newslope;
	    *dir = newdir;
	}
    }

    return;

}

/* process one row, filling single-cell pits */
int fill_row(int nl, int ns, struct band3 *bnd)
{
    int j, offset, inc, rc;
    void *min;
    char *center;
    char *edge;

    inc = bpe();

    min = G_malloc(bpe());

    rc = 0;
    for (j = 1; j < ns - 1; j += 1) {
	offset = j * bpe();
	center = bnd->b[1] + offset;
	if (is_null(center))
	    return rc;

	edge = bnd->b[0] + offset;
	min = edge - inc;
	min = get_min(min, edge);
	min = get_min(min, edge + inc);

	min = get_min(min, center - inc);
	min = get_min(min, center + inc);

	edge = bnd->b[2] + offset;
	min = get_min(min, edge - inc);
	min = get_min(min, edge);
	min = get_min(min, edge + inc);

	if (get_min(center, min) == center) {
	    rc = 1;
	    memcpy(center, min, bpe());
	}

    }
    return rc;
}

/* determine the flow direction at each cell on one row */
void build_one_row(int i, int nl, int ns, struct band3 *bnd, CELL * dir)
{
    int j, offset, inc;
    CELL sdir;
    double slope;
    char *center;
    char *edge;

    inc = bpe();

    for (j = 0; j < ns; j += 1) {
	offset = j * bpe();
	center = bnd->b[1] + offset;
	if (is_null(center)) {
	    Rast_set_c_null_value(dir + j, 1);
	    continue;
	}

	sdir = 0;
	slope = HUGE_VAL;
	if (i == 0) {
	    sdir = 128;
	}
	else if (i == nl - 1) {
	    sdir = 8;
	}
	else if (j == 0) {
	    sdir = 32;
	}
	else if (j == ns - 1) {
	    sdir = 2;
	}
	else {
	    slope = -HUGE_VAL;

	    /* check one row back */
	    edge = bnd->b[0] + offset;
	    check(64, &sdir, center, edge - inc, 1.4142136, &slope);
	    check(128, &sdir, center, edge, 1., &slope);
	    check(1, &sdir, center, edge + inc, 1.4142136, &slope);

	    /* check this row */
	    check(32, &sdir, center, center - inc, 1., &slope);
	    check(2, &sdir, center, center + inc, 1., &slope);

	    /* check one row forward */
	    edge = bnd->b[2] + offset;
	    check(16, &sdir, center, edge - inc, 1.4142136, &slope);
	    check(8, &sdir, center, edge, 1., &slope);
	    check(4, &sdir, center, edge + inc, 1.4142136, &slope);
	}

	if (slope == 0.)
	    sdir = -sdir;
	else if (slope < 0.)
	    sdir = -256;
	dir[j] = sdir;
    }
    return;
}

//void filldir(int fe, int fd, int nl, struct band3 *bnd)
void filldir(char* elev, char* dirs, int nl, struct band3 *bnd)
{
    int i, bufsz;
    CELL *dir;

    // Get the starting address of the elev and dirs buffers.
    char* elevbuf;
    char* dirsbuf;

    /* fill single-cell depressions, except on outer rows and columns */
    elevbuf = elev;
    advance_band3mem(&elevbuf, bnd);
    advance_band3mem(&elevbuf, bnd);

    for (i = 1; i < nl - 1; i += 1) {

    	elevbuf = elev + (i + 1) * bnd->sz;
		advance_band3mem(&elevbuf, bnd);

		if (fill_row(nl, bnd->ns, bnd)) {
			elevbuf = elev + i * bnd->sz;
			memcpy(elevbuf, bnd->b[1], bnd->sz);
			elevbuf += bnd->sz;
		}
    }

    advance_band3mem(0, bnd);

    if (fill_row(nl, bnd->ns, bnd)) {
    	elevbuf = elev + i * bnd->sz;
    	memcpy(elevbuf, bnd->b[1], bnd->sz);
    	elevbuf += bnd->sz;
    }

    /* determine the flow direction in each cell.  On outer rows and columns
     * the flow direction is always directly out of the map */

    dir = G_calloc(bnd->ns, sizeof(CELL));
    bufsz = bnd->ns * sizeof(CELL);

    elevbuf = elev;
    dirsbuf = dirs;

    advance_band3mem(&elevbuf, bnd);

    // TODO: The original has i < nl, but this seems to fail on advance_band, because it 
    // forces 2 too many advances.
    for (i = 0; i < nl - 2; i += 1) {
		advance_band3mem(&elevbuf, bnd);
		build_one_row(i, nl, bnd->ns, bnd, dir);
		memcpy(dirsbuf, dir, bufsz);
		dirsbuf += bufsz;
    }

    advance_band3mem(&elevbuf, bnd);
    build_one_row(i, nl, bnd->ns, bnd, dir);
	memcpy(dirsbuf, dir, bufsz);
	dirsbuf += bufsz;

    G_free(dir);

    return;
}
