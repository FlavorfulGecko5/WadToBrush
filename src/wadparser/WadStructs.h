#include <cstdint>
#include <BinaryReader.h>

template<typename T>
class WadArray {
	private:
	T* items = nullptr;
	int32_t num = 0;

	public:
	~WadArray() {
		delete[] items;
	}

	void Reserve(int32_t p_num) {
		num = p_num;
		delete[] items;
		items = new T[num];
	}

	void ReserveFrom(BinaryReader& reader) {
		reader.ReadLE(num);
		delete[] items;
		items = new T[num];
	}

	T& operator[](const int32_t index) {
		return items[index];
	}

	int32_t Num() {
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

	bool operator==(const char* stringB) {
		return strcmp(data, stringB) == 0;
	}

	void ReadFrom(BinaryReader& reader) {
		reader.ReadBytes(data, LENGTH_WADSTRING);
	}

	const char* Data() {
		return data;
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

struct LumpTable {
	WadArray<LumpEntry> lumps;
	int32_t tableOffset = 0;

	// Metadata Variables
	int32_t levelCount = 0;

	bool ReadFrom(BinaryReader &reader);
};

struct Vertex {
	int16_t x;
	int16_t y;

	Vertex() : x(0), y(0) {}

	Vertex(int16_t p_x, int16_t p_y) : x(p_x), y(p_y) {}

	static constexpr int32_t size() {
		return 4;
	}
};

// Not part of original doom wad structures. But we need this for precision when converting
struct VertexFloat {
	float x;
	float y;

	VertexFloat() : x(0), y(0) {}

	VertexFloat(float p_x, float p_y) : x(p_x), y(p_y) {}

	VertexFloat(Vertex copy) : x(copy.x), y(copy.y) {}
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

struct Sector {
	int16_t floorHeight;
	int16_t ceilingHeight;
	WadString floorTexture;
	WadString ceilingTexture;
	int16_t lightLevel;
	int16_t specialType;
	int16_t tagNumber;

	static constexpr int32_t size() {
		return 26;
	}
};

struct WadLevel {
	LumpEntry* lumpHeader;
	LumpEntry* lumpThings;
	LumpEntry* lumpLines;
	LumpEntry* lumpSides;
	LumpEntry* lumpVertex;
	LumpEntry* lumpSectors;

	WadArray<Vertex> vertices;
	WadArray<LineDef> linedefs;
	WadArray<SideDef> sidedefs;
	WadArray<Sector> sectors;

	bool ReadFrom(BinaryReader &reader);
	void Debug();
};

struct Wad {
	LumpTable lumptable;
	WadArray<WadLevel> levels;

	bool ReadFrom(const char* wadpath);
};