# WadToBrush
WadToBrush is a program for converting Classic Doom levels into Doom Eternal maps usable in idStudio. 

*THIS PROGRAM IS IN AN ALPHA STATE. CURRENTLY, IT CANNOT DO MORE THAN CONVERT MOST (NOT ALL) LEVEL GEOMETRY INTO TEXTURED BRUSHES AND EXPORT TEXTURES FROM WADS.*

*ONLY THE VANILLA DOOM WAD FORMAT IS CURRENTLY SUPPORTED. OTHER WAD FORMATS LIKE UDMF WILL NOT WORK AT THIS TIME*

## Usage
Usage: `./wadtobrush.exe [WAD] [Map] [XY Downscale] [Z Downscale] [X Shift] [Y Shift]`
* `[WAD]` - Path to the .WAD file containing your level
* `[Map]` - Name of the Map Header Lump (i.e. "E1M1" or "MAP01") (Case Sensitive)
* `[XY Downscale]` - Map geometry will be horizontally downsized by this scale factor. Recommend at least a value of 10.
* `[Z Downscale]` - Map geometry will be vertically downsized by this scale factor. Recommend at least a value of 10.
* `[X Shift]` - Map geometry will be shifted this many X units. Use if your map is built far away from the origin.
* `[Y Shift]` - Map geometry will be shifted this many Y units. Use if your map is built far away from the origin.

Output: A `.map` file with the same name as the Map Header Lump (i.e. "E1M1.map" or "MAP01.map") 

Input `[WAD]` with no other arguments to export the WAD's textures instead of a level. The texture images will be converted to `.tga` files and `material2 decls` will be generated for them.

## Contributing
WadToBrush is written in C++ using Visual Studio.

## Credits
* FlavorfulGecko5 - Author of WadToBrush
* Mapbox - Their [Earcut polygon triangulation algorithm](https://github.com/mapbox/earcut.hpp) is used to produce the floor and ceiling brushes.
* Maluoi - [tga.h](https://gist.github.com/maluoi/ade07688e741ab188841223b8ffeed22)