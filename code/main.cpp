#include "tinyrender.h"

int main()
{
	tinyrender::init(800, 600);
	tinyrender::setCameraAt(0.f, 0.f, 0.f);
	tinyrender::setCameraEye(-10.f, 1.f, 0.f);
	tinyrender::pushPlaneRegularMesh(10.0f, 256);

	while (!tinyrender::shouldQuit())
	{
		tinyrender::update();
		tinyrender::render();
		tinyrender::swap();
	}
	tinyrender::terminate();
	return 0;
}
