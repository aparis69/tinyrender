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

	// Load the raw obj
	obj.vertices.resize(attrib.vertices.size() / 3);
	obj.normals.resize(attrib.vertices.size() / 3);
	for (size_t s = 0; s < shapes.size(); s++) {
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
			int fv = shapes[s].mesh.num_face_vertices[f];
			for (size_t v = 0; v < fv; v++) {
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
				obj.vertices[idx.vertex_index] = 
				{
					attrib.vertices[3 * idx.vertex_index + 0],
					attrib.vertices[3 * idx.vertex_index + 1],
					attrib.vertices[3 * idx.vertex_index + 2]
				};
				if (idx.normal_index >= 0)
				{
					obj.normals[idx.vertex_index] = {
						attrib.normals[3 * idx.normal_index + 0],
						attrib.normals[3 * idx.normal_index + 1],
						attrib.normals[3 * idx.normal_index + 2]
					};
				}
				obj.triangles.push_back(idx.vertex_index);
			}
			index_offset += fv;
		}
	}

	// Make sure the loaded object has normals
	bool hasNormals = attrib.normals.size() > 0;
	if (!hasNormals)
	{
		for (int i = 0; i < obj.triangles.size(); i += 3)
		{
			const auto& v0 = obj.vertices[obj.triangles[i + 0]];
			const auto& v1 = obj.vertices[obj.triangles[i + 1]];
			const auto& v2 = obj.vertices[obj.triangles[i + 2]];
			tinyrender::v3f n = tinyrender::internalCross((v1 - v0), (v2 - v0));
			obj.normals[obj.triangles[i + 0]] += n;
			obj.normals[obj.triangles[i + 1]] += n;
			obj.normals[obj.triangles[i + 2]] += n;
		}
		for (int i = 0; i < obj.normals.size(); i++)
			obj.normals[i] = tinyrender::internalNormalize(obj.normals[i]);
	}
}

int main() {
	tinyrender::init(800, 600);
	tinyrender::setCameraAt(0.f, 0.f, 0.f);
	tinyrender::setCameraEye(-10.f, 1.f, 0.f);
	
	tinyrender::object obj;
	LoadMesh("../resources/airboat.obj", obj);
	obj.colors.resize(obj.vertices.size());
	for (auto& col : obj.colors) {
		col = { .6f, .6f, .6f };
	}
	int id = tinyrender::pushObject(obj);

	while (!tinyrender::shouldQuit())
	{
		tinyrender::update();
		tinyrender::render();

		ImGui::Begin("Scene settings");
		static bool doLighting = true;
		if (ImGui::Checkbox("Lighting", &doLighting))
			tinyrender::setDoLighting(doLighting);
		static bool drawWireframe = true;
		if (ImGui::Checkbox("Wireframe", &drawWireframe))
			tinyrender::setDrawWireframe(drawWireframe);
		static bool showNormals = false;
		if (ImGui::Checkbox("Show Normals", &showNormals))
			tinyrender::setShowNormals(showNormals);
		ImGui::End();

		tinyrender::swap();
	}

	tinyrender::terminate();
	return 0;
}
