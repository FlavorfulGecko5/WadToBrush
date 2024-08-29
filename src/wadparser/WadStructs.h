#include <cstdint>
#include <BinaryReader.h>
#include <vector>
#include <array>
#include <unordered_map>

template<typename T, typename N>
class WadArray {
	private:
	T* items = nullptr;
	N num = 0;

	public:
	~WadArray() {
		delete[] items;
	}

	void Reserve(N p_num) {
		num = p_num;
		delete[] items;
		items = new T[num];
	}

	void ReserveFrom(BinaryReader& reader) {
		reader.ReadLE(num);
		delete[] items;
		items = new T[num];
	}

	T& operator[](const N index) {
		return items[index];
	}

	N Num() {
		return num;
	}
};

class WadString {
	private:
	#define LENGTH_WADSTRING 8
	char data[LENGTH_WADSTRING + 1];

	public:
	WadString() {
		data[LENGTH_WADSTRING] = '\0';
	}

	WadString(const WadString& b) {
		for(int i = 0; i < LENGTH_WADSTRING; i++)
			data[i] = b.data[i];
		data[LENGTH_WADSTRING] = '\0';
	}

	WadString(const char* s) {
		size_t length = strlen(s);
		if(length > LENGTH_WADSTRING)
			length = LENGTH_WADSTRING;

		int i = 0;
		for(; i < length; i++)
			data[i] = s[i];

		// Must ensure null-termination to prevent errors
		for(; i <= LENGTH_WADSTRING; i++)
			data[i] = '\0';
	}

	bool operator==(const WadString b) const {
		return strcmp(data, b.Data()) == 0;
	}

	bool operator==(const char* stringB) const {
		return strcmp(data, stringB) == 0;
	}

	bool operator!=(const char* stringB) const {
		return strcmp(data, stringB) != 0;
	}

	void ReadFrom(BinaryReader& reader) {
		reader.ReadBytes(data, LENGTH_WADSTRING);
	}

	const char* Data() const {
		return data;
	}

	operator char*() {
		return data;
	}
};

template <>
struct std::hash<WadString>
{
	std::size_t operator()(const WadString& k) const
	{
		const char* data = k.Data();
		size_t hash = 0;

		while (*data != '\0')
			hash ^= std::hash<char>()(*data++);
		return hash;
	}
};

enum class LumpType : unsigned char {
	UNIDENTIFIED,
	MAP_HEADER,
	MAP_THINGS
};

struct LumpEntry {
	int32_t offset = 0;
	int32_t size = 0;
	WadString name;
	LumpType type = LumpType::UNIDENTIFIED;
};

// Not part of original doom wad structures. But we need this for precision when converting
// We will convert from 16-bit integer vertices read from WADs, to float32 vertices
struct VertexFloat {
	float x;
	float y;

	VertexFloat() : x(0), y(0) {}

	VertexFloat(float p_x, float p_y) : x(p_x), y(p_y) {}

	VertexFloat(std::array<float, 2> p) : x(p[0]), y(p[1]) {}

	bool operator==(VertexFloat b) {
		return x == b.x && y == b.y;
	}

	static constexpr int32_t size() {
		return 4;
	}
};

#define NO_SIDEDEF 0xFFFF

struct LineDef {
	// NOTE: Most source ports treat these values as unsigned, but
	// the original Doom code has these as shorts
	uint16_t vertexStart;
	uint16_t vertexEnd;
	uint16_t flags;
	uint16_t specialType;
	uint16_t sectorTag;
	uint16_t sideFront;
	uint16_t sideBack;

	static constexpr int32_t size() {
		return 14;
	}
};

struct SideDef {
	int16_t texXOffset;
	int16_t texYOffset;
	WadString upperTexture;
	WadString middleTexture;
	WadString lowerTexture;
	int16_t sector;

	static constexpr int32_t size() {
		return 30;
	}
};

struct SimpleLineDef {
	VertexFloat v0;
	VertexFloat v1;

	void swapOrder() {
		VertexFloat temp = v0;
		v0 = v1;
		v1 = temp;
	}
};

struct Sector {
	//int16_t floorHeight;
	//int16_t ceilingHeight;
	float floorHeight = 0.0f;
	float ceilHeight = 0.0f;
	WadString floorTexture;
	WadString ceilingTexture;
	int16_t lightLevel;
	int16_t specialType;
	int16_t tagNumber;

	// Stores Sector's linedef info for quick retrieval during floor construction
	std::vector<SimpleLineDef> lines;

	static constexpr int32_t size() {
		return 26;
	}
};

struct VertexTransforms {
	float xShift = 0.0f;
	float yShift = 0.0f;
	float xyDownscale = 1.0f;
	float zDownscale = 1.0f;

	VertexTransforms() {}

	VertexTransforms(int16_t p_xShift, int16_t p_yShift, float p_xyDownscale, float p_zDownscale) 
		: xShift(p_xShift), yShift(p_yShift), xyDownscale(p_xyDownscale), zDownscale(p_zDownscale) {}
};

struct WadLevel {
	LumpEntry* lumpHeader;
	LumpEntry* lumpThings;
	LumpEntry* lumpLines;
	LumpEntry* lumpSides;
	LumpEntry* lumpVertex;
	LumpEntry* lumpSectors;

	//WadArray<Vertex> vertices;
	WadArray<VertexFloat, int32_t> verts;
	WadArray<LineDef, int32_t> linedefs;
	WadArray<SideDef, int32_t> sidedefs;
	WadArray<Sector, int32_t> sectors;

	float maxHeight;
	float minHeight;
	float xyDownscale; // Needed for texture scaling

	bool ReadFrom(BinaryReader &reader, VertexTransforms transforms);
	void Debug();
};


struct Color;
class Wad {
	private:
	BinaryReader reader;
	int32_t lumptableOffset = 0;
	WadArray<LumpEntry, int32_t> lumps;
	WadArray<WadLevel, int32_t> levels;

	public:
	bool ReadFrom(const char* wadpath);
	WadLevel* DecodeLevel(const char* name, VertexTransforms transforms);
	void WriteLumpNames();

	/* Texture Exporting */

	private:
	std::unordered_map<WadString, LumpEntry*> lumpMap;
	void ExportTextures_Walls(Color* palette, WadString name);
	void ExportTextures_Flats(Color* palette);

	public:
	void ExportTextures(bool walls, bool flats);
};