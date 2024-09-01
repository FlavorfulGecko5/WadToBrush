#include "WadStructs.h"
#include <tga.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>

bool WadLevel::ReadFrom(BinaryReader &reader, VertexTransforms p_transforms, 
	std::unordered_map<WadString, Dimension>& wallDimensions) {
	
	// Set transform data
	transforms = p_transforms;
	metersPerPixel.clear();
	metersPerPixel.reserve(wallDimensions.size());
	for (auto pair : wallDimensions) {
		DimFloat dim;
		dim.width = 1.0f / pair.second.width;
		dim.height = 1.0f / pair.second.height;

		metersPerPixel[pair.first] = dim;
	}

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
	int16_t tempOffset =0;
	for (int32_t i = 0; i < sidedefs.Num(); i++) {
		SideDef& s = sidedefs[i];

		// Read and adjust texture offsets
		reader.ReadLE(tempOffset);
		s.offsetX = tempOffset / transforms.xyDownscale;
		reader.ReadLE(tempOffset);
		s.offsetY = tempOffset / transforms.zDownscale;

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

	// Build Lump Map
	lumpMap.clear();
	lumpMap.reserve(lumps.Num());
	for (int i = 0; i < lumps.Num(); i++)
		lumpMap[lumps[i].name] = &lumps[i];

	// Build texture dimension map
	textureSizes.clear();
	GetTextureDimensions("TEXTURE1");
	GetTextureDimensions("TEXTURE2");

	return true;
}

WadLevel* Wad::DecodeLevel(const char* name, VertexTransforms transforms) {
	for (int32_t i = 0; i < levels.Num(); i++)
		if (levels[i].lumpHeader->name == name) {
			levels[i].ReadFrom(reader, transforms, textureSizes);
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

void Wad::GetTextureDimensions(WadString name) {
	if(lumpMap.find(name) == lumpMap.end())
		return;

	size_t startPosition = lumpMap.at(name)->offset;
	int32_t textureCount = 0;
	BinaryReader offsetReader(reader);
	offsetReader.Goto(startPosition);
	offsetReader.ReadLE(textureCount);

	for (int32_t i = 0; i < textureCount; i++) {
		int32_t offset;
		offsetReader.ReadLE(offset);

		reader.Goto(startPosition + offset);
		WadString name;
		Dimension dim;

		name.ReadFrom(reader);
		reader.GoRight(4); // skip masked bool integer
		reader.ReadLE(dim.width);
		reader.ReadLE(dim.height);
		textureSizes[name] = dim;

		//printf("%s (%i, %i)\n", name, dim.width, dim.height);
	}
}

const char* dir_critical =    "base/art/wadtobrush/";
const char* dir_flats =		  "base/art/wadtobrush/flats/";
const char* dir_flats_mat =   "base/declTree/material2/art/wadtobrush/flats/";
const char* dir_walls =       "base/art/wadtobrush/walls/";
const char* dir_walls_mat =   "base/declTree/material2/art/wadtobrush/walls/";
const char* dir_patches =     "base/art/wadtobrush/patches/";
const char* dir_patches_mat = "base/declTree/material2/art/wadtobrush/patches/";

void WriteMaterial2(const char* subFolder, WadString name) {
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

	std::string matPath = "base/declTree/material2/art/wadtobrush/";
	matPath.append(subFolder);
	matPath.append(name);
	matPath.append(".decl");

	std::ofstream mat2Writer(matPath.data(), std::ios_base::binary);
	mat2Writer << mat2_start << "art/wadtobrush/" << subFolder << name << ".tga" << mat2_end;
	mat2Writer.close();
}

struct Color { // This BGRA variable order is what the TGA writer expects
	uint8_t b = 0;
	uint8_t g = 0;
	uint8_t r = 0;
	uint8_t a = 0;
};

struct PatchImage {
	uint16_t width = 0;
	uint16_t height = 0;
	Color* pixels = nullptr;

	~PatchImage() {
		delete[] pixels;
	}
};

void Wad::ExportTextures(bool exportWalls, bool exportFlats, bool exportPatches) {
	const int32_t paletteSize = 256;
	Color palette[paletteSize];

	// Create directories
	std::filesystem::create_directories(dir_flats);
	std::filesystem::create_directories(dir_flats_mat);
	std::filesystem::create_directories(dir_walls);
	std::filesystem::create_directories(dir_walls_mat);
	std::filesystem::create_directories(dir_patches);
	std::filesystem::create_directories(dir_patches_mat);

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
		uint32_t offset = 0; // Relative to beginning of patch header
		uint8_t topDelta = 0;
		uint8_t length = 0;
		uint8_t unused = 0;
		//WadArray<uint8_t, uint8_t> colors;
		uint8_t unused2 = 0;
	};

	struct PatchHeader {
		WadString name;
		uint16_t width = 0;
		uint16_t height = 0;
		int16_t leftOffset = 0; // Used to create standalone image file - NOT to offset patch in texture
		int16_t topOffset = 0;
		//WadArray<PatchColumn, uint16_t> columns;
	};

	if(exportWalls || exportPatches) {
		PatchImage* images = nullptr;
		int32_t imageCount = 0;

		// Allocate Color array
		BinaryReader columnReader(reader);
		BinaryReader pnameReader(reader);
		pnameReader.Goto(lumpMap.at("PNAMES")->offset);
		pnameReader.ReadLE(imageCount);
		images = new PatchImage[imageCount];

		//std::ofstream patchmeta("patchmeta.txt", std::ios_base::binary);
		printf("%i Patches Found\n", imageCount);
		//patchmeta << imageCount << " Patches Found\n";
		for (int i = 0; i < imageCount; i++) {
			PatchHeader patch;
			patch.name.ReadFrom(pnameReader);

			size_t startPosition = lumpMap.at(patch.name)->offset;
			reader.Goto(startPosition);
			reader.ReadLE(patch.width);
			reader.ReadLE(patch.height);
			reader.ReadLE(patch.leftOffset);
			reader.ReadLE(patch.topOffset);

			//printf("%i %s (%i, %i)\n", i, patch.name.Data(), patch.width, patch.height);
			//patchmeta << i << " " << patch.name.Data() << " (" << patch.width << ", " << patch.height 
			//	<< " ) (" << patch.leftOffset << " " << patch.topOffset << ")\n";

			Color* pixels = new Color[patch.width * patch.height];
			images[i].pixels = pixels;
			images[i].width = patch.width;
			images[i].height = patch.height;
			uint8_t row = 0, col = 0;

			for (int k = 0; k < patch.width; k++) { // Width dictates column count
				PatchColumn column;
				reader.ReadLE(column.offset);
				columnReader.Goto(startPosition + column.offset);

				while (true) {
					columnReader.ReadLE(column.topDelta);
					if (column.topDelta == 0xFF) {
						col++;
						break;
					}
					else row = column.topDelta;

					columnReader.ReadLE(column.length);
					columnReader.ReadLE(column.unused);
					uint8_t index;

					for (int z = 0; z < column.length; z++) {
						columnReader.ReadLE(index);
						pixels[patch.width * row++ + col] = palette[index];
					}
					columnReader.ReadLE(column.unused2);
				}
			}
			if (exportPatches) {
				std::string filepath = dir_patches;
				filepath.append(patch.name);
				filepath.append(".tga");
				tga_write(filepath.data(), patch.width, patch.height, (uint8_t*)pixels, 4, 4);

				WriteMaterial2("patches/", patch.name);
			}

		}
		//patchmeta.close();
		if (exportWalls) {
			ExportTextures_Walls(images, "TEXTURE1");
			ExportTextures_Walls(images, "TEXTURE2");
		}
		delete[] images;

	}

	if(exportFlats)
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
	int16_t patchCount;
	//WadArray<MapPatch, int16_t> patches;
};

void Wad::ExportTextures_Walls(PatchImage* patches, WadString name) {
	if(lumpMap.find(name) == lumpMap.end())
		return;
	printf("Reading %s\n", name.Data());

	MapTexture texture;
	MapPatch patchDef;
	BinaryReader patchReader(reader);
	int32_t wallCount = 0;

	size_t startPosition = lumpMap.at(name)->offset;
	reader.Goto(startPosition);
	reader.ReadLE(wallCount);

	printf("%i Map Textures Found\n", wallCount);

	for (int32_t i = 0; i < wallCount; i++) {
		reader.ReadLE(texture.offset);
		patchReader.Goto(startPosition + texture.offset);

		texture.name.ReadFrom(patchReader);
		patchReader.ReadLE(texture.masked);
		patchReader.ReadLE(texture.width);
		patchReader.ReadLE(texture.height);
		patchReader.ReadLE(texture.columnDir);

		patchReader.ReadLE(texture.patchCount);
		//printf("%i %s (%i x %i) %i", i, texture.name, texture.width, texture.height, texture.patchCount);

		Color* pixels = new Color[texture.width * texture.height];
		for (int16_t k = 0; k < texture.patchCount; k++) {
			patchReader.ReadLE(patchDef.originX);
			patchReader.ReadLE(patchDef.originY);
			patchReader.ReadLE(patchDef.patchIndex);
			patchReader.ReadLE(patchDef.stepDir);
			patchReader.ReadLE(patchDef.colormap);

			//printf(" (%i %i, %i)", patchDef.patchIndex, patchDef.originX, patchDef.originY);

			// IMPORTANT: PATCHES MAY BE BIGGER THAN THE FINAL WALL TEXTURES (running off the screen)
			// IMPORTANT: PATCHES WITH OPACITY 0 PIXELS CAN GET OVERLAYED OVER OTHER TEXTURES
			//	SOLUTION: Initialize wall texture with all opacity 0 pixels. Only copy a pixel onto the texture
			// if it's opacity isn't 0

			Color* patchImage = patches[patchDef.patchIndex].pixels;
			int patchWidth = patches[patchDef.patchIndex].width;
			int patchHeight = patches[patchDef.patchIndex].height;


			// Define bounds on the canvas
			int rowMin = patchDef.originY, colMin = patchDef.originX;

			// Classic doom just...ignores negative vertical offsets
			// Most textures don't have these...but it's noticeable in the ones that do
			if(rowMin < 0)
				rowMin = 0;

			int rowMax = rowMin + patchHeight, colMax = colMin + patchWidth;
			
			if(rowMax > texture.height)
				rowMax = texture.height;
			if(colMax > texture.width)
				colMax = texture.width;

			for (int currentRow = rowMin; currentRow < rowMax; currentRow++) {
				//if(currentRow < 0) // We won't be drawing any out of bounds rows
				//	continue;
				// Ensure we're reading correct row of patch image (if patch runs off texture)
				int patchImageIndex = patchWidth * (currentRow - rowMin); 
				int index = currentRow * texture.width;
				for (int currentCol = colMin; currentCol < colMax; currentCol++) {
					// Current column may be out of bounds while future columns are not.
					// We must ensure the patch index continues to get incremented no matter what
					if(currentCol > -1 && patchImage[patchImageIndex].a > 0)
						pixels[index + currentCol] = patchImage[patchImageIndex];
					patchImageIndex++;
				}
			}

		}
		std::string filepath = dir_walls;
		filepath.append(texture.name);
		filepath.append(".tga");
		tga_write(filepath.data(), texture.width, texture.height, (uint8_t*)pixels, 4, 4);

		WriteMaterial2("walls/", texture.name);

		//printf("\n");
		delete[] pixels;
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
		WriteMaterial2("flats/", lumps[i].name);
	}
}