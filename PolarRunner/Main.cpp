#include <Engine/Engine.h>
#include <Level/TitleLevel.h>

int main()
{
	Craft::Engine engine;
	engine.PlayBGM("main.wav", true);
	engine.AddNewLevel<TitleLevel>();
	engine.Run();
	return 0;
}

