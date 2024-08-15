
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

	for (int32_t i = 0; i < e1m1.linedefs.Num(); i++) {
		if(i % 25 == 0)
			std::cout << "LineDef Number " << i << "\n";

		LineDef& line = e1m1.linedefs[i];
		VertexFloat v0(e1m1.vertices[line.vertexStart]);
		v0.x -= 1024;
		v0.x /= 10;
		v0.y += 3680;
		v0.y /= 10;
		VertexFloat v1(e1m1.vertices[line.vertexEnd]);
		v1.x -= 1024;
		v1.x /= 10;
		v1.y += 3680;
		v1.y /= 10;

		WallBrush brush(v0, v1, i);
		brush.ToString(writer);
	}
	writer << "\n}";

	std::string output = writer.str();

	std::ofstream file("e1m1.map", std::ios_base::binary);
	file.write(output.data(), output.length());
	file.close();

		
	return 0;
}