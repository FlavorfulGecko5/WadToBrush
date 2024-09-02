#include <WadStructs.h>

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

	float Magnitude() {
		return sqrtf(x * x + y * y + z * z);
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
};