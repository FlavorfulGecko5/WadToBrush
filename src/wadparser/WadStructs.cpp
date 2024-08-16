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

bool WadLevel::ReadFrom(BinaryReader &reader, VertexTransforms transforms) {
	// Read Vertices
	reader.Goto(lumpVertex->offset);
	verts.Reserve(lumpVertex->size / VertexFloat::size());
	int16_t tempX = 0, tempY = 0;
	for (int32_t i = 0; i < verts.Num(); i++) {
		reader.ReadLE(tempX);
		reader.ReadLE(tempY);

		verts[i].x = (tempX + transforms.xShift) / transforms.xyDownscale;
		verts[i].y = (tempY + transforms.yShift) / transforms.xyDownscale;
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
		s.lowerTexture.ReadFrom(reader);
		s.middleTexture.ReadFrom(reader);
		reader.ReadLE(s.sector);
	}

	// Read Sectors
	reader.Goto(lumpSectors->offset);
	sectors.Reserve(lumpSectors->size / Sector::size());
	int16_t tempZ = 0;
	maxHeight = FLT_TRUE_MIN;
	minHeight = FLT_MAX;
	for (int32_t i = 0; i < sectors.Num(); i++) {
		Sector& s = sectors[i];
		reader.ReadLE(tempZ);
		s.floorHeight = tempZ / transforms.zDownscale;
		reader.ReadLE(tempZ);
		s.ceilHeight = tempZ / transforms.zDownscale;
		s.floorTexture.ReadFrom(reader);
		s.ceilingTexture.ReadFrom(reader);
		reader.ReadLE(s.lightLevel);
		reader.ReadLE(s.specialType);
		reader.ReadLE(s.tagNumber);

		if(s.floorHeight < minHeight)
			minHeight = s.floorHeight;
		if(s.ceilHeight > maxHeight)
			maxHeight = s.ceilHeight;
	}


	return true;
}

void WadLevel::Debug() {
	using namespace std;
	cout << lumpHeader->name.Data() << "\n";
	cout << "Vertex Count: " << verts.Num() << "\n";
	cout << "LineDef Count: " << linedefs.Num() << "\n";
	cout << "SideDef Count: " << sidedefs.Num() << "\n";
	cout << "Sector Count: " << sectors.Num() << "\n";
}

Wad::Wad(const char* wadpath) : reader(wadpath) {
	if (!reader.InitSuccessful())
		return;

	lumptable.ReadFrom(reader);

	// Initiate Level Array and populate lump references
	levels.Reserve(lumptable.levelCount);
	WadArray<LumpEntry>& lumps = lumptable.lumps;
	for (int32_t i = 0, lvlNum = 0; i < lumps.Num(); i++) {
		if (lumps[i].type != LumpType::MAP_HEADER)
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
}

WadLevel& Wad::DecodeLevel(int index, VertexTransforms transforms) {
	levels[index].ReadFrom(reader, transforms);
	return levels[index];
}