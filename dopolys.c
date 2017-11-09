#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <grass/gis.h>
#include <grass/raster.h>
#include <grass/glocale.h>

#include "ds.h"

void recurse_list(int flag, int *cells, int sz, int start)
{
    int cnt, i, j, ii, jj;

    i = cells[start];
    j = cells[start + 1];
    cells[start + 2] = flag;

    for (cnt = 0; cnt < sz; cnt += 3) {
	ii = cells[cnt];
	jj = cells[cnt + 1];

	if (ii == i - 1 && (jj == j - 1 || jj == j || jj == j + 1)) {
	    if (cells[cnt + 2] == 0)
		recurse_list(flag, cells, sz, cnt);
	}

	else if (ii == i && (jj == j - 1 || jj == j + 1)) {
	    if (cells[cnt + 2] == 0)
		recurse_list(flag, cells, sz, cnt);
	}

	else if (ii == i + 1 && (jj == j - 1 || jj == j || jj == j + 1)) {
	    if (cells[cnt + 2] == 0)
		recurse_list(flag, cells, sz, cnt);
	}
    }
}
/*
struct rec {
    int flag;
    int* cells;
    int sz;
    int start;
};

void recurse_list(struct queue* q, int flag, int *cells, int sz, int start)
{

    struct rec* r = (struct rec*) calloc(1, sizeof(struct rec));
    r->flag = flag;
    r->cells = cells;
    r->sz = sz;
    r->start = start;
    queue_push(q, r);

    int cnt, i, j, ii, jj;
    struct rec* rr;

    while((r = queue_pop(q))) {

        i = r->cells[r->start];
        j = r->cells[r->start + 1];
        r->cells[r->start + 2] = r->flag;

        for (cnt = 0; cnt < r->sz; cnt += 3) {
            ii = r->cells[cnt];
            jj = r->cells[cnt + 1];

            if (ii == i - 1 && (jj == j - 1 || jj == j || jj == j + 1)) {
                if (cells[cnt + 2] == 0) {
                    rr = (struct rec*) calloc(1, sizeof(struct rec));
                    rr->flag = flag;
                    rr->cells = cells;
                    rr->sz = sz;
                    rr->start = cnt;
                    queue_insert(q, rr);
                }
            } else if (ii == i && (jj == j - 1 || jj == j + 1)) {
                if (cells[cnt + 2] == 0) {
                    rr = (struct rec*) calloc(1, sizeof(struct rec));
                    rr->flag = flag;
                    rr->cells = cells;
                    rr->sz = sz;
                    rr->start = cnt;
                    queue_insert(q, rr);
                }
            } else if (ii == i + 1 && (jj == j - 1 || jj == j || jj == j + 1)) {
                if (cells[cnt + 2] == 0) {
                    rr = (struct rec*) calloc(1, sizeof(struct rec));
                    rr->flag = flag;
                    rr->cells = cells;
                    rr->sz = sz;
                    rr->start = cnt;
                    queue_insert(q, rr);                    
                }
            }
        }
    }
}
*/

/* scan the direction file and construct a list of all cells with negative
 * values.  The list will contain the row and column of the cell and a space
 * to include the polygon number */

//int dopolys(int fd, int fm, int nl, int ns)
int dopolys(char* dirs, char* prob, int nl, int ns)
{
    int cnt, i, j, found, flag;
    int bufsz, cellsz;
    int *cells;
    int *dir;

    char* dirsbuf;
    char* probbuf;

    bufsz = ns * sizeof(int);
    dir = (int *)G_calloc(ns, sizeof(int));
    cellsz = 3 * ns;
    cells = (int *)G_malloc(cellsz * sizeof(int));

    found = 0;

    dirsbuf = dirs;

    for (i = 1; i < nl - 1; i += 1) {
    	memcpy(dir, dirsbuf, bufsz);
    	dirsbuf += bufsz;

		for (j = 1; j < ns - 1; j += 1) {
		    if (Rast_is_c_null_value(&dir[j]) || dir[j] >= 0)
				continue;
		    cells[found++] = i;
		    cells[found++] = j;
		    cells[found++] = 0;

		    if (found >= cellsz) {
				cellsz += 3 * ns;
				cells = (int *)G_realloc(cells, cellsz * sizeof(int));
		    }
		}
    }
    if (found == 0)
		return 0;

    /* Loop through the list, assigning polygon numbers to unassigned entries
       and carrying the same assignment over to adjacent cells.  Repeat
       recursively */

    //struct queue* q = queue_init(&q_deleter);

    flag = 0;
    for (i = 0; i < found; i += 3) {
		if (cells[i + 2] == 0) {
		    flag += 1;
	    	recurse_list(flag, cells, found, i);
		}
    }
    
    G_message(n_("Found %d unresolved area", "Found %d unresolved areas", flag), flag);

    /* Compose a new raster map to contain the resulting assignments */
    probbuf = prob;

    cnt = 0;
    for (i = 0; i < nl; i += 1) {
		for (j = 0; j < ns; j += 1)
	    	dir[j] = -1;
		while (cells[cnt] == i) {
	    	dir[cells[cnt + 1]] = cells[cnt + 2];
	    	cnt += 3;
		}
		memcpy(probbuf, dir, bufsz);
		probbuf += bufsz;
    }

    G_free(cells);
    G_free(dir);

    return flag;
}
