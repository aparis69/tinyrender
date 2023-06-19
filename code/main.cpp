#include "tinyrender.h"

int main()
{
	tinyrender::init(800, 600);
	while (!tinyrender::shouldQuit())
	{
		tinyrender::update();

		// Objects
		tinyrender::render();

		// GUI goes here
		ImGui::Begin("Window");
		{
			ImGui::Text("This is a text");
		}
		ImGui::End();

		// Finalize
		tinyrender::swap();
	}
	tinyrender::terminate();
	return 0;
}
