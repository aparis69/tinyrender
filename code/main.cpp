#include "tinyrender.h"
#include <time.h>

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
	tinyrender::init("tinyrender - primitives");
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

static void ExampleHeavyScene()
{
	srand(unsigned int(time(NULL)));

	tinyrender::init("tinyrender - primitives", 800, 600);
	tinyrender::setCameraAt({ 0.f, 0.f, 0.f });
	tinyrender::setCameraEye({ 0.f, 0.f, -70.0f });

	const int objectCount = 10000;
	const tinyrender::v3f rotation = { 0, 0, 0 };
	const tinyrender::v3f scale = { 1, 1, 1};
	for (int i = 0; i < objectCount; i++)
	{
		const float x = float(rand() % 50) - 25.0f;
		const float y = float(rand() % 50) - 25.0f;
		const float z = float(rand() % 50) - 25.0f;
		const float r = (float(rand()) / float(RAND_MAX)) * 1.5f + 0.5f;
		const int id = tinyrender::addSphere(r, 16);
		tinyrender::updateObject(id, { x, y, z }, rotation, scale);
	}

	while (!tinyrender::shouldQuit())
	{
		tinyrender::update();
		tinyrender::render();
		tinyrender::swap();
	}
	tinyrender::terminate();
}

static void ExampleAnimatedObjects()
{
	tinyrender::init("tinyrender - primitives", 800, 600);
	tinyrender::setCameraAt({ 0.f, 0.f, 0.f });
	tinyrender::setCameraEye({ 0.f, 1.f, -10.0f });

	const int idBox = tinyrender::addBox(1.0f);
	const int idSphere1 = tinyrender::addSphere(0.5f, 16);
	const int idSphere2 = tinyrender::addSphere(0.25f, 16);

	float rot = 0.0f;
	while (!tinyrender::shouldQuit())
	{
		const float t = tinyrender::globalTime();
		float x = cos(t * 1.5f) * 1.5f;
		float z = sin(t * 1.5f) * 1.5f;
		tinyrender::setObjectPosition(idSphere1, { x, 0.0f, z });
		tinyrender::setObjectPosition(idSphere2, { 0.0f, z, x });

		rot += 40.0f * tinyrender::deltaTime();
		if (rot > 360.0f)
			rot = 0.0f;
		tinyrender::setObjectRotation(idBox, { rot, 0.0f, 0.0f });

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
	//ExampleHeavyScene();
	//ExampleAnimatedObjects();

	return 0;
}
