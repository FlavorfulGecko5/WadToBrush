#include "BrushBuilder.h"
#include <sstream>

class MapWriter {
	private:
	std::ostringstream writer;
	VertexTransforms tforms;
	std::unordered_map<WadString, DimFloat>& wallRatios;


	// Must increment with every written brush
	int brushHandle = 100000000;


	public:
	MapWriter(WadLevel& level);
	void SaveFile(WadString levelName);

	void WriteBrush(const WallBrush& b);
	void WriteBrush(const FloorBrush& b);

	private:
	void WritePlane(const Plane p);
};