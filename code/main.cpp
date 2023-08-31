#include "tinyrender.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "../dependency/tinyobj/tiny_obj_loader.h"
#include <iostream>

static void LoadMesh(const char* path, tinyrender::object& obj) {
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path);
	if (!warn.empty())
		std::cout << warn << std::endl;
	if (!err.empty())
		std::cout << err << std::endl;
	if (!ret)
		return;

	for (size_t s = 0; s < shapes.size(); s++)
	{
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
		{
			int fv = shapes[s].mesh.num_face_vertices[f];
			for (size_t v = 0; v < fv; v++)
			{
				// tinyobj index
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

				// Vertex
				obj.vertices.push_back(
					{
						attrib.vertices[3 * idx.vertex_index + 0],
						attrib.vertices[3 * idx.vertex_index + 1],
						attrib.vertices[3 * idx.vertex_index + 2]
					}
				);
				// Normals
				obj.normals.push_back(
					{
						attrib.normals[3 * idx.normal_index + 0],
						attrib.normals[3 * idx.normal_index + 1],
						attrib.normals[3 * idx.normal_index + 2]
					}
				);
				// Ignore UVs for now
			}
			index_offset += fv;
		}
	}

	// Vertex/Normals are now in order - so indices are easy
	for (int i = 0; i < obj.vertices.size(); i += 3) {
		obj.triangles.push_back(i);
		obj.triangles.push_back(i + 1);
		obj.triangles.push_back(i + 2);
	}
}

static float Uniform() {
	return float(rand()) / float(RAND_MAX);
}

int main() {
	tinyrender::init(800, 600);
	tinyrender::setCameraAt(0.f, 0.f, 0.f);
	tinyrender::setCameraEye(-10.f, 1.f, 0.f);
	tinyrender::pushPlaneRegularMesh(10.0f, 256);
	
	tinyrender::object obj;
	obj.position = { 0.f, 2.f, 0.f };
	LoadMesh("../sphere.obj", obj);
	obj.colors.resize(obj.vertices.size());
	for (auto& col : obj.colors) {
		col = { Uniform(), Uniform(), Uniform() };
	}
	tinyrender::pushObject(obj);

	while (!tinyrender::shouldQuit())
	{
		tinyrender::update();
		tinyrender::render();
		tinyrender::swap();
	}
	tinyrender::terminate();
	return 0;
}
