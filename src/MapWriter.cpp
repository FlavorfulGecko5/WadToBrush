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

MapWriter::MapWriter(WadLevel& level) : tforms(level.transforms), wallRatios(level.metersPerPixel),
	flatScale(0.015625f * tforms.xyDownscale), //Flats are always 64x64, allowing us to use 1 / 64 as a constant
	flatXShift(-0.015625f * tforms.xShift),    // Full formula is xShift / xyDownscale * xyDownscale * 0.015625f * -1
	flatYShift(0.015625f * tforms.yShift)      // Sign is flipped for right/left shift, but not for up/down shift
{
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
	writer << "( " << p.n.x << ' ' << p.n.y << ' ' << p.n.z << ' ' << -p.d
		<< " ) ( ( 1 0 0 ) ( 0 1 0 ) ) \"art/tile/common/shadow_caster\" 0 0 0";
}

void MapWriter::BeginBrushDef() {
	writer << "{\n\thandle = " << brushHandle++ << "\n\tbrushDef3 {";
}

void MapWriter::EndBrushDef() {
	writer << "\n\t}\n}\n";
}

void MapWriter::WriteWallBrush(VertexFloat v0, VertexFloat v1, float minHeight, float maxHeight, float drawHeight, WadString texture, float offsetX) {
	Plane bounds[5]; // Untextured surfaces
	Plane surface;   // Texture surface
	Vector horizontal(v0, v1);

	// PART 1 - CONSTRUCT THE PLANES
	surface.SetFrom(Vector(horizontal.y, -horizontal.x, 0), v1);

	// Plane 0 - The "Back" SideDef to the LineDef's left
	bounds[0].n.x = -surface.n.x;
	bounds[0].n.y = -surface.n.y;
	bounds[0].n.z = 0;

	VertexFloat d0(bounds[0].n.x * 0.0075f + v0.x, bounds[0].n.y * 0.0075f + v0.y);
	VertexFloat d1(bounds[0].n.x * 0.0075f + v1.x, bounds[0].n.y * 0.0075f + v1.y);
	bounds[0].d = bounds[0].n.x * d1.x + bounds[0].n.y * d1.y;
	
	// Plane 1: Forward Border Sliver: d1 - v1
	Vector deltaVector = Vector(v1, d1);
	bounds[1].SetFrom(Vector(deltaVector.y, -deltaVector.x, 0), d1);

	// Plane 2: Rear Border Sliver: v0 - d0
	bounds[2].n.x = -bounds[1].n.x;
	bounds[2].n.y = -bounds[1].n.y;
	bounds[2].n.z = 0;
	bounds[2].d = bounds[2].n.x * v0.x + bounds[2].n.y * v0.y;

	// Plane 3: Upper Bound:
	bounds[3].n = Vector(0, 0, 1);
	bounds[3].d = maxHeight;

	// Plane 4: Lower Bound
	bounds[4].n = Vector(0, 0, -1);
	bounds[4].d = minHeight * -1;


	// PART 2: DRAW THE SURFACE
	BeginBrushDef();
	
	// Write untextured bounds
	for (int i = 0; i < 5; i++) {
		writer << "\n\t\t";
		WritePlane(bounds[i]);
	}

	// Write Textured surface
	// REMOVED: TEST IF TEXTURE DOES NOT EXIST, draw as regular plane if it doesn't
	writer << "\n\t\t";

	DimFloat ratios = wallRatios[texture];
	float xScale = ratios.width * tforms.xyDownscale;
	float yScale = ratios.height * tforms.zDownscale;

	/*
	* We must shift the texture grid such that the origin is centered on
	* the wall's left vertex. To do this accurately, we calculate the magnitude
	* of the projection of the shift vector onto the horizontal wall vector.
	* We finalize this by adding the texture X offset to this value.
	* The math works out such that the XY downscale cancels in both terms when
	* the texture's X scale is multiplied in at the end.
	*/
	float projection = ((horizontal.x * v0.x + horizontal.y * v0.y) / horizontal.Magnitude() - offsetX) * xScale * -1;


	writer << "( " << surface.n.x << ' ' << surface.n.y << ' ' << surface.n.z << ' ' << -surface.d << " ) ";
	writer << "( ( " << xScale << " 0 " << projection << " ) ( 0 " << yScale << " " << drawHeight * yScale << " ) ) \"art/wadtobrush/walls/" << texture.Data() << "\" 0 0 0";
	EndBrushDef();
}

void MapWriter::WriteFloorBrush(VertexFloat a, VertexFloat b, VertexFloat c, float height, bool isCeiling, WadString texture) {
	Plane bounds[4]; // Untextured surfaces
	Plane surface;   // Textured surface.

	// PART 1 - CONSTRUCT PLANE OBJECTS
	// Based on the order Earcut yields in the points in,
	// we cross horizontal X <0, 0, 1> to get our normal

	// Plane 0 - First Wall
	Vector h(a, b);
	bounds[0].SetFrom(Vector(h.y, -h.x, 0), a);

	// Plane 1 - Second Wall
	h = Vector(b, c);
	bounds[1].SetFrom(Vector(h.y, -h.x, 0), b);

	// Plane 2 - Last Wall
	h = Vector(c, a);
	bounds[2].SetFrom(Vector(h.y, -h.x, 0), c);

	if (isCeiling) {
		bounds[3].n = Vector(0, 0, 1);
		bounds[3].d = height + 0.0075f;
		surface.n = Vector(0, 0, -1);
		surface.d = -height;
	} 
	else {
		surface.n = Vector(0, 0, 1);
		surface.d = height;
		bounds[3].n = Vector(0, 0, -1);
		bounds[3].d = 0.0075f - height;
	}

	// PART 2: DRAW THE SURFACE
	BeginBrushDef();
	for (int i = 0; i < 4; i++) {
		writer << "\n\t\t";
		WritePlane(bounds[i]);
	}
	writer << "\n\t\t";

	// horizontal: (0, -1) Vertical (1, 0) - Ensures proper rotation of textures (for floors)
	writer << "( " << surface.n.x << ' ' << surface.n.y << ' ' << surface.n.z << ' ' << -surface.d << " ) ";
	writer << "( ( 0 " <<  (isCeiling ? -flatScale : flatScale) << " " << flatXShift << " ) ( " << -flatScale << " 0 " << flatYShift
		<< " ) ) \"art/wadtobrush/flats/" << texture.Data() << "\" 0 0 0";

	EndBrushDef();
}