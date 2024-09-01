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

MapWriter::MapWriter(WadLevel& level) : tforms(level.transforms), wallRatios(level.metersPerPixel) {
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
	writer << "{\n\thandle = " << brushHandle++ << "\n\tbrushDef3 {";\
	
	// Write untextured bounds
	for (int i = 1; i < 6; i++) {
		writer << "\n\t\t";
		WritePlane(b.bounds[i]);
	}
	

	// Write front wall
	// TODO: CONSIDER OFFSETS
	writer << "\n\t\t";
	if (b.texture == "-")
		WritePlane(b.bounds[0]);
	else {
		DimFloat ratios = wallRatios[b.texture];
		float xScale = ratios.width * tforms.xyDownscale;
		float yScale = ratios.height * tforms.zDownscale;


		/*
		* We must shift the texture grid such that the texture patch begins on the 
		* left side of the wall. To do this accurately, we calculate the magnitude
		* of the projection of the shift vector onto the horizontal wall vector
		* 
		* Currently, this accounts for all vertextransforms and sidedef X shift values, but doesn't yet account for linedef flags
		*/
		Vector wallVector(b.p0, b.p1);
		Vector shiftVector(VertexFloat(0.0f, 0.0f), b.p0);
		float projection = ((wallVector.x * shiftVector.x + wallVector.y * shiftVector.y) / wallVector.Magnitude() - b.offsetX) * xScale * -1;

		writer << "( " << b.bounds[0].n.x << ' ' << b.bounds[0].n.y << ' ' << b.bounds[0].n.z << ' ' << (b.bounds[0].d * -1) << " ) ";
		writer << "( ( " << xScale << " 0 " << projection << " ) ( 0 " << yScale << " " << 0 << " ) ) \"art/wadtobrush/walls/" << b.texture.Data() << "\" 0 0 0";
	}
	writer << "\n\t}\n}\n";
}

// TODO MUST FIX: CEILING BRUSHES ARE NOT ROTATED PROPERLY
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