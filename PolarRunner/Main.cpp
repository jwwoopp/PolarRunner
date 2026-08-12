#include <Engine/Engine.h>
#include <Level/PolarLevel.h>

int main()
{
	Craft::Engine engine;
	engine.AddNewLevel<PolarLevel>();
	engine.Run();
	return 0;
}

