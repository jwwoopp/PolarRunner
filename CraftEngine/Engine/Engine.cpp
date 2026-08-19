#include "Engine.h"
#include <Level/Level.h>
#include <Input/Input.h>
#include <Render/Renderer.h>
#include <Physics/CollisionSystem.h>
#include <SoundSystem/Sound.h>

#include <iostream>
#include <Windows.h>
#include <cassert>

namespace Craft
{
	// 전역 변수 초기화.
	Engine* Engine::instance = nullptr;

	Engine::Engine()
	{
		// ! -> Not -> 불리언 값을 뒤집음.
		// !true => false;

		// instance 초기화.
		assert(!instance && "instance is not null");
		instance = this;

		// 엔진 설정 로드.
		LoadEngineSetting();

		// 입력 객체 생성.
		input = std::make_unique<Input>();

		// 렌더러 객체 생성.
		renderer = std::make_unique<Renderer>(
			Vector2(setting.width, setting.height)
		);

		// 콜리전 시스템 객체 생성.
		collisionSystem = std::make_unique<CollisionSystem>();
	
		// 사운드 시스템 객체 생성.
		sound = std::make_unique<Sound>();
	}

	Engine::~Engine()
	{
		instance = nullptr;
	}

	void Engine::Run()
	{
		// 고해상도 타이머 사용.
		// 밀리세컨드 - 1/1000초 -> 해상도 1000.
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);

		// 현재 시간 읽기.
		LARGE_INTEGER counter;
		QueryPerformanceCounter(&counter);

		// 프레임 계산을 위한 변수.
		int64_t current = counter.QuadPart;
		int64_t previous = current;

		// 고정 프레임으로 만들기 위한 값.
		float oneFrameTime = 1.0f / setting.framerate;

		// 엔진 루프.
		while (true)
		{
			// 종료 조건 처리.
			if (isQuit)
			{
				break;
			}

			// 프레임 처리.

			// 입력 처리.
			ProcessInput();

			// 프레임 시간 계산.
			// 1. 현재 시간 읽기.
			QueryPerformanceCounter(&counter);
			current = counter.QuadPart;

			// 2. (현재 시간 - 이전 시간) / 시간 단위(해상도)
			//    -> 초단위로 변환.
			//    예) 밀리세컨드(1/1000초). 200밀리세컨드

			float deltaTime
				= static_cast<float>(current - previous)
				/ static_cast<float>(frequency.QuadPart);

			// 고정 프레임 처리.
			// 프레임 사이에 걸린 시간이 목표 시간보다 더 많이 지났으면
			// 프레임 처리.
			if (deltaTime >= oneFrameTime)
			{

				// 게임 이벤트 함수 호출.
				OnInitialized();

				// 게임 이벤트의 초기화 함수(1번만 호출).
				BeginPlay();

				// 게임 업데이트.
				Tick(deltaTime);

				// 충돌 처리.
				ProcessCollision();

				// 화면 그리기.
				Draw();

				//  이 위까지 호출이 완료되면 프레임 처리 완료됨.

				// 레벨 전환 처리.
				if (nextLevel)
				{
					// 기존 레벨 정리.
					if (mainLevel)
					{
						mainLevel.reset();
					}

					// 추가 요청된 레벨을 메인 레벨로 설정.
					mainLevel = nextLevel;

					// 포인터 정리.
					nextLevel.reset();
				}

					Sleep(1);

				// 추가/제거 요청된 액터 정리.
				if (mainLevel)
				{
					mainLevel->ProcessAddAndDestroyActors();

					// 액터의 이전 상태 저장 처리.
					mainLevel->SavePreviousActorStates();
				}

				if (secondaryLevel)
				{
					secondaryLevel->ProcessAddAndDestroyActors();
					secondaryLevel->SavePreviousActorStates();
				}

				// 입력 상태 저장.
				SavePreviousInputStates();

				// 현재 시간을 이전 시간으로 저장.
				previous = current;
			}
		}

		// 종료 처리 함수 호출.
		Shutdown();
	}

	void Engine::Quit()
	{
		// 엔진 종료 플래그 설정.
		isQuit = true;
	}

	void Engine::PlayOneShot(const std::string& filename)
	{
		if (!sound)
		{
			return;
		}

		// 사운드 시스템 함수 호출.
		sound->PlayOneShot(std::string("../Assets/Sound/")+ filename);

	}
	void Engine::PlayBGM(const std::string& filename, bool loop)
	{
		if (!sound)
		{
			return;
		}

		// 사운드 시스템 함수 호출.
		sound->PlayBGM(std::string("../Assets/Sound/")+ filename, loop);

	}
	void Engine::StopBGM()
	{
		if (!sound)
		{
			return;
		}

		// 사운드 시스템 함수 호출.
		sound->StopBGM();
	}

	Engine& Engine::Get()
	{
		// 검증 - 어서트(어써트/assert).
		// 무조건(필수로) 통화해야하는 조건이 있을 때 사용.
		// 디버그 모드에서만 동작.
		assert(instance && "instance is null");
		return *instance;
	}

	void Engine::ProcessInput()
	{
		assert(input && "input should not be null here");
		if (!input)
		{
			return;
		}
		input->ProcessInput();
	}


	void Engine::OnInitialized()
	{
		// 레벨 초기화 처리.
		if (mainLevel && !mainLevel->HasInitialized())
		{
			// 초기화 이벤트 호출.
			mainLevel->OnInitialized();
		}

		if (secondaryLevel && !secondaryLevel->HasInitialized())
		{
			secondaryLevel->OnInitialized();
		}
	}

	void Engine::BeginPlay()
	{
		if (mainLevel)
		{
			// 레벨에 이벤트 전달.
			mainLevel->BeginPlay();
		}

		if (secondaryLevel)
		{
			secondaryLevel->BeginPlay();
		}
	}
	void Engine::Tick(float deltaTime)
	{
		// 보조 레벨이 켜져 있는 동안에는 메인 레벨의 Tick(게임플레이 갱신)을
		// 멈춘다. 예) 일시정지 메뉴가 떠 있을 때 게임 상태는 그대로 유지.
		if (secondaryLevelActive && secondaryLevel)
		{
			secondaryLevel->Tick(deltaTime);
			return;
		}

		if (!mainLevel)
		{
			return;
		}

		// 레벨에 이벤트 전달.
		mainLevel->Tick(deltaTime);

	}

	void Engine::Draw()
	{
		if (mainLevel)
		{
			// 메인 레벨은 보조 레벨이 켜져 있어도 배경으로 계속 그린다.
			mainLevel->Draw();
		}

		if (secondaryLevelActive && secondaryLevel)
		{
			secondaryLevel->Draw();
		}

		// 렌더러에 Draw 이벤트 호출.
		if (!renderer)
		{
			return;
		}

		renderer->Draw();
	}

	void Engine::ProcessCollision()
	{
		// 예외처리.
		if (!mainLevel || !collisionSystem)
		{
			return;
		}

		// 충돌처리.
		// 의존성 주입(Dependency Injection).
		collisionSystem->ProcessCollision(mainLevel->actorList);
	}

	void Engine::SavePreviousInputStates()
	{
		assert(input && "input should not be null here");
		if (!input)
		{
			return;
		}
		input->SavePreviousStates();
	}

	void Engine::Shutdown()
	{
	}
	void Engine::LoadEngineSetting()
	{
		// 파일 열기 (개행 문자 처리를 쉽게 텍스트 모드로 열기).
		FILE* file = nullptr;
		fopen_s(&file, "../Config/Setting.txt", "rt");

		// 예외처리.
		if (!file)
		{
			std::cout << "Failed to open engine setting file.\n";

			// 디버그 모드에서 강제 중단 시키는 기능.
			__debugbreak();
			return;
		}

		// 데이터 읽어오기.
		const int bufferSize = 2048;
		char buffer[bufferSize] = {};

		size_t readSize
			= fread(buffer, sizeof(char), bufferSize, file);

		// 값 저장을 위해 서식 해석 (파싱-Parsing).
		// 문자열 자르기(Split).
		char* context = nullptr;
		char* token = nullptr;
		// 파일에서 읽은 전체 문자열을 개행(\n)문자 기준으로 처음 자르기.
		token = strtok_s(buffer, "\n", &context);

		// 반복해서 자르기
		while (token)
		{
			// 공백 전까지 읽은 문자열을 저장할 변수.
			char key[15] = {};

			// 포맷을 지정한 문자열 읽기.
			// 공백 문자를 만나면 그 전까지 읽어서 저장.
			sscanf_s(token, "%s", key, 15);

			// 키 값을 비교해서 값 설정.
			if (strcmp(key, "framerate") == 0)
			{
				sscanf_s(token, "framerate = %f", &setting.framerate);
			}
			else if (strcmp(key, "width") == 0)
			{
				sscanf_s(token, "width = %d", &setting.width);
			}
			else if (strcmp(key, "height") == 0)
			{
				sscanf_s(token, "height = %d", &setting.height);
			}

			// 나머지 문자열 자르기(개행 문자 기준으로).
			token = strtok_s(nullptr, "\n", &context);
		}

		// 파일 닫기.
		fclose(file);
		file = nullptr;

		// 고정 설정값 대신 현재 콘솔 창의 전체 영역을 게임 화면으로 사용한다.
		// 콘솔 정보를 가져오지 못한 경우에는 Setting.txt의 크기를 그대로 사용한다.
		CONSOLE_SCREEN_BUFFER_INFO consoleInfo = {};
		if (GetConsoleScreenBufferInfo(
			GetStdHandle(STD_OUTPUT_HANDLE),
			&consoleInfo))
		{
			setting.width = consoleInfo.srWindow.Right
				- consoleInfo.srWindow.Left + 1;
			setting.height = consoleInfo.srWindow.Bottom
				- consoleInfo.srWindow.Top + 1;
		}
	}
}
