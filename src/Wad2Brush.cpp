
#include <fstream>
#include <vector>
#include "BrushBuilder.h"
#include <iostream>

const char* rootMap = 
"Version 7\n"
"HierarchyVersion 1\n"
"entity{\n"
"	entityDef world {\n"
"		inherit = \"worldspawn\";\n"
"		edit = {\n"
"		}\n"
"	}\n";


// TODO: MUST FIGURE OUT WHY SOME WALLS DON'T DRAW
// INVESTIGATING: POINT DELTA
int main() {

	Wad doomWad;
	doomWad.ReadFrom("DOOM.WAD");

	std::ostringstream writer;
	writer << rootMap;
	writer.precision(8);
	writer << std::fixed;

	WadLevel& e1m1 = doomWad.levels[0];

	// TODO: May need to split each linedef into two sets of brushes

	int brushHandle = 0;
	float minHeight = e1m1.minHeight / 10.0f;
	float maxHeight = e1m1.maxHeight / 10.0f;
	for (int32_t i = 0; i < e1m1.linedefs.Num(); i++) {
		LineDef& line = e1m1.linedefs[i];
		VertexFloat v0(e1m1.vertices[line.vertexStart]);
		VertexFloat v1(e1m1.vertices[line.vertexEnd]);
		v0.x -= 1024;
		v0.x /= 10;
		v0.y += 3680;
		v0.y /= 10;
		v1.x -= 1024;
		v1.x /= 10;
		v1.y += 3680;
		v1.y /= 10;

		// We assume linedefs can't have a back sidedef without a front
		if(line.sideFront == NO_SIDEDEF)
			continue;

		SideDef& frontSide = e1m1.sidedefs[line.sideFront];
		Sector& frontSector = e1m1.sectors[frontSide.sector];
		float frontFloor = frontSector.floorHeight / 10.0f;
		float frontCeiling = frontSector.ceilingHeight / 10.0f;

		if (line.sideBack == NO_SIDEDEF) {
			WallBrush wall(v0, v1, minHeight, maxHeight, brushHandle++); 
			wall.ToString(writer);
		} else {
			SideDef& backSide = e1m1.sidedefs[line.sideBack];
			Sector& backSector = e1m1.sectors[backSide.sector];
			float backFloor = backSector.floorHeight / 10.0f;
			float backCeiling = backSector.ceilingHeight / 10.0f;

			// Brush the front sidedefs in relation to the back sector heights
			if (frontSide.lowerTexture != "-") {
				WallBrush lower(v0, v1, minHeight, backFloor, brushHandle++);
				lower.ToString(writer);
			}
			if (frontSide.middleTexture != "-") {
				WallBrush middle(v0, v1, backFloor, backCeiling, brushHandle++);
				middle.ToString(writer);
			}
			if (frontSide.upperTexture != "-") {
				WallBrush upper(v0, v1, backCeiling, maxHeight, brushHandle++);
				upper.ToString(writer);
			}

			// Brush the back sidedefs in relation to the front sector heights
			// Technically this results in two overlapped brushes. However, this is rare
			// enough to not be considered an issue (yet) - back-textured surfaces mainly
			// appear to be windows
			if (backSide.lowerTexture != "-") {
				WallBrush lower(v0, v1, minHeight, frontFloor, brushHandle++);
				lower.ToString(writer);
			}
			if (backSide.middleTexture != "-") {
				WallBrush middle(v0, v1, frontFloor, frontCeiling, brushHandle++);
				middle.ToString(writer);
			}
			if (backSide.upperTexture != "-") {
				WallBrush upper(v0, v1, frontCeiling, maxHeight, brushHandle++);
				upper.ToString(writer);
			}
		}
	}
	writer << "\n}";

	std::string output = writer.str();

	std::ofstream file("e1m1.map", std::ios_base::binary);
	file.write(output.data(), output.length());
	file.close();

		
	return 0;
}