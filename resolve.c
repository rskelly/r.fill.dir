#include <stdlib.h>
#include <string.h>
#include <grass/gis.h>
#include <grass/raster.h>
#include <grass/glocale.h>
#include "tinf.h"

CELL select_dir(CELL i)
{
    CELL dir[256] = { 0, 1, 2, 2, 4, 1, 2, 2, 8, 1,
	8, 2, 8, 4, 4, 2, 16, 16, 16, 2, 16, 4, 4,
	2, 8, 8, 8, 8, 8, 8, 8, 4, 32, 1, 2, 2,
	4, 4, 2, 2, 32, 8, 8, 2, 8, 8, 4, 4, 32,
	32, 32, 32, 16, 32, 4, 2, 16, 16, 16, 16, 8, 16,
	8, 8, 64, 64, 64, 1, 64, 1, 2, 2, 64, 64, 8,
	2, 8, 8, 4, 2, 16, 64, 64, 2, 16, 64, 2, 2,
	16, 8, 8, 8, 8, 8, 8, 4, 32, 64, 32, 1, 32,
	32, 32, 2, 32, 32, 32, 2, 32, 8, 4, 4, 32, 32,
	32, 32, 32, 32, 32, 32, 32, 32, 16, 16, 16, 16, 8,
	8, 128, 128, 128, 1, 4, 1, 2, 2, 128, 128, 2, 1,
	8, 4, 4, 2, 16, 128, 2, 1, 4, 128, 2, 1, 8,
	128, 8, 1, 8, 8, 4, 2, 32, 128, 1, 1, 128, 128,
	2, 1, 32, 128, 32, 1, 8, 128, 4, 2, 32, 32, 32,
	1, 32, 128, 32, 1, 16, 16, 16, 1, 16, 16, 8, 4,
	128, 128, 128, 128, 128, 128, 2, 1, 128, 128, 128, 1, 128,
	128, 4, 2, 64, 128, 128, 1, 128, 128, 128, 1, 8, 128,
	8, 1, 8, 8, 8, 2, 64, 128, 64, 128, 64, 128, 64,
	128, 32, 64, 64, 128, 64, 64, 64, 1, 32, 64, 64, 128,
	64, 64, 64, 128, 32, 32, 32, 64, 32, 32, 16, 128
    };

    return dir[i];
}

void flink(int i, int j, int nl, int ns, CELL * p1, CELL * p2, CELL * p3,
	   int *active, int *goagain)
{
    CELL bitmask[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };
    CELL outflow, cwork, c[8];
    int k;

    cwork = p2[j];
    if (Rast_is_c_null_value(p2 + j) || cwork >= 0 || cwork == -256)
	return;
    cwork = -cwork;

    for (k = 7; k >= 0; k--) {
	c[k] = 0;
	if (cwork >= bitmask[k]) {
	    c[k] = 1;
	    cwork -= bitmask[k];
	}
    }

    outflow = 0;

    /* There's no need to resolve directions at cells adjacent to null cells,
     * as those directions will be resolved before we get here */
    /* look one row back */
    cwork = p1[j - 1];
    if (cwork > 0 && cwork != 4 && c[6])
	outflow += 64;
    cwork = p1[j];
    if (cwork > 0 && cwork != 8 && c[7])
	outflow += 128;
    cwork = p1[j + 1];
    if (cwork > 0 && cwork != 16 && c[0])
	outflow += 1;

    /* look at this row */
    cwork = p2[j - 1];
    if (cwork > 0 && cwork != 2 && c[5])
	outflow += 32;
    cwork = p2[j + 1];
    if (cwork > 0 && cwork != 32 && c[1])
	outflow += 2;

    /* look one row forward */
    cwork = p3[j - 1];
    if (cwork > 0 && cwork != 1 && c[4])
	outflow += 16;
    cwork = p3[j];
    if (cwork > 0 && cwork != 128 && c[3])
	outflow += 8;
    cwork = p3[j + 1];
    if (cwork > 0 && cwork != 64 && c[2])
	outflow += 4;

    if (outflow == 0) {
	*active = 1;
    }
    else {
	*goagain = 1;
	p2[j] = select_dir(outflow);
    }
    return;
}

//void resolve(int fd, int nl, struct band3 *bnd)
void resolve(char* dirs, int nl, struct band3 *bnd)
{
    CELL cvalue;
    int *active;
    int offset, isz, i, j, pass, activity, goagain, done;

    active = (int *)G_calloc(nl, sizeof(int));

    isz = sizeof(CELL);

    // Address of dirs.
    char* dirsbuf;

    /* select a direction when there are multiple non-flat links */

    dirsbuf = dirs;

    for (i = 1; i < nl - 1; i += 1) {
    	memcpy(bnd->b[0], dirsbuf, bnd->sz);
    	dirsbuf += bnd->sz;

		for (j = 1; j < bnd->ns - 1; j += 1) {
	    	offset = j * isz;
	    	if (Rast_is_c_null_value((CELL *) (bnd->b[0] + offset)))
				continue;
	    	memcpy(&cvalue, bnd->b[0] + offset, isz);
	    	if (cvalue > 0)
				cvalue = select_dir(cvalue);
	    	memcpy(bnd->b[0] + offset, &cvalue, isz);
		}
		dirsbuf -= bnd->sz;
		memcpy(dirsbuf, bnd->b[0], bnd->sz);
		dirsbuf += bnd->sz;

    }

    pass = 0;
    for (i = 1; i < nl - 1; i += 1)
		active[i] = 1;

    /* select a direction when there are multiple flat links */
    do {
		done = 1;
		pass += 1;

		activity = 0;
		G_verbose_message(_("Downward pass %d"), pass);

		dirsbuf = dirs;

		advance_band3mem(&dirsbuf, bnd);
		advance_band3mem(&dirsbuf, bnd);
		for (i = 1; i < nl - 1; i++) {
			dirsbuf = dirs + (i + 1) * bnd->sz;

		    advance_band3mem(&dirsbuf, bnd);

		    if (!active[i])
				continue;

		    done = 0;
		    active[i] = 0;
		    do {
				goagain = 0;
				for (j = 1; j < bnd->ns - 1; j += 1) {
				    flink(i, j, nl, bnd->ns,
					  (CELL *) bnd->b[0], (CELL *) bnd->b[1],
					  (CELL *) bnd->b[2], &active[i], &goagain);
				    if (goagain)
						activity = 1;
				}
		    } while (goagain);

		    dirsbuf = dirs + i * bnd->sz;
		    memcpy(dirsbuf, bnd->b[1], bnd->sz);
		    dirsbuf += bnd->sz;

		}

		if (!activity) {
		    G_warning(_("Could not solve for all cells"));
		    done = 1;
		    continue;
		}

		activity = 0;
		G_verbose_message(_("Upward pass %d"), pass);

		dirsbuf = dirs + (nl - 1) * bnd->sz;

		retreat_band3mem(&dirsbuf, bnd);
		retreat_band3mem(&dirsbuf, bnd);
		for (i = nl - 2; i >= 1; i -= 1) {
			dirsbuf = dirs + (i - 1) * bnd->sz;

		    retreat_band3mem(&dirsbuf, bnd);

		    if (!active[i])
				continue;

		    done = 0;
		    active[i] = 0;
		    do {
				goagain = 0;
				for (j = 1; j < bnd->ns - 1; j++) {
				    flink(i, j, nl, bnd->ns,
					  (CELL *) bnd->b[0], (CELL *) bnd->b[1],
					  (CELL *) bnd->b[2], &active[i], &goagain);
				    if (goagain)
						activity = 1;
				}
		    } while (goagain);

		    dirsbuf = dirs + i * bnd->sz;
		    memcpy(dirsbuf, bnd->b[1], bnd->sz);
		    dirsbuf += bnd->sz;
		}

		if (!activity) {
		    G_warning(_("Could not solve for all cells"));
		    done = 1;
		}

    } while (!done);

    G_free(active);

    return;

}
