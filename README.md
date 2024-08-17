# WadToBrush
WadToBrush is a program for converting Classic Doom levels into Doom Eternal maps usable in idStudio. 

*THIS PROGRAM IS IN AN EARLY ALPHA STATE. CURRENTLY, IT CANNOT DO MORE THAN CONVERT MOST (NOT ALL) LEVEL GEOMETRY INTO UNTEXTURED BRUSHES.*

## Usage
Usage: `./wadtobrush.exe [WAD] [Map] [XY Downscale] [Z Downscale] [X Shift] [Y Shift]`
* `[WAD]` - Path to the .WAD file containing your level
* `[Map]` - Name of the Map Header Lump (i.e. "E1M1" or "MAP01") (Case Sensitive)
* `[XY Downscale]` - Map geometry will be horizontally downsized by this scale factor. Recommend at least a value of 10.
* `[Z Downscale]` - Map geometry will be vertically downsized by this scale factor. Recommend at least a value of 10.
* `[X Shift]` - Map geometry will be shifted this many X units. Use if your map is built far away from the origin.
* `[Y Shift]` - Map geometry will be shifted this many Y units. Use if your map is built far away from the origin.

Output: A `.map` file with the same name as the Map Header Lump (i.e. "E1M1.map" or "MAP01.map") 

## Contributing
WadToBrush is written in C++ using Visual Studio.

## Credits
* FlavorfulGecko5 - Author of WadToBrush
* Mapbox - Their [Earcut polygon triangulation algorithm](https://github.com/mapbox/earcut.hpp) is used to produce the floor and ceiling brushes.
