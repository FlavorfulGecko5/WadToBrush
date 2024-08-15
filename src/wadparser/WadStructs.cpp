#include "WadStructs.h"
#include <iostream>

bool LumpTable::ReadFrom(BinaryReader& reader) {
	// Clear previous state
	levelCount = 0;
	reader.Goto(0);

	// Read WAD Magic - Only IWAD will be readable for now
	const size_t SIZE_MAGIC = 4;
	char magic[SIZE_MAGIC];
	reader.ReadBytes(magic, SIZE_MAGIC);
	if(memcmp(magic, "IWAD", SIZE_MAGIC) != 0)
		return false;

	// Read Table Metadata
	lumps.ReserveFrom(reader);
	reader.ReadLE(tableOffset);

	// Read Each Table Entry
	reader.Goto(tableOffset);
	for (int32_t i = 0; i < lumps.Num(); i++) {
		LumpEntry& lump = lumps[i];
		reader.ReadLE(lump.offset);
		reader.ReadLE(lump.size);
		lump.name.ReadFrom(reader);
	}

	// Identify map header lumps
	for (int32_t i = 0; i < lumps.Num(); i++) {
		if (lumps[i].name == "THINGS") {
			lumps[i - 1].type = LumpType::MAP_HEADER;
			levelCount++;
		}
	}

	return true;
}

bool WadLevel::ReadFrom(BinaryReader &reader) {
	// Read Vertices
	reader.Goto(lumpVertex->offset);
	vertices.Reserve(lumpVertex->size / Vertex::size());
	for (int32_t i = 0; i < vertices.Num(); i++) {
		reader.ReadLE(vertices[i].x);
		reader.ReadLE(vertices[i].y);
	}

	// Read LineDefs
	reader.Goto(lumpLines->offset);
	linedefs.Reserve(lumpLines->size / LineDef::size());
	for (int32_t i = 0; i < linedefs.Num(); i++) {
		LineDef& d = linedefs[i];
		reader.ReadLE(d.vertexStart);
		reader.ReadLE(d.vertexEnd);
		reader.ReadLE(d.flags);
		reader.ReadLE(d.specialType);
		reader.ReadLE(d.sectorTag);
		reader.ReadLE(d.sideFront);
		reader.ReadLE(d.sideBack);
	}

	// Read SideDefs
	reader.Goto(lumpSides->offset);
	sidedefs.Reserve(lumpSides->size / SideDef::size());
	for (int32_t i = 0; i < sidedefs.Num(); i++) {
		SideDef& s = sidedefs[i];
		reader.ReadLE(s.texXOffset);
		reader.ReadLE(s.texYOffset);
		s.upperTexture.ReadFrom(reader);
		s.middleTexture.ReadFrom(reader);
		s.lowerTexture.ReadFrom(reader);
		reader.ReadLE(s.sector);
	}

	// Read Sectors
	reader.Goto(lumpSectors->offset);
	sectors.Reserve(lumpSectors->size / Sector::size());
	for (int32_t i = 0; i < sectors.Num(); i++) {
		Sector& s = sectors[i];
		reader.ReadLE(s.floorHeight);
		reader.ReadLE(s.ceilingHeight);
		s.floorTexture.ReadFrom(reader);
		s.ceilingTexture.ReadFrom(reader);
		reader.ReadLE(s.lightLevel);
		reader.ReadLE(s.specialType);
		reader.ReadLE(s.tagNumber);
	}


	return true;
}

void WadLevel::Debug() {
	using namespace std;
	cout << lumpHeader->name.Data() << "\n";
	cout << "Vertex Count: " << vertices.Num() << "\n";
	cout << "LineDef Count: " << linedefs.Num() << "\n";
	cout << "SideDef Count: " << sidedefs.Num() << "\n";
	cout << "Sector Count: " << sectors.Num() << "\n";
}

bool Wad::ReadFrom(const char* wadpath) {
	BinaryReader reader(wadpath);
	if(!reader.InitSuccessful())
		return false;

	lumptable.ReadFrom(reader);

	// Initiate Level Array and populate lump references
	levels.Reserve(lumptable.levelCount);
	WadArray<LumpEntry>& lumps = lumptable.lumps;
	for (int32_t i = 0, lvlNum = 0; i < lumps.Num(); i++) {
		if(lumps[i].type != LumpType::MAP_HEADER)
			continue;

		levels[lvlNum].lumpHeader = &lumps[i++];
		levels[lvlNum].lumpThings = &lumps[i++];
		levels[lvlNum].lumpLines = &lumps[i++];
		levels[lvlNum].lumpSides = &lumps[i++];
		levels[lvlNum].lumpVertex = &lumps[i++];
		i += 3; // Skipping segments, subsectors, and nodes
		levels[lvlNum].lumpSectors = &lumps[i];
		lvlNum++;
	}

	// UNCOMMENT ME AFTER TESTING
	// Build data structures for levels
	//for (int32_t i = 0; i < levels.Num(); i++)
	//	levels[i].ReadFrom(reader);

	levels[0].ReadFrom(reader);
	//levels[0].Debug();

	return true;
}