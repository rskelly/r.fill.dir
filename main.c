/*
 *
 *****************************************************************************
 *
 * MODULE:       r.fill.dir
 * AUTHOR(S):    Original author unknown - Raghavan Srinivasan Nov, 1991
 *               (srin@ecn.purdue.edu) Agricultural Engineering, 
 *               Purdue University
 *               Markus Neteler: update to FP (C-code)
 *                             : update to FP (Fortran)
 *               Roger Miller: rewrite all code in C, complient with GRASS 5
 * PURPOSE:      fills a DEM to become a depression-less DEM
 *               This creates two layers from a user specified elevation map.
 *               The output maps are filled elevation or rectified elevation
 *               map and a flow direction map based on one of the type
 *               specified. The filled or rectified elevation map generated
 *               will be filled for depression, removed any circularity or
 *               conflict flow direction is resolved. This program helps to
 *               get a proper elevation map that could be used for
 *               delineating watershed using r.watershed module. However, the
 *               boundaries may have problem and could be resolved using
 *               the cell editor d.rast.edit
 *               Options have been added to produce a map of undrained areas
 *               and to run without filling undrained areas except single-cell
 *               pits.  Not all problems can be solved in a single pass.  The
 *               program can be run repeatedly, using the output elevations from
 *               one run as input to the next run until all problems are 
 *               resolved.
 * COPYRIGHT:    (C) 2001, 2010 by the GRASS Development Team
 *
 *               This program is free software under the GNU General Public
 *               License (>=v2). Read the file COPYING that comes with GRASS
 *               for details.
 *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/mman.h>

/* for using the "open" statement */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* for using the close statement */
#include <unistd.h>
#include <errno.h>

#include <grass/gis.h>
#include <grass/raster.h>
#include <grass/glocale.h>

#define DEBUG
#include "tinf.h"
#include "local.h"

static int dir_type(int type, int dir);

int allocate(char** elev, off_t elevsize, char** dirs, off_t dirsize, 
    char** prob, off_t probsize, struct Flag* flag) {
         G_verbose_message(_("%ld %ld %ld"), elevsize, dirsize, probsize);

    if(flag->answer) {
        if(MAP_FAILED == (*elev = (char*) mmap(NULL, elevsize, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0))) {
            G_important_message(_("Failed to map memory for filled: %s"), strerror(errno));
            return 0;
        }
        if(MAP_FAILED == (*dirs = (char*) mmap(NULL, dirsize, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0))) {
            G_important_message(_("Failed to map memory for directions: %s"), strerror(errno));
            return 0;
        }
        if(MAP_FAILED == (*prob = (char*) mmap(NULL, probsize, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0))) {
            G_important_message(_("Failed to map memory for problems: %s"), strerror(errno));
            return 0;
        }
    } else {
        if(!(*elev = (char*) malloc(elevsize))) {
            G_important_message(_("Failed to allocate memory for filled: %s"), strerror(errno));
            return 0;
        }
        if(!(*dirs = (char*) malloc(dirsize))) {
            G_important_message(_("Failed to allocate memory for directions: %s"), strerror(errno));
            return 0;
        }
        if(!(*prob = (char*) malloc(probsize))) {
            G_important_message(_("Failed to allocate memory for problems: %s"), strerror(errno));            
            return 0;
        }
    }
    return 1;
}

void deallocate(char* elev, off_t elevsize, char* dirs, off_t dirsize, 
    char* prob, off_t probsize, struct Flag* flag) {
    if(flag->answer) {
        munmap(elev, elevsize);
        munmap(dirs, dirsize);
        munmap(prob, probsize);
    } else {
        free(elev);
        free(dirs);
        free(prob);
    }
}

int main(int argc, char **argv)
{

    int i, j, type;
    int new_id;
    int nrows, ncols, nbasins;
    int map_id, dir_id, bas_id;
    char map_name[GNAME_MAX], new_map_name[GNAME_MAX];
    char dir_name[GNAME_MAX];
    char bas_name[GNAME_MAX];

    struct Cell_head window;
    struct GModule *module;
    struct Option *opt1, *opt2, *opt3, *opt4, *opt5;
    struct Flag *flag1, *flag2;
    int in_type, bufsz;
    void *in_buf;
    CELL *out_buf;
    struct band3 bnd, bndC;
    struct Colors colors; 

    // Initialize the GRASS environment variables.
    G_gisinit(argv[0]);

    module = G_define_module();
    G_add_keyword(_("raster"));
    G_add_keyword(_("hydrology"));
    G_add_keyword(_("sink"));
    G_add_keyword(_("fill sinks"));
    G_add_keyword(_("depressions"));
    module->description = _("Filters and generates a depressionless elevation map and a "
        "flow direction map from a given elevation raster map.");
    
    opt1 = G_define_standard_option(G_OPT_R_ELEV);
    opt1->key = "input";
    
    opt2 = G_define_standard_option(G_OPT_R_OUTPUT);
    opt2->description = _("Name for output depressionless elevation raster map");
    
    opt4 = G_define_standard_option(G_OPT_R_OUTPUT);
    opt4->key = "direction";
    opt4->description = _("Name for output flow direction map for depressionless elevation raster map");

    opt5 = G_define_standard_option(G_OPT_R_OUTPUT);
    opt5->key = "areas";
    opt5->required = NO;
    opt5->description = _("Name for output raster map of problem areas");

    opt3 = G_define_option();
    opt3->key = "format";
    opt3->type = TYPE_STRING;
    opt3->required = NO;
    opt3->description = _("Aspect direction format");
    opt3->options = "agnps,answers,grass";
    opt3->answer = "grass";
    
    flag1 = G_define_flag();
    flag1->key = 'f';
    flag1->description = _("Find unresolved areas only");
    
    flag2 = G_define_flag();
    flag2->key = 'm';
    flag2->description = _("Use mapped memory");
    
    if (G_parser(argc, argv))
	   exit(EXIT_FAILURE);

    if (flag1->answer && opt5->answer == NULL)
    	G_fatal_error(_("The '%c' flag requires '%s'to be specified"), flag1->key, opt5->key);

    type = 0;
    strcpy(map_name, opt1->answer);
    strcpy(new_map_name, opt2->answer);
    strcpy(dir_name, opt4->answer);
    if (opt5->answer != NULL)
	   strcpy(bas_name, opt5->answer);

    if (strcmp(opt3->answer, "agnps") == 0)
	   type = 1;
    else if (strcmp(opt3->answer, "answers") == 0)
	   type = 2;
    else if (strcmp(opt3->answer, "grass") == 0)
	   type = 3;
    
    G_debug(1, "output type (1=AGNPS, 2=ANSWERS, 3=GRASS): %d", type);

    if (type == 3)
	   G_verbose_message(_("Direction map is D8 resolution, i.e. 45 degrees"));
    
    // Open the maps and get their file ids.
    map_id = Rast_open_old(map_name, "");
    if (Rast_read_colors(map_name, "", &colors) < 0)
        G_warning(_("Unable to read color table for raster map <%s>"), map_name);
    
    // Allocate cell buf for the map layer.
    in_type = Rast_get_map_type(map_id);

    // Set the pointers for multi-typed functions.
    set_func_pointers(in_type);

    // Get the window information.
    G_get_window(&window);
    nrows = Rast_window_rows();
    ncols = Rast_window_cols();

    // Buffers for internal use.
    bndC.ns = ncols;
    bndC.sz = sizeof(CELL) * ncols;
    bndC.b[0] = G_calloc(ncols, sizeof(CELL));
    bndC.b[1] = G_calloc(ncols, sizeof(CELL));
    bndC.b[2] = G_calloc(ncols, sizeof(CELL));

    // Buffers for external use.
    bnd.ns = ncols;
    bnd.sz = ncols * bpe();
    bnd.b[0] = G_calloc(ncols, bpe());
    bnd.b[1] = G_calloc(ncols, bpe());
    bnd.b[2] = G_calloc(ncols, bpe());

    in_buf = get_buf();

    int mb = 1024 * 1024;

    // The size of the memory mappings. Must be rounded up to the nearest page boundary.
    off_t mapsize = nrows * ncols;
    off_t elevsize = ((mapsize * bpe()) / sysconf(_SC_PAGE_SIZE) + 1) * sysconf(_SC_PAGE_SIZE);
    off_t dirsize = ((mapsize * sizeof(CELL)) / sysconf(_SC_PAGE_SIZE) + 1) * sysconf(_SC_PAGE_SIZE);
    off_t probsize = ((mapsize * sizeof(CELL)) / sysconf(_SC_PAGE_SIZE) + 1) * sysconf(_SC_PAGE_SIZE);
    G_verbose_message(_("Memory allocations: elev: %ldMB; dirs: %ldMB; probs: %ldMB"), elevsize / mb, dirsize / mb, probsize / mb);

    // Pointers to memory (mapped or malloced). Replaces the file handles used in the original.
    char* elev;
    char* dirs;
    char* prob;

    // Pointers to the mapped memory. These can be moved, the original pointers should not be.
    char* elevbuf;
    char* dirsbuf;
    char* probbuf;

    if(flag2->answer) {
        G_important_message(_("Using mapped memory."));
    } else {
        G_important_message(_("Using physical RAM."));
    }

    if(!allocate(&elev, elevsize, &dirs, dirsize, &prob, probsize, flag2)) {
        G_important_message(_("Failed to allocate memory. Try using mapped?"));
        return 1;
    };

    // Copy the source image into the mapped buffer.
    G_message(_("Reading input elevation raster map..."));
    for (i = 0; i < nrows; i++) {
	   G_percent(i, nrows, 2);
	   get_row(map_id, in_buf, i);
       memcpy(elev + i * bnd.sz, in_buf, bnd.sz);
    }
    G_percent(1, 1, 1);
    Rast_close(map_id);

    // Fill single-cell holes and take a first stab at flow directions.
    G_message(_("Filling sinks..."));
    filldir(elev, dirs, nrows, &bnd);

    // Determine flow directions for ambiguous cases.
    G_message(_("Determining flow directions for ambiguous cases..."));
    resolve(dirs, nrows, &bndC);

    // Mark and count the sinks in each internally drained basin.
    nbasins = dopolys(dirs, prob, nrows, ncols);
    if (!flag1->answer) {
    	// Determine the watershed for each sink.
        G_message(_("Determining watershed for each sink..."));
    	wtrshed(prob, dirs, nrows, ncols, 4);

    	// Fill all of the watersheds up to the elevation necessary for drainage.
    	G_message(_("Filling watersheds..."));
        ppupdate(elev, prob, nrows, nbasins, &bnd, &bndC);

    	// Repeat the first three steps to get the final directions.
    	G_message(_("Repeat to get the final directions..."));
    	filldir(elev, dirs, nrows, &bnd);
    	resolve(dirs, nrows, &bndC);
    	nbasins = dopolys(dirs, prob, nrows, ncols);
    }

    G_free(bndC.b[0]);
    G_free(bndC.b[1]);
    G_free(bndC.b[2]);

    G_free(bnd.b[0]);
    G_free(bnd.b[1]);
    G_free(bnd.b[2]);

    G_important_message(_("Writing output raster maps..."));

    out_buf = Rast_allocate_c_buf();
    bufsz = ncols * sizeof(CELL);

    // Reset the elevations buffer position.
    elevbuf = elev;
    new_id = Rast_open_new(new_map_name, in_type);

    // Reset the directions buffer position.
    dirsbuf = dirs;
    dir_id = Rast_open_new(dir_name, CELL_TYPE);

    // Write problem areas to a file.
    if (opt5->answer != NULL) {
        G_important_message(_("Writing problem map..."));
        probbuf = prob;
    	bas_id = Rast_open_new(bas_name, CELL_TYPE);
    	for (i = 0; i < nrows; i++) {
            memcpy(out_buf, probbuf, bufsz);
            probbuf += bufsz;
    	    Rast_put_row(bas_id, out_buf, CELL_TYPE);
    	}
    	Rast_close(bas_id);
    }

    G_important_message(_("Writing filled and directions maps..."));
    for (i = 0; i < nrows; i++) {
        G_percent(i, nrows, 5);
        
        memcpy(in_buf, elevbuf, bnd.sz);
        elevbuf += bnd.sz;
        put_row(new_id, in_buf);

        memcpy(out_buf, dirsbuf, bufsz);
        dirsbuf += bufsz;
        for (j = 0; j < ncols; j += 1)
    	   out_buf[j] = dir_type(type, out_buf[j]);
    	Rast_put_row(dir_id, out_buf, CELL_TYPE);
    }
    G_percent(1, 1, 1);

    // Copy color table from input.
    Rast_write_colors(new_map_name, G_mapset(), &colors);

    // Close up the rasters and unmap the memory.
    Rast_close(new_id);    
    Rast_close(dir_id);

    deallocate(elev, elevsize, dirs, dirsize, prob, probsize, flag2);

    G_free(in_buf);
    G_free(out_buf);

    exit(EXIT_SUCCESS);
}

static int dir_type(int type, int dir)
{
    if (type == 1) {		/* AGNPS aspect format */
	if (dir == 128)
	    return (1);
	else if (dir == 1)
	    return (2);
	else if (dir == 2)
	    return (3);
	else if (dir == 4)
	    return (4);
	else if (dir == 8)
	    return (5);
	else if (dir == 16)
	    return (6);
	else if (dir == 32)
	    return (7);
	else if (dir == 64)
	    return (8);
	else
	    return (dir);
    }

    else if (type == 2) {	/* ANSWERS aspect format */
	if (dir == 128)
	    return (90);
	else if (dir == 1)
	    return (45);
	else if (dir == 2)
	    return (360);
	else if (dir == 4)
	    return (315);
	else if (dir == 8)
	    return (270);
	else if (dir == 16)
	    return (225);
	else if (dir == 32)
	    return (180);
	else if (dir == 64)
	    return (135);
	else
	    return (dir);
    }

    else {			/* [new] GRASS aspect format */
	if (dir == 128)
	    return (90);
	else if (dir == 1)
	    return (45);
	else if (dir == 2)
	    return (360);
	else if (dir == 4)
	    return (315);
	else if (dir == 8)
	    return (270);
	else if (dir == 16)
	    return (225);
	else if (dir == 32)
	    return (180);
	else if (dir == 64)
	    return (135);
	else
	    return (dir);
    }

}
