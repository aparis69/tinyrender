#include "tinyrender.h"

static void ExampleLoadMesh() 
{
	tinyrender::init("tinyrender - loading mesh", 800, 600);
	tinyrender::setCameraAt({ 0.f, 0.f, 0.f });
	tinyrender::setCameraEye({ 0.f, 1.f, -10.0f });

	tinyrender::object obj;
	bool ret = tinyrender::loadObjFile("../resources/airboat.obj", obj);
	if (!ret)
		return;
	tinyrender::addObject(obj);

	while (!tinyrender::shouldQuit())
	{
		tinyrender::update();
		tinyrender::render();
		tinyrender::swap();
	}
	tinyrender::terminate();
}

static void ExamplePrimitives() 
{
	tinyrender::init("tinyrender - primitives", 800, 600);
	tinyrender::setCameraAt({ 0.f, 0.f, 0.f });
	tinyrender::setCameraEye({ 0.f, 1.f, -10.0f });

	int id = tinyrender::addSphere(1.0f, 16);

	id = tinyrender::addPlane(1.0f, 4);
	tinyrender::updateObject(id, { -2.5f, 0.f, 0.f }, { 0, 0, 0 }, { 1.0f, 1.0f, 1.0f });

	id = tinyrender::addBox(1.0f);
	tinyrender::updateObject(id, { 2.5f, 0.f, 0.f }, { 0, 0, 0 }, { 1.0f, 1.0f, 1.0f });

	while (!tinyrender::shouldQuit())
	{
		tinyrender::update();
		tinyrender::render();
		tinyrender::swap();
	}
	tinyrender::terminate();
}

int main() 
{
	//ExampleLoadMesh();
	ExamplePrimitives();

	return 0;
}
