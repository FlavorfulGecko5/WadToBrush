#include <fstream>
#include "MapWriter.h"

const char* rootMap = 
"Version 7\n"
"HierarchyVersion 1\n"
"entity{\n"
"	entityDef world {\n"
"		inherit = \"worldspawn\";\n"
"		edit = {\n"
"		}\n"
"	}\n";

MapWriter::MapWriter(WadLevel& level) : tforms(level.transforms) {
	printf("%f %f %f %f", tforms.xyDownscale, tforms.zDownscale, tforms.xShift, tforms.yShift);
	writer.precision(8);
	writer << std::fixed << rootMap;
}

void MapWriter::SaveFile(WadString levelName) {
	// Close entity
	writer << "\n}";

	std::string output = writer.str();
	std::string fileName;
	fileName.append(levelName);
	fileName.append(".map");
	
	printf("Creating output file %s\n", fileName.data());
	std::ofstream file(fileName, std::ios_base::binary);
	file.write(output.data(), output.length());
	file.close();
}

void MapWriter::WritePlane(const Plane p) {
	writer << "( " << p.n.x << ' ' << p.n.y << ' ' << p.n.z << ' ' << (p.d * -1) << " ) " 
		<< "( ( 1 0 0 ) ( 0 1 0 ) ) \"art/tile/common/shadow_caster\" 0 0 0";
}

void MapWriter::WriteBrush(const WallBrush& b) {
	writer << "{\n\thandle = " << brushHandle++ << "\n\tbrushDef3 {";
	for (int i = 0; i < 6; i++) {
		writer << "\n\t\t";
		WritePlane(b.bounds[i]);
	}
	writer << "\n\t}\n}\n";
}

void MapWriter::WriteBrush(const FloorBrush& b) {
	writer << "{\n\thandle = " << brushHandle++ << "\n\tbrushDef3 {";
	for (int i = 0; i < 4; i++) {
		writer << "\n\t\t";
		WritePlane(b.bounds[i]);
	}
	writer << "\n\t\t";

	// Flats are always 64x64, allowing us to use 1 / 64 as a constant (in classic Doom, 1 unit is 1 pixel)
	// horizontal: (0, -1) Vertical (1, 0) - Ensures proper rotation of textures
	float scaleMagnitude = 0.015625f * tforms.xyDownscale;
	const float horizontalOffset = tforms.xShift * -0.015625f; // xShift / xyDownscale * xyDownscale * 0.015625f * -1
	const float verticalOffset = tforms.yShift * 0.015625f;    // Signs are flipped for right/left shift, but not for up/down shift

	writer << "( " << b.texturedBound.n.x << ' ' << b.texturedBound.n.y << ' ' << b.texturedBound.n.z << ' ' << (b.texturedBound.d * -1) << " ) ";
	writer << "( ( 0 " << (scaleMagnitude) << " " << horizontalOffset << " ) ( " << (scaleMagnitude * -1) << " 0 " << verticalOffset << " ) ) \"art/wadtobrush/flats/" << b.texture.Data() << "\" 0 0 0";
	writer << "\n\t}\n}\n";
}