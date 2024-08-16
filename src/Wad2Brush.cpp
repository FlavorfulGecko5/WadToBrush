
#include <fstream>
#include <vector>
#include "BrushBuilder.h"
#include <iostream>
#include <earcut.hpp>
#include <array>

const char* rootMap = 
"Version 7\n"
"HierarchyVersion 1\n"
"entity{\n"
"	entityDef world {\n"
"		inherit = \"worldspawn\";\n"
"		edit = {\n"
"		}\n"
"	}\n";

void BuildLevel(WadLevel& level) {
	std::ostringstream writer;
	writer << rootMap;
	writer.precision(8);
	writer << std::fixed;

	int brushHandle = 0;

	for (int32_t i = 0; i < level.linedefs.Num(); i++) {
		LineDef& line = level.linedefs[i];
		VertexFloat v0(level.verts[line.vertexStart]);
		VertexFloat v1(level.verts[line.vertexEnd]);

		// We assume linedefs can't have a back sidedef without a front
		if(line.sideFront == NO_SIDEDEF)
			continue;

		SideDef& frontSide = level.sidedefs[line.sideFront];
		Sector& frontSector = level.sectors[frontSide.sector];

		if (line.sideBack == NO_SIDEDEF) {
			WallBrush wall(v0, v1, level.minHeight, level.maxHeight, brushHandle++); 
			wall.ToString(writer);
		} else {
			SideDef& backSide = level.sidedefs[line.sideBack];
			Sector& backSector = level.sectors[backSide.sector];

			// Brush the front sidedefs in relation to the back sector heights
			if (frontSide.lowerTexture != "-") {
				WallBrush lower(v0, v1, level.minHeight, backSector.floorHeight, brushHandle++);
				lower.ToString(writer);
			}
			if (frontSide.middleTexture != "-") {
				WallBrush middle(v0, v1, backSector.floorHeight, backSector.ceilHeight, brushHandle++);
				middle.ToString(writer);
			}
			if (frontSide.upperTexture != "-") {
				WallBrush upper(v0, v1, backSector.ceilHeight, level.maxHeight, brushHandle++);
				upper.ToString(writer);
			}

			// Brush the back sidedefs in relation to the front sector heights
			// Technically this results in two overlapped brushes. However, this is rare
			// enough to not be considered an issue (yet) - back-textured surfaces mainly
			// appear to be windows
			if (backSide.lowerTexture != "-") {
				WallBrush lower(v0, v1, level.minHeight, frontSector.floorHeight, brushHandle++);
				lower.ToString(writer);
			}
			if (backSide.middleTexture != "-") {
				WallBrush middle(v0, v1, frontSector.floorHeight, frontSector.ceilHeight, brushHandle++);
				middle.ToString(writer);
			}
			if (backSide.upperTexture != "-") {
				WallBrush upper(v0, v1, frontSector.ceilHeight, level.maxHeight, brushHandle++);
				upper.ToString(writer);
			}
		}
		//SimpleLineDef simple;
		//simple.v0 = e1m1.vertices[line.vertexStart];
		//simple.v1 = e1m1.vertices[line.vertexEnd];
		//frontSector.lines.push_back(simple);
	}

	// STEP TWO: FLOOR AND CEILING BRUSHES....

	using Point = std::array<double, 2>;
	std::vector<std::vector<Point>> test;
	mapbox::earcut<int32_t>(test);


	//	FINISH UP
	writer << "\n}";
	std::string output = writer.str();
	std::string name;
	name.append(level.lumpHeader->name.Data());
	name.append(".map");

	std::ofstream file("e1m1.map", std::ios_base::binary);
	file.write(output.data(), output.length());
	file.close();
}

// TODO: MUST FIGURE OUT WHY SOME WALLS DON'T DRAW
// INVESTIGATING: POINT DELTA
int main() {

	Wad doomWad("DOOM.WAD");

	VertexTransforms e1m1Transforms(-1024, 3680, 10.0f, 10.0f);
	WadLevel& e1m1 = doomWad.DecodeLevel(0, e1m1Transforms);
	BuildLevel(e1m1);

	return 0;
}