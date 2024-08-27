#include "WadStructs.h"
#include <tga.h>
#include <iostream>
#include <filesystem>
#include <fstream>

bool LumpTable::ReadFrom(BinaryReader& reader) {
	// Clear previous state
	levelCount = 0;
	reader.Goto(0);

	// Read WAD Magic - Only IWAD will be readable for now
	const size_t SIZE_MAGIC = 4;
	char magic[SIZE_MAGIC];
	reader.ReadBytes(magic, SIZE_MAGIC);
	if (memcmp(magic, "IWAD", SIZE_MAGIC) != 0 && memcmp(magic, "PWAD", SIZE_MAGIC) != 0) {
		printf("File must be an IWAD or a PWAD\n");
		return false;
	}

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

	xyDownscale = transforms.xyDownscale;
	return true;
}

void WadLevel::Debug() {
	printf(lumpHeader->name.Data());
	printf("\nVertex Count: %i\n", verts.Num());
	printf("LineDef Count: %i\n", linedefs.Num());
	printf("SideDef Count: %i\n", sidedefs.Num());
	printf("Sector Count: %i\n", sectors.Num());
}

bool Wad::ReadFrom(const char* wadpath) {
	if (!reader.SetBuffer(wadpath)) {
		printf("Failed to read file from disk\n");
		return false;
	}

	if(!lumptable.ReadFrom(reader))
		return false;

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

	return true;
}

WadLevel* Wad::DecodeLevel(const char* name, VertexTransforms transforms) {
	for (int32_t i = 0; i < levels.Num(); i++)
		if (levels[i].lumpHeader->name == name) {
			levels[i].ReadFrom(reader, transforms);
			return &levels[i];
		}

	printf("Failed to find level with specified name.\n");
	return nullptr;
}

void Wad::ExportTextures() {
	const char* mat2_start = 
"declType( material2 ) {\n"
"	inherit = \"template/pbr\";\n"
"	edit = {\n"
"		RenderLayers = {\n"
"			item[0] = {\n"
"				parms = {\n"
"					smoothness = {\n"
"						filePath = \"art/wadtobrush/black.tga\";\n"
"					}\n"
"					specular = {\n"
"						filePath = \"art/wadtobrush/black.tga\";\n"
"					}\n"
"					albedo = {\n"
"						filePath = \"";

	const char* mat2_end = "\";\n"
"					}\n"
"				}\n"
"			}\n"
"		}\n"
"	}\n"
"}";

	const char* dir_flats = "base/art/wadtobrush/flats/";
	const char* dir_flats_mat = "base/declTree/material2/art/wadtobrush/flats/";

	struct Color { // This BGRA variable order is what the TGA writer expects
		uint8_t b = 0; 
		uint8_t g = 0;
		uint8_t r = 0;
		uint8_t a = 0;
	};

	const int32_t numColors = 256;

	Color palette[numColors];

	const int32_t flatSize = 4096;

	WadArray<LumpEntry>& lumps = lumptable.lumps;

	// Create directories
	std::filesystem::create_directories(dir_flats);
	std::filesystem::create_directories(dir_flats_mat);
	
	// Read First PlayPal Palette
	for (int i = 0; i < lumps.Num(); i++) {
		if (lumps[i].name == "PLAYPAL") {
			reader.Goto(lumps[i].offset);
			for (int c = 0; c < numColors; c++) {
				reader.ReadLE(palette[c].r);
				reader.ReadLE(palette[c].g);
				reader.ReadLE(palette[c].b);
				palette[c].a = 255;
			}
			break;
		}
	}

	// Generate black texture
	{
		const size_t size_black = 256;
		Color black[size_black];

		for (int i = 0; i < size_black; i++) {
			black[i].r = 0;
			black[i].g = 0;
			black[i].b = 0;
			black[i].a = 255;
		}
		tga_write("base/art/wadtobrush/black.tga", 16, 16, (uint8_t*)black, 4, 4);
	}

	// Read Flats
	for (int i = 0; i < lumps.Num(); i++) {
		if (lumps[i].size != flatSize)
			continue;
		reader.Goto(lumps[i].offset);

		Color flat[flatSize];
		uint8_t colorIndex;
		for (int c = 0; c < flatSize; c++) {
			reader.ReadLE(colorIndex);
			flat[c] = palette[colorIndex];
		}

		// Write the .tga
		std::string path = dir_flats;
		path.append(lumps[i].name.Data());
		path.append(".tga");
		tga_write(path.data(), 64, 64, (uint8_t*)flat, 4, 4);

		// Write the material2 decl
		std::string mat2Path = dir_flats_mat;
		mat2Path.append(lumps[i].name.Data());
		mat2Path.append(".decl");
		
		std::ofstream mat2Writer(mat2Path, std::ios_base::binary);
		mat2Writer << mat2_start << "art/wadtobrush/flats/" << lumps[i].name.Data() << ".tga" << mat2_end;
		mat2Writer.close();
	}

	/*
	* Floor/Ceiling Texture plane pair:
	* - Cannot be rotated
	* - Cannot be shifted
	* - One pixel = 1x1 meter
	* - Might need to rotate 90 degrees clockwise
	* ((0.015625 0 0) (0 0.015625 0))
	*/
}