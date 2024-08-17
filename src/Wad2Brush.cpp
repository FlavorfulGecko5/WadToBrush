
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

		// Create this for later use
		SimpleLineDef simple;
		simple.v0 = level.verts[line.vertexStart];
		simple.v1 = level.verts[line.vertexEnd];
		frontSector.lines.push_back(simple);

		if (line.sideBack == NO_SIDEDEF) {
			WallBrush wall(v0, v1, level.minHeight, level.maxHeight, brushHandle++); 
			wall.ToString(writer);
		} else {
			SideDef& backSide = level.sidedefs[line.sideBack];
			Sector& backSector = level.sectors[backSide.sector];
			backSector.lines.push_back(simple);

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
	}

	// STEP TWO: FLOOR AND CEILING BRUSHES....

	for (int sectorIndex = 0; sectorIndex < level.sectors.Num(); sectorIndex++) {
		Sector& sector = level.sectors[sectorIndex];
		
		//std::cout << "Sorting Sector " << sectorIndex << " with " << sector.lines.size() << " Linedefs\n";

		// Assemble Data
		std::vector<SimpleLineDef>& unsorted = sector.lines;
		std::vector<SimpleLineDef> sorted;
		sorted.reserve(sector.lines.size());

		// Sort first linedef
		sorted.push_back(unsorted[0]);
		unsorted.erase(unsorted.begin());

		bool forceBreak = false;
		VertexFloat tempVertex;

		while (!unsorted.empty() && !forceBreak) {
			SimpleLineDef firstSorted = sorted[0];
			SimpleLineDef lastSorted = sorted[sorted.size() - 1];

			
			for(int i = 0, max = unsorted.size(); i < max; i++) {
				SimpleLineDef current = unsorted[i];
				if (current.v0 == firstSorted.v0) { // TODO: CONSOLIDATE
					current.swapOrder();
					sorted.insert(sorted.begin(), current);
					unsorted.erase(unsorted.begin() + i);
					goto LABEL_SKIP_FORCEBREAK;
				} else if (current.v1 == firstSorted.v0) {
					sorted.insert(sorted.begin(), current);
					unsorted.erase(unsorted.begin() + i);
					goto LABEL_SKIP_FORCEBREAK;
				} else if (current.v0 == lastSorted.v1) {
					sorted.push_back(current);
					unsorted.erase(unsorted.begin() + i);
					goto LABEL_SKIP_FORCEBREAK;
				} else if (current.v1 == lastSorted.v1) {
					current.swapOrder();
					sorted.push_back(current);
					unsorted.erase(unsorted.begin() + i);
					goto LABEL_SKIP_FORCEBREAK;
				}
				//std::cout << "----\n";
				//std::cout << current.v0.x << " " << current.v0.y << " " << current.v1.x << " " << current.v1.y << "\n";
				//std::cout << firstSorted.v0.x << " " << firstSorted.v0.y << " " << firstSorted.v1.x << " " << firstSorted.v1.y << "\n";
				//std::cout << lastSorted.v0.x << " " << lastSorted.v0.y << " " << lastSorted.v1.x << " " << lastSorted.v1.y << "\n";
				//std::cout << "NOTHINGFOUND\n";
			}
			forceBreak = true; // For polygons with holes
			LABEL_SKIP_FORCEBREAK:;
		}

		if (forceBreak) {
			std::cout << "Unable to generate floors/ceilings for Sector " << sectorIndex << "\n";
			continue;
		}

		//for (SimpleLineDef s : sorted) {
		//	std::cout << "[ (" << s.v0.x << ", " << s.v0.y << ") (" << s.v1.x << ", " << s.v1.y << " )] ";
		//}
		//if (!unsorted.empty()) std::cout << "UNSORTED IS NOT EMPTY\n";
		//std::cout << "\n";

		// Execute EarCut
		{
			typedef std::array<float, 2> Point;
			std::vector<std::vector<Point>> polylines;
			polylines.emplace_back();
			std::vector<Point>& mainLine = polylines[0];

			for(SimpleLineDef s : sorted) // Each consecutive sorted linedef shares a end/beginning point
				mainLine.push_back({s.v0.x, s.v0.y});

			//std::cout << mainLine.size() << "\n";
			std::vector<int16_t> triangleIndices = mapbox::earcut<int16_t>(polylines);
			//std::cout << triangleIndices.size() << "\n";

			for (int i = 0, max = triangleIndices.size(); i < max;) {
				VertexFloat a(mainLine[triangleIndices[i++]]);
				VertexFloat b(mainLine[triangleIndices[i++]]);
				VertexFloat c(mainLine[triangleIndices[i++]]);

				FloorBrush floor(a, b, c, sector.floorHeight - 1, sector.floorHeight, brushHandle++);
				FloorBrush ceiling(a, b, c, sector.ceilHeight, sector.ceilHeight + 1, brushHandle++);
				floor.ToString(writer);
				ceiling.ToString(writer);
			}
		}

	}


	//	FINISH UP
	writer << "\n}";
	std::string output = writer.str();
	std::string name;
	name.append(level.lumpHeader->name.Data());
	name.append(".map");

	std::cout << "Creating output file .\\" << name << "\n";
	std::ofstream file(name, std::ios_base::binary);
	file.write(output.data(), output.length());
	file.close();
}

int main(int argc, char* argv[]) {
	using namespace std;
	cout << "WadToBrush by FlavorfulGecko5 - EARLY ALPHA\n\n";
	if (argc < 3) {
		cout << "Usage: ./wadtobrush.exe [WAD] [Map] [XY Downscale] [Z Downscale] [X Shift] [Y Shift]\n\n"
			<< "[WAD] - Path to the .WAD file containing your level\n"
			<< "[Map] - Name of the Map Header Lump (i.e. \"E1M1\" or \"MAP01\") (Case Sensitive)\n"
			<< "[XY Downscale] - Map geometry will be horizontally downsized by this scale factor. Recommend at least a value of 10.\n"
			<< "[Z Downscale] - Map geometry will be vertically downsized by this scale factor. Recommend at least a value of 10.\n"
			<< "[X Shift] - Map geometry will be shifted this many X units. Use if your map is built far away from the origin.\n"
			<< "[Y Shift] - Map geometry will be shifted this many Y units. Use if your map is built far away from the origin.\n";
			return 0;
	}
	cout << "If you do not see a \"SUCCESS\" message after some time, this program has likely failed.\n";
	Wad doomWad(argv[1]);


	VertexTransforms transformations;
	if(argc > 3) transformations.xyDownscale = atof(argv[3]);
	if(argc > 4) transformations.zDownscale = atof(argv[4]);
	if(argc > 5) transformations.xShift = atof(argv[5]);
	if(argc > 6) transformations.yShift = atof(argv[6]);

	//VertexTransforms e1m1Transforms(-1024, 3680, 10.0f, 10.0f);
	WadLevel& e1m1 = doomWad.DecodeLevel(argv[2], transformations);
	BuildLevel(e1m1);

	//VertexTransforms e1m2Transforms(0, 0, 10.0f, 10.0f);
	//WadLevel& e1m2 = doomWad.DecodeLevel(1, e1m2Transforms);
	//BuildLevel(e1m2);

	//Wad doom2Wad("DOOM2.WAD");
	//WadLevel& map01 = doom2Wad.DecodeLevel(0, e1m2Transforms);
	//BuildLevel(map01);

	cout << "SUCCESS - Please remember that terrain generation is not fully complete, and some floors/ceilings may be missing.";
	return 0;
}