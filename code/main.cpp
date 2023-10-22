#include "tinyrender.h"

static void ExampleLoadMesh() {
	tinyrender::object obj;
	bool ret = tinyrender::loadObjFile("../resources/airboat.obj", obj);
	if (!ret)
		return;
	tinyrender::addObject(obj);
}

static void ExamplePrimitives() {
	int id = tinyrender::addSphere(1.0f, 16);

	id = tinyrender::addPlane(1.0f, 4);
	tinyrender::updateObject(id, { -2.5f, 0.f, 0.f }, { 0, 0, 0 }, { 1.0f, 1.0f, 1.0f });

	id = tinyrender::addBox(1.0f);
	tinyrender::updateObject(id, { 2.5f, 0.f, 0.f }, { 0, 0, 0 }, { 1.0f, 1.0f, 1.0f });
}

int main() {
	tinyrender::init("tinyrender", 800, 600);
	tinyrender::setCameraAt(0.f, 0.f, 0.f);
	tinyrender::setCameraEye(0.f, 1.f, -10.0f);

	//ExampleLoadMesh();
	ExamplePrimitives();

	while (!tinyrender::shouldQuit())
	{
		tinyrender::update();
		tinyrender::render();
		tinyrender::swap();
	}
	tinyrender::terminate();
	return 0;
}
