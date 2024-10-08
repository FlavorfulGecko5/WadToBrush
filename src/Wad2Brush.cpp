#include <vector>
#include "MapWriter.h"
#include <iostream>
#include <earcut.hpp>
#include <array>
#include <filesystem>


void BuildLevel(WadLevel& level) {
	MapWriter writer(level);

	// STEP 1: WALL BRUSHES
	for (int32_t i = 0; i < level.linedefs.Num(); i++) {	
		LineDef& line = level.linedefs[i];
		VertexFloat v0(level.verts[line.vertexStart]);
		VertexFloat v1(level.verts[line.vertexEnd]);
		bool upperUnpegged = line.flags & UPPER_UNPEGGED;
		bool lowerUnpegged = line.flags & LOWER_UNPEGGED;

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

		/*
		* Draw Height Rules:
		*
		* One Sided: Ceiling (Default) / Floor (Lower Unpegged)
		* Lower Textures: Highest Floor (Default) / Ceiling the side is facing (Lower Unpegged) 
			- WIKI IS INCORRECT: Falsely asserts Lower Unpegged draws it from the higher ceiling downward
		* Upper Textures: Lowest Ceiling (Default) / Highest Ceiling (Upper Unpegged)
		* Middle Textures:
		*	- Do not repeat vertically - we must modify the brush bounds to account for this
		*		- TODO: THIS QUIRK IS NOT YET IMPLEMENTED
		*	- Highest Ceiling (Default) / Highest Floor (Lower Unpegged)
		*
		* No need for any crazy vector projection when calculating drawheight, so we can simply add in the
		* vertical offset right now
		*/

		if (line.sideBack == NO_SIDEDEF) {
			float drawHeight = frontSide.offsetY + (lowerUnpegged ? frontSector.floorHeight : frontSector.ceilHeight);
			// level.minHeight, level.maxHeight
			writer.WriteWallBrush(v0, v1, frontSector.floorHeight, frontSector.ceilHeight, drawHeight, frontSide.middleTexture, frontSide.offsetX);
		} else {
			SideDef& backSide = level.sidedefs[line.sideBack];
			Sector& backSector = level.sectors[backSide.sector];
			backSector.lines.push_back(simple);

			// Texture pegging is based on the lowest/highest floor/ceiling - so we must distinguish
			// which values are smaller / larger - no way around this ugly chain of if statements unfortunately
			float lowerFloor, lowerCeiling, higherFloor, higherCeiling;
			if (frontSector.ceilHeight < backSector.ceilHeight) {
				lowerCeiling = frontSector.ceilHeight;
				higherCeiling = backSector.ceilHeight;
			} else {
				lowerCeiling = backSector.ceilHeight;
				higherCeiling = frontSector.ceilHeight;
			}
			if (frontSector.floorHeight < backSector.floorHeight) {
				lowerFloor = frontSector.floorHeight;
				higherFloor = backSector.floorHeight;
			} else {
				lowerFloor = backSector.floorHeight;
				higherFloor = frontSector.floorHeight;
			}

			// Brush the front sidedefs in relation to the back sector heights
			if (frontSide.lowerTexture != "-") {
				
				//float drawHeight = frontSide.offsetY + (lowerUnpegged ? higherCeiling : higherFloor);
				//float drawHeight = frontSide.offsetY + (lowerUnpegged ? frontSector.ceilHeight : frontSector.floorHeight);
				float drawHeight = frontSide.offsetY + (lowerUnpegged ? frontSector.ceilHeight : higherFloor);
				// level.minHeight, backSector.floorHeight
				writer.WriteWallBrush(v0, v1, frontSector.floorHeight, backSector.floorHeight, drawHeight, frontSide.lowerTexture, frontSide.offsetX);
			}
			if (frontSide.middleTexture != "-") {
				float drawHeight = frontSide.offsetY + (lowerUnpegged ? higherFloor : higherCeiling);
				writer.WriteWallBrush(v0, v1, backSector.floorHeight, backSector.ceilHeight, drawHeight, frontSide.middleTexture, frontSide.offsetX);
			}
			if (frontSide.upperTexture != "-") {
				float drawHeight = frontSide.offsetY + upperUnpegged ? higherCeiling : lowerCeiling;
				// backSector.ceilHeight, level.maxHeight
				writer.WriteWallBrush(v0, v1, backSector.ceilHeight, frontSector.ceilHeight, drawHeight, frontSide.upperTexture, frontSide.offsetX);
			}

			// Brush the back sidedefs in relation to the front sector heights
			// Technically this results in two overlapped brushes. However, this is rare
			// enough to not be considered an issue (yet) - back-textured surfaces mainly
			// appear to be windows
			// BUG FIXED: Must swap start/end vertices to ensure texture is drawn on correct face
			// and begins at correct position
			if (backSide.lowerTexture != "-") {
				//float drawHeight = backSide.offsetY + lowerUnpegged ? higherCeiling : higherFloor;
				//float drawHeight = backSide.offsetY + (lowerUnpegged ? backSector.ceilHeight : backSector.floorHeight);
				float drawHeight = backSide.offsetY + lowerUnpegged ? backSector.ceilHeight : higherFloor;
				// level.minHeight, frontSector.floorHeight
				writer.WriteWallBrush(v1, v0, backSector.floorHeight, frontSector.floorHeight, drawHeight, backSide.lowerTexture, backSide.offsetX);
			}
			if (backSide.middleTexture != "-") {
				float drawHeight = backSide.offsetY + (lowerUnpegged ? higherFloor : higherCeiling);
				writer.WriteWallBrush(v1, v0, frontSector.floorHeight, frontSector.ceilHeight, drawHeight, backSide.middleTexture, backSide.offsetX);
			}
			if (backSide.upperTexture != "-") {
				float drawHeight = backSide.offsetY + upperUnpegged ? higherCeiling : lowerCeiling;
				// frontSector.ceilHeight, level.maxHeight
				writer.WriteWallBrush(v1, v0, frontSector.ceilHeight, backSector.ceilHeight, drawHeight, backSide.upperTexture, backSide.offsetX);
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

				writer.WriteFloorBrush(a, b, c, sector.floorHeight, false, sector.floorTexture);
				writer.WriteFloorBrush(a, b, c, sector.ceilHeight, true, sector.ceilingTexture);
			}
		}

	}

	// FINISH UP
	writer.SaveFile(level.lumpHeader->name);
}

void DebugTextures() {
	Wad doomwad;
	doomwad.ReadFrom("DOOM.WAD");
	//doomwad.WriteLumpNames();
	doomwad.ExportTextures(true, true, true);
}

int main(int argc, char* argv[]) {
	#ifdef _DEBUG
	//DebugTextures();
	//return 0;
	#endif

	using namespace std;

	const char* helpMessage = 
R"(Usage: ./wadtobrush.exe [WAD] [Map] [XY Downscale] [Z Downscale] [X Shift] [Y Shift]

[WAD] - Path to the .WAD file containing your level
[Map] - Name of the Map Header Lump (i.e. "E1M1" or "MAP01") (Case Sensitive)
[XY Downscale] - Map geometry will be horizontally downsized by this scale factor. Recommend at least a value of 10.
[Z Downscale] - Map geometry will be vertically downsized by this scale factor. Recommend at least a value of 10.
[X Shift] - Map geometry will be shifted this many X units. Use if your map is built far away from the origin.
[Y Shift] - Map geometry will be shifted this many Y units. Use if your map is built far away from the origin.

Input [WAD] with no other arguments to export a WAD's textures instead of a level
)";

	cout << "WadToBrush by FlavorfulGecko5 - ALPHA VERSION 2\n\n";
	if (argc < 2) {
		cout << helpMessage;
		return 0;
	}
	cout << "If you do not see a \"SUCCESS\" message after some time, this program has likely failed.\n";
	cout << "At this time, only the VANILLA DOOM WAD format is supported.\n\n";


	Wad doomWad;
	if(!doomWad.ReadFrom(argv[1])) {
		printf("ERROR READING WAD FILE\n");
		return 0;
	}
	printf("Successfully read WAD from file.\n");


	if (argc == 2) {
		cout << "No level input detected. WadToBrush will run in Export Textures Mode\n";
		std::filesystem::path outputDir = std::filesystem::absolute("base/");
		cout << "Files will be output to " << outputDir.string() << "\nThis may take some time\n\n";

		doomWad.ExportTextures(true, true, true);
		cout << "SUCCESS - Texture exporting completed.\n";
		return 0;
	}


	VertexTransforms transformations;
	if(argc > 3) transformations.xyDownscale = atof(argv[3]);
	if(argc > 4) transformations.zDownscale = atof(argv[4]);
	if(argc > 5) transformations.xShift = atof(argv[5]);
	if(argc > 6) transformations.yShift = atof(argv[6]);

	//VertexTransforms e1m1Transforms(-1024, 3680, 10.0f, 10.0f);
	WadLevel* level = doomWad.DecodeLevel(argv[2], transformations);

	if(level == nullptr) {
		printf("ERROR PARSING LEVEL DATA\n");
		return 0;
	}
	printf("Successfully parsed level data.\n-----\nPerforming Conversion\n");

	BuildLevel(*level);

	cout << "-----\nSUCCESS - Please remember that terrain generation is not fully complete, and some floors/ceilings may be missing.";
	return 0;
}