#ifndef TINYRENDER_CPP_INCLUDED
#define TINYRENDER_CPP_INCLUDED

#include "../dependency/GL/glew.h"
#include "../dependency/GLFW/glfw3.h"
#include "../dependency/imgui/imgui.h"
#include "../dependency/imguizmo/ImGuizmo.h"

#include <cmath>
#include <vector>

namespace tinyrender
{
	// Minimalist quick & dirty maths. Not here for completeness.
	template<typename T>
	inline T min(const T& a, const T& b)
	{
		return a < b ? a : b;
	}

	template<typename T>
	inline T max(const T& a, const T& b)
	{
		return a > b ? a : b;
	}

	typedef struct v2f
	{
	public:
		union
		{
			float v[2];
			struct { float x, y; };
		};
	} v2f;
	inline float internalLength2(const v2f v)
	{
		return v.x * v.x + v.y * v.y;
	}
	inline v2f operator-(const v2f& l, const v2f r)
	{
		return { l.x - r.x, l.y - r.y };
	}

	typedef struct v3f
	{
	public:
		union
		{
			float v[3];
			struct { float x, y, z; };
		};
		inline v3f& operator+=(const v3f& r)
		{
			x += r.x; y += r.y; z += r.z;
			return *this;
		}
		inline v3f& operator/=(float r)
		{
			x /= r; y /= r; z /= r;
			return *this;
		}
		inline float& operator[](int i)
		{
			return v[i];
		}
	} v3f;
	inline v3f min(const v3f& a, const v3f& b)
	{
		return { min(a.x, b.x), min(a.y, b.y), min(a.z, b.z) };
	}
	inline v3f max(const v3f& a, const v3f& b)
	{
		return { max(a.x, b.x), max(a.y, b.y), max(a.z, b.z) };
	}
	inline float internalLength2(const v3f& v)
	{
		return v.x * v.x + v.y * v.y + v.z * v.z;
	}
	inline float internalLength(const v3f& v)
	{
		return std::sqrt(internalLength2(v));
	}
	inline v3f internalNormalize(const v3f& v)
	{
		float vv = internalLength(v);
		return v3f({ v.x / vv, v.y / vv, v.z / vv });
	}
	inline v3f internalCross(const v3f& v1, const v3f& v2)
	{
		return v3f({ v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x });
	}
	inline float internalDot(const v3f& v1, const v3f& v2)
	{
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	}
	inline v3f operator-(const v3f& l, const v3f r)
	{
		return { l.x - r.x, l.y - r.y, l.z - r.z };
	}
	inline v3f operator+(const v3f& l, const v3f& r)
	{
		return { l.x + r.x, l.y + r.y, l.z + r.z };
	}
	inline v3f operator*(const v3f& l, float x)
	{
		return { l.x * x, l.y * x, l.z * x };
	}
	inline v3f operator/(const v3f& l, float x)
	{
		return { l.x / x, l.y / x, l.z / x };
	}
	inline v3f operator-(const v3f& l)
	{
		return { -l.x, -l.y, -l.z };
	}
	inline float toRadian(float degrees)
	{
		return degrees * static_cast<float>(0.01745329251994329576923690768489);
	}

	typedef struct v4f
	{
		union
		{
			float v[4];
			struct { float x, y, z, w; };
		};
		inline v4f& operator+=(const v4f& r)
		{
			x += r.x; y += r.y; z += r.z; w += r.w;
			return *this;
		}
		inline v4f& operator/=(float r)
		{
			x /= r; y /= r; z /= r; w /= r;
			return *this;
		}
		inline float& operator[](int i)
		{
			return v[i];
		}
	} v4f;

	typedef struct m4
	{
	public:
		float m[16];

		inline m4()
		{
			for (int i = 0; i < 16; i++)
				m[i] = 0.0f;
			m[toIndex1D(0, 0)] = 1.0f;
			m[toIndex1D(1, 1)] = 1.0f;
			m[toIndex1D(2, 2)] = 1.0f;
			m[toIndex1D(3, 3)] = 1.0f;
		}
		inline int toIndex1D(int i, int j) const
		{
			return i * 4 + j;
		}
		inline float& operator()(int i, int j)
		{
			return m[toIndex1D(i, j)];
		}
		inline float operator()(int i, int j) const
		{
			return m[toIndex1D(i, j)];
		}
	} m4;
	inline m4 perspectiveMatrix(float zNear, float zFar, float width, float height)
	{
		const float tanHalfFovy = tan(toRadian(45.0f) / 2.0f);

		m4 mat;
		mat(0, 0) = 1.0f / (width / height * tanHalfFovy);
		mat(1, 1) = 1.0f / tanHalfFovy;
		mat(2, 2) = -(zFar + zNear) / (zFar - zNear);
		mat(2, 3) = -1.0f;
		mat(3, 2) = -(2.0f * zFar * zNear) / (zFar - zNear);

		return mat;
	}
	inline m4 lookAtMatrix(const v3f& eye, const v3f& at)
	{
		const v3f f = internalNormalize(at - eye);
		const v3f s = internalNormalize(internalCross(f, { 0, 1, 0 }));
		const v3f u = internalCross(s, f);

		m4 mat;
		mat(0, 0) = s.x;
		mat(1, 0) = s.y;
		mat(2, 0) = s.z;
		mat(0, 1) = u.x;
		mat(1, 1) = u.y;
		mat(2, 1) = u.z;
		mat(0, 2) = -f.x;
		mat(1, 2) = -f.y;
		mat(2, 2) = -f.z;
		mat(3, 0) = -internalDot(s, eye);
		mat(3, 1) = -internalDot(u, eye);
		mat(3, 2) = internalDot(f, eye);
		mat(3, 3) = 1.0f;

		return mat;
	}
	inline v4f operator*(const v4f& v, const m4& mat)
	{
		v4f out;
		out.x = v.x * mat(0, 0) + v.y * mat(1, 0) + v.z * mat(2, 0) + v.w * mat(3, 0);
		out.y = v.x * mat(0, 1) + v.y * mat(1, 1) + v.z * mat(2, 1) + v.w * mat(3, 1);
		out.z = v.x * mat(0, 2) + v.y * mat(1, 2) + v.z * mat(2, 2) + v.w * mat(3, 2);
		out.w = v.x * mat(0, 3) + v.y * mat(1, 3) + v.z * mat(2, 3) + v.w * mat(3, 3);
		return out;
	}

	typedef struct aabb
	{
	public:
		v3f a, b;

		inline v3f vertex(int k) const
		{
			return { (k & 1) ? b.x : a.x, (k & 2) ? b.y : a.y, (k & 4) ? b.z : a.z };
		}
	} aabb;
	inline aabb computeAABB(const std::vector<v3f>& pts)
	{
		aabb ret =
		{	{ 100000.0f, 100000.0f, 100000.0f },
			{ -100000.0f, -100000.0f, -100000.0f }
		};
		for (const v3f& p : pts)
		{
			ret.a = min(ret.a, p);
			ret.b = max(ret.b, p);
		}
		return ret;
	}
	inline aabb transform(const aabb& box, const m4& mat)
	{
		v4f a = v4f({ box.a.x, box.a.y, box.a.z, 1.0f }) * mat;
		v4f b = v4f({ box.b.x, box.b.y, box.b.z, 1.0f }) * mat;
		return { { a.x, a.y, a.z }, { b.x, b.y, b.z } };
	}
	inline void fixFlatAABB(aabb& box)
	{
		for (int i = 0; i < 3; i++)
		{
			if (box.a[i] == box.b[i])
			{
				box.a[i] -= 0.1f;
				box.b[i] += 0.1f;
			}
		}
	}

	// Public interface
	struct object
	{
	public:
		v3f position = { 0, 0, 0 };
		v3f rotation = { 0, 0, 0 };
		v3f scale = { 1, 1, 1 };

		std::vector<v3f> vertices;
		std::vector<v3f> normals;
		std::vector<v3f> colors;
		std::vector<int> triangles;
	};

	// Window
	void init(const char* windowName = "tinyrender", int width = -1, int height = -1);
	bool shouldQuit();
	bool getKey(int key);
	float deltaTime();
	float globalTime();
	void update();
	void render();
	void swap();
	void terminate();
	v2f getMousePosition();

	// Object management
	int addObject(const object& obj);
	bool removeObject(int id);
	void updateObject(int id, const object& obj);
	void updateObject(int id, const v3f& position, const v3f& rotation, const v3f& scale);
	void updateObject(int id, const std::vector<v3f>& newColors);
	aabb getBoundingBox(int id);

	// Scene parameters
	void setGuizmoEnabled(bool enabled);
	void setDoLighting(bool doLighting);
	void setDrawWireframe(bool drawWireframe);
	void setWireframeThickness(float thickness);
	void setShowNormals(bool showNormals);
	void setCameraEye(const v3f& newEye);
	void setCameraAt(const v3f& newAt);
	void setCameraPlanes(float near, float far);
	void setLightDir(const v3f& newLightDir);

	// Simple mesh API
	int addSphere(float r, int n);
	int addPlane(float size, int n);
	int addBox(float size);
	int addBox(const v3f& a, const v3f& b);
	bool exportObjFile(const char* filename, const object& object);
	bool loadObjFile(const char* filename, object& object);
}

#endif
