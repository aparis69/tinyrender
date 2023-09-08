#ifndef TINYRENDER_CPP_INCLUDED
#define TINYRENDER_CPP_INCLUDED

#include "../dependency/GL/glew.h"
#include "../dependency/GLFW/glfw3.h"
#include "../dependency/imgui/imgui.h"

#include <cmath>
#include <vector>

namespace tinyrender
{
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
		return v3f({ l.x - r.x, l.y - r.y, l.z - r.z });
	}
	inline v3f operator+(const v3f& l, const v3f& r)
	{
		return { l.x + r.x, l.y + r.y, l.z + r.z };
	}
	inline v3f operator*(const v3f& l, float x)
	{
		return v3f({ l.x * x, l.y * x, l.z * x });
	}
	inline v3f operator/(const v3f& l, float x)
	{
		return v3f({ l.x / x, l.y / x, l.z / x });
	}
	inline v3f operator-(const v3f& l)
	{
		return v3f({ -l.x, -l.y, -l.z });
	}
	inline float radian(float degrees)
	{
		return degrees * static_cast<float>(0.01745329251994329576923690768489);
	}

	// Public interface
	struct object
	{
	public:
		v3f position = { 0, 0, 0 };
		v3f scale = { 1, 1, 1 };

		std::vector<v3f> vertices;
		std::vector<v3f> normals;
		std::vector<v3f> colors;
		std::vector<int> triangles;
	};
	void init(int width, int height);
	bool shouldQuit();
	bool getKey(int key);
	float deltaTime();
	float globalTime();
	void update();
	void render();
	void swap();
	void terminate();

	// 3D objects
	int pushObject(const object& obj);
	void updateObject(int id, const object& obj);
	void updateObject(int id, const v3f& position, const v3f& scale);
	void updateObjectColors(int id, const std::vector<v3f>& newColors);
	void popObject(int id);
	void popFirst();

	// Parameters
	void setDoLighting(bool doLighting);
	void setDrawWireframe(bool drawWireframe);
	void setShowNormals(bool showNormals);

	// Provided simple mesh creation
	// TODO: provide more function for sphere and box
	int pushPlaneRegularMesh(float size, int n);

	// Camera
	void setCameraEye(float x, float y, float z);
	void setCameraAt(float x, float y, float z);
	void setCameraPlanes(float near, float far);
	void setLightDir(float x, float y, float z);
}

#endif
