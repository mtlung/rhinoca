#include "pch.h"
#include "roVector.h"

namespace ro {

inline Vec2& Vec2::truncate(float length) {
	float length2;
	float ilength;

	if (!length) {
		zero();
	}
	else {
		length2 = lengthSqr();
		if (length2 > length * length) {
			ilength = length * roInvSqrt(length2);
			x *= ilength;
			y *= ilength;
		}
	}

	return *this;
}

inline void Vec2::clamp(const Vec2& min, const Vec2& max) {
	if (x < min.x) {
		x = min.x;
	} else if (x > max.x) {
		x = max.x;
	}
	if (y < min.y) {
		y = min.y;
	} else if (y > max.y) {
		y = max.y;
	}
}


// ----------------------------------------------------------------------

inline bool Vec3::fixDegenerateNormal(void) {
	if (x == 0.0f) {
		if (y == 0.0f) {
			if (z > 0.0f) {
				if (z != 1.0f) {
					z = 1.0f;
					return true;
				}
			} else {
				if (z != -1.0f) {
					z = -1.0f;
					return true;
				}
			}
			return false;
		} else if (z == 0.0f) {
			if (y > 0.0f) {
				if (y != 1.0f) {
					y = 1.0f;
					return true;
				}
			} else {
				if (y != -1.0f) {
					y = -1.0f;
					return true;
				}
			}
			return false;
		}
	} else if (y == 0.0f) {
		if (z == 0.0f) {
			if (x > 0.0f) {
				if (x != 1.0f) {
					x = 1.0f;
					return true;
				}
			} else {
				if (x != -1.0f) {
					x = -1.0f;
					return true;
				}
			}
			return false;
		}
	}
	if (roFAbs(x) == 1.0f) {
		if (y != 0.0f || z != 0.0f) {
			y = z = 0.0f;
			return true;
		}
		return false;
	} else if (roFAbs(y) == 1.0f) {
		if (x != 0.0f || z != 0.0f) {
			x = z = 0.0f;
			return true;
		}
		return false;
	} else if (roFAbs(z) == 1.0f) {
		if (x != 0.0f || y != 0.0f) {
			x = y = 0.0f;
			return true;
		}
		return false;
	}
	return false;
}

inline bool Vec3::fixDenormals(void) {
	bool denormal = false;
	if (roFAbs(x) < 1e-30f) {
		x = 0.0f;
		denormal = true;
	}
	if (roFAbs(y) < 1e-30f) {
		y = 0.0f;
		denormal = true;
	}
	if (roFAbs(z) < 1e-30f) {
		z = 0.0f;
		denormal = true;
	}
	return denormal;
}

inline Vec3& Vec3::truncate(float length) {
	float length2;
	float ilength;

	if (!length) {
		zero();
	}
	else {
		length2 = lengthSqr();
		if (length2 > length * length) {
			ilength = length * roInvSqrt(length2);
			x *= ilength;
			y *= ilength;
			z *= ilength;
		}
	}

	return *this;
}

inline void Vec3::clamp(const Vec3& min, const Vec3& max) {
	if (x < min.x) {
		x = min.x;
	} else if (x > max.x) {
		x = max.x;
	}
	if (y < min.y) {
		y = min.y;
	} else if (y > max.y) {
		y = max.y;
	}
	if (z < min.z) {
		z = min.z;
	} else if (z > max.z) {
		z = max.z;
	}
}

inline void Vec3::normalVectors(Vec3& left, Vec3& down) const {
	float d;

	d = x * x + y * y;
	if (!d) {
		left[0] = 1;
		left[1] = 0;
		left[2] = 0;
	} else {
		d = roInvSqrt(d);
		left[0] = -y * d;
		left[1] = x * d;
		left[2] = 0;
	}
	down = left.cross(*this);
}

inline void Vec3::orthogonalBasis(Vec3& left, Vec3& up) const {
	float l, s;

	if (roFAbs(z) > 0.7f) {
		l = y * y + z * z;
		s = roInvSqrt(l);
		up[0] = 0;
		up[1] = z * s;
		up[2] = -y * s;
		left[0] = l * s;
		left[1] = -x * up[2];
		left[2] = x * up[1];
	}
	else {
		l = x * x + y * y;
		s = roInvSqrt(l);
		left[0] = -y * s;
		left[1] = x * s;
		left[2] = 0;
		up[0] = -z * left[1];
		up[1] = z * left[0];
		up[2] = l * s;
	}
}

inline void Vec3::projectOntoPlane(const Vec3& normal, float overBounce) {
	float backoff;

	backoff = dot(normal);

	if (overBounce != 1.0) {
		if (backoff < 0) {
			backoff *= overBounce;
		} else {
			backoff /= overBounce;
		}
	}

	*this -= backoff * normal;
}

inline bool Vec3::projectAlongPlane(const Vec3& normal, float epsilon, float overBounce) {
	Vec3 cross;
	float len;

	cross = this->cross(normal).cross((*this));
	// normalize so a fixed epsilon can be used
	cross.normalize();
	len = normal.dot(cross);
	if (roFAbs(len) < epsilon) {
		return false;
	}
	cross *= overBounce * dot(normal) / len;
	(*this) -= cross;
	return true;
}

roStatus Reflection::serialize_vec2(Serializer& se, Field& field, void* fieldParent)
{
	return roStatus::ok;
}

roStatus Reflection::serialize_vec3(Serializer& se, Field& field, void* fieldParent)
{
	return roStatus::ok;
}

roStatus Reflection::serialize_vec4(Serializer& se, Field& field, void* fieldParent)
{
	return roStatus::ok;
}

}	// namespace ro
