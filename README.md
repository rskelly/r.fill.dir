# r.fill.dir
Replacement for GRASS GIS (7.2.2, though this extension doesn't really change much) which hopefully fixes some bugs and runs faster on larger datasets.

The easiest way to install this is to check out this repo in a usual place, then download the GRASS source. Go to [grass]/raster and rename or remove the r.fill.dir folder. Then create a link to this repo in the same place. When you build GRASS, it should include this extension.
