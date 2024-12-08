#ifndef PHYSICS_H
#define PHYSICS_H

#include <cmath>

struct hv3 {
	float x;
	float y;
	float z;
	float w;

	hv3 &operator*=(float scalar) {
		x *= scalar;
		y *= scalar;
		z *= scalar;
		return *this;
	}

	inline float magnitude() const { return std::sqrt(x * x + y * y + z * z); }

	inline float squareMagnitude() const { return x * x + y * y + z * z; }

	inline void normalize() {
		float l = magnitude();
		if (l > 0) {
			*this *= (1.f / l);
		}
	}

	hv3 operator%(const hv3 &other) const {
		return hv3{y * other.z - z * other.y, z * other.x - x * other.z,
				   x * other.y - y * other.x, 0.0f};
	}
};

void physics_system(struct game *g, struct Memory *m);

#endif // PHYSICS_H
