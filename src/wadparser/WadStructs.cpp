#include "WadStructs.h"
#include <tga.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>

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

	// Read WAD Magic
	{
		const size_t SIZE_MAGIC = 4;
		char magic[SIZE_MAGIC];
		reader.ReadBytes(magic, SIZE_MAGIC);
		if (memcmp(magic, "IWAD", SIZE_MAGIC) != 0 && memcmp(magic, "PWAD", SIZE_MAGIC) != 0) {
			printf("File must be an IWAD or a PWAD\n");
			return false;
		}
	}

	// Read Table Metadata
	lumps.ReserveFrom(reader);
	reader.ReadLE(lumptableOffset);

	// Read Each Table Entry
	reader.Goto(lumptableOffset);
	for (int32_t i = 0; i < lumps.Num(); i++) {
		LumpEntry& lump = lumps[i];
		reader.ReadLE(lump.offset);
		reader.ReadLE(lump.size);
		lump.name.ReadFrom(reader);
	}

	// Identify map header lumps
	int32_t levelCount = 0;
	for (int32_t i = 0; i < lumps.Num(); i++) {
		if (lumps[i].name == "THINGS") {
			lumps[i - 1].type = LumpType::MAP_HEADER;
			levelCount++;
		}
	}

	// Initiate Level Array and populate lump references
	levels.Reserve(levelCount);
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

void Wad::WriteLumpNames() {
	std::ofstream output("lumpnames.txt", std::ios_base::binary);

	for(int i = 0; i < lumps.Num(); i++)
		output << lumps[i].name << "\n";
}


// ====================
// TEXTURE EXPORTING
// ====================

void WriteMaterial2(const std::string& filepath, WadString texture) {
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

	const char* mat2_end = 
		"\";\n"
		"					}\n"
		"				}\n"
		"			}\n"
		"		}\n"
		"	}\n"
		"}";

	std::ofstream mat2Writer(filepath, std::ios_base::binary);
	mat2Writer << mat2_start << "art/wadtobrush/flats/" << texture << ".tga" << mat2_end;
	mat2Writer.close();
}

struct Color { // This BGRA variable order is what the TGA writer expects
	uint8_t b = 0;
	uint8_t g = 0;
	uint8_t r = 0;
	uint8_t a = 0;
};

const char* dir_flats = "base/art/wadtobrush/flats/";
const char* dir_flats_mat = "base/declTree/material2/art/wadtobrush/flats/";
const char* dir_walls = "base/art/wadtobrush/walls/";
const char* dir_walls_mat = "base/declTree/material2/art/wadtobrush/walls/";

void Wad::ExportTextures(bool walls, bool flats) {
	const int32_t paletteSize = 256;
	Color palette[paletteSize];

	// Create directories
	std::filesystem::create_directories(dir_flats);
	std::filesystem::create_directories(dir_flats_mat);
	std::filesystem::create_directories(dir_walls);
	std::filesystem::create_directories(dir_walls_mat);

	// Build Lump Map
	lumpMap.reserve(lumps.Num());
	for(int i = 0; i < lumps.Num(); i++)
		lumpMap[lumps[i].name] = &lumps[i];

	// Read First PlayPal Palette
	reader.Goto(lumpMap.at("PLAYPAL")->offset);
	for (int i = 0; i < lumps.Num(); i++) {
		if (lumps[i].name == "PLAYPAL") {
			reader.Goto(lumps[i].offset);
			for (int c = 0; c < paletteSize; c++) {
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

	struct PatchColumn {
		uint32_t offset; // Relative to beginning of patch header
		uint8_t topDelta;
		uint8_t length;
		uint8_t unused;
		//WadArray<uint8_t, uint8_t> colors;
		uint8_t unused2;
	};

	struct PatchHeader {
		WadString name;
		uint16_t width;
		uint16_t height;
		int16_t leftOffset;
		int16_t topOffset;
		WadArray<PatchColumn, uint16_t> columns;

		Color* pixels = nullptr;

		~PatchHeader() {
			delete[] pixels;
		}
	};

	if(walls) {
		// Read Patch Names
		WadArray<PatchHeader, int32_t> patches;
		reader.Goto(lumpMap.at("PNAMES")->offset);
		patches.ReserveFrom(reader);
		for(int i = 0; i < patches.Num(); i++)
			patches[i].name.ReadFrom(reader);

		printf("%i Patches Found\n", patches.Num());
		for (int i = 0; i < patches.Num(); i++) {
			//if(i == 1) break;
			if (i == 162) continue; // w94_1 is cursed - force WadStrings all uppercase?
			
			PatchHeader& p = patches[i];
			size_t startPosition = lumpMap.at(p.name)->offset;
			reader.Goto(startPosition);

			reader.ReadLE(p.width);
			reader.ReadLE(p.height);
			reader.ReadLE(p.leftOffset);
			reader.ReadLE(p.topOffset);
			
			p.columns.Reserve(p.width); // Width dictates number of columns

			printf("%i %s (%i, %i) (%i, %i)\n", i, p.name, p.width, p.height, p.leftOffset, p.topOffset);

			// Read Column Offsets
			for(int k = 0; k < p.columns.Num(); k++)
				reader.ReadLE(p.columns[k].offset);

			// Initialize Color Data
			// index = width * row + col
			p.pixels = new Color[p.width * p.height];
			uint8_t row = 0, col = 0;

			for (int k = 0; k < p.columns.Num(); k++) {
				printf("Reading Column %i\n", k);
				PatchColumn& c = p.columns[k];
				reader.Goto(startPosition + c.offset);

				bool foundTerminal = false;
				while (!foundTerminal) {
					reader.ReadLE(c.topDelta);
					if (c.topDelta == (uint8_t)0xFF) {
						printf("0xFF - Shifting Column\n");
						foundTerminal = true;
						col++;
						continue;
					}
					else row = c.topDelta; // Default initializer sets alpha of skipped pixels to 0

					uint8_t index;
					reader.ReadLE(c.length);
					reader.ReadLE(c.unused);

					printf("%i pixels in patch\n", c.length);
					for (uint8_t z = 0; z < c.length; z++) {
						reader.ReadLE(index);
						p.pixels[p.width * row++ + col] = palette[index];
					}
					reader.ReadLE(c.unused2);
				}
			}
			std::string filepath = "base/art/wadtobrush/walls/";
			filepath.append(p.name);
			filepath.append(".tga");
			tga_write(filepath.data(), p.width, p.height, (uint8_t*)p.pixels, 4, 4);
		}
		//ExportTextures_Walls(palette, "TEXTURE1");
		//ExportTextures_Walls(palette, "TEXTURE2");
	}

	if(flats)
		ExportTextures_Flats(palette);
}

struct MapPatch {
	int16_t originX;
	int16_t originY;
	int16_t patchIndex;
	int16_t stepDir;  //Unused
	int16_t colormap; //Unused
};

struct MapTexture {
	int32_t offset; // Relative to start of texture lump
	WadString name;
	int32_t masked;  // Unknown use
	int16_t width;
	int16_t height;
	int32_t columnDir; // Unused
	WadArray<MapPatch, int16_t> patches;
};

void Wad::ExportTextures_Walls(Color* palette, WadString name) {
	if(lumpMap.find(name) == lumpMap.end())
		return;
	printf("Reading %s\n", name.Data());

	WadArray<MapTexture, int32_t> textures;

	// Read the lump header containing the relative offsets
	size_t startPosition = lumpMap.at(name)->offset;
	reader.Goto(startPosition);
	textures.ReserveFrom(reader);
	for(int i = 0; i < textures.Num(); i++)
		reader.ReadLE(textures[i].offset);

	printf("%i Map Textures Found\n", textures.Num());
	// Read each MapTexture
	for (int i = 0; i < textures.Num(); i++) {
		MapTexture& t = textures[i];
		reader.Goto(startPosition + t.offset);
		
		t.name.ReadFrom(reader);
		reader.ReadLE(t.masked);
		reader.ReadLE(t.width);
		reader.ReadLE(t.height);
		reader.ReadLE(t.columnDir);
		

		t.patches.ReserveFrom(reader);
		printf("%s (%i x %i) %i", t.name, t.width, t.height, t.patches.Num());
		for (int16_t k = 0; k < t.patches.Num(); k++) {
			MapPatch& mp = t.patches[k];
			reader.ReadLE(mp.originX);
			reader.ReadLE(mp.originY);
			reader.ReadLE(mp.patchIndex);
			reader.ReadLE(mp.stepDir);
			reader.ReadLE(mp.colormap);

			printf(" (%i, %i)", mp.originX, mp.originY);
		}
		printf("\n");
		
	}

	

}

void Wad::ExportTextures_Flats(Color* palette) {
	const int32_t flatSize = 4096;
	Color flat[flatSize];

	for (int i = 0; i < lumps.Num(); i++) {
		// This will cause ANY 4096 byte lump to
		// get interpreted as a texture, not just genuine flat textures
		if (lumps[i].size != flatSize)
			continue;
		reader.Goto(lumps[i].offset);

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

		WriteMaterial2(mat2Path, lumps[i].name);
	}
}