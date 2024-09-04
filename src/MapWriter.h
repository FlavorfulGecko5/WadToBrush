#include "BrushBuilder.h"
#include <sstream>

class MapWriter {
	private:
	std::ostringstream writer;
	VertexTransforms tforms;
	std::unordered_map<WadString, DimFloat>& wallRatios;

	const float flatScale;
	const float flatXShift;
	const float flatYShift;


	// Must increment with every written brush
	int brushHandle = 100000000;


	public:
	MapWriter(WadLevel& level);
	void SaveFile(WadString levelName);

	void WriteWallBrush(VertexFloat v0, VertexFloat v1, float minHeight, float maxHeight, float drawHeight, WadString texture, float offsetX);
	void WriteFloorBrush(VertexFloat a, VertexFloat b, VertexFloat c, float height, bool isCeiling, WadString texture);

	private:
	void BeginBrushDef();
	void EndBrushDef();
	void WritePlane(const Plane p);
};