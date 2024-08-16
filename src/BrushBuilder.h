#include <WadStructs.h>
#include <sstream>

struct Vector {
	float x, y, z;

	Vector() : x(0), y(0), z(0) {}
	
	Vector(float p_x, float p_y, float p_z) : x(p_x), y(p_y), z(p_z) {}

	Vector(VertexFloat v0, VertexFloat v1) : x(v1.x - v0.x), y(v1.y - v0.y), z(0) {}

	void Normalize() {
		float magnitude = sqrtf(x * x + y * y + z * z);
		if (magnitude != 0) {
			x /= magnitude;
			y /= magnitude;
			z /= magnitude;
		}
	}
};

struct Plane {
	Vector n;
	float d = 0;

	void SetFrom(Vector p_normal, VertexFloat point) {
		n = p_normal;
		n.Normalize();
		d = n.x * point.x + n.y * point.y;
	}

	void ToString(std::ostringstream& writer) {
		writer << "( " << n.x << ' ' << n.y << ' ' << n.z << ' ' << (d * -1) << " ) " << "( ( 1 0 0 ) ( 0 1 0 ) ) \"art/tile/common/shadow_caster\" 0 0 0";
	}
};

struct WallBrush {
	Plane bounds[6];
	int lineIndex;

	WallBrush(VertexFloat v0, VertexFloat v1, float minHeight, float maxHeight, int p_lineIndex) : lineIndex(100000000 + p_lineIndex)  
	{
		Vector horizontal(v0, v1);

		// Plane 0 - The "Front" SideDef to the LineDef's right
		bounds[0].SetFrom(Vector(horizontal.y, -horizontal.x, 0), v1);

		// Plane 1 - The "Back" SideDef to the LinDef's left
		bounds[1].n = bounds[0].n;
		bounds[1].n.x *= -1;
		bounds[1].n.y *= -1;

		VertexFloat d0(bounds[1].n.x * 0.0075 + v0.x, bounds[1].n.y * 0.0075 + v0.y);
		VertexFloat d1(bounds[1].n.x * 0.0075 + v1.x, bounds[1].n.y * 0.0075 + v1.y);
		bounds[1].d = bounds[1].n.x * d1.x + bounds[1].n.y * d1.y;


		// Plane 2: Forward Border Sliver: d1 - v1
		horizontal = Vector(v1, d1);
		bounds[2].SetFrom(Vector(horizontal.y, -horizontal.x, 0), d1);
	 
		// Plane 3: Rear Border Sliver: v0 - d0
		bounds[3].n = bounds[2].n;
		bounds[3].n.x *= -1;
		bounds[3].n.y *= -1;
		bounds[3].d = bounds[3].n.x * v0.x + bounds[3].n.y * v0.y;

		// Plane 4: Upper Bound:
		bounds[4].n = Vector(0, 0, 1);
		bounds[4].d = maxHeight;

		// Plane 5: Lower Bound
		bounds[5].n = Vector(0, 0, -1);
		bounds[5].d = minHeight * -1;

	}

	void ToString(std::ostringstream& writer) {
		writer << "{\n\thandle = " << lineIndex << "\n\tbrushDef3 {";
		for (int i = 0; i < 6; i++) {
			writer << "\n\t\t";
			bounds[i].ToString(writer);
		}
		writer << "\n\t}\n}\n";
	}
};