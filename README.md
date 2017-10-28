# r.fill.dir
Replacement for GRASS GIS (7.2.2, though this extension doesn't really change much) which hopefully fixes some bugs and runs faster on larger datasets.

The easiest way to install this is to download the GRASS source. Go to [grass]/raster and rename or remove the r.fill.dir folder. Then check out this repo in the same place. When you build GRASS, it should include this extension.

This version adds a flag for mapped memory (on Linux, possibly OSX) that lets the user choose between anonymous mapped memory and physical RAM. 
