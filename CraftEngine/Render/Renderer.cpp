#include "Renderer.h"
#include "ScreenBuffer.h"
#include <cassert>
#include <iostream>
#include <Windows.h>

namespace Craft
{
	// ------------- Frame --------------- //
	Renderer::Frame::Frame(int bufferCount)
	{
		// 2차원 배열 생성.
		charInfoArray = std::make_unique<CHAR_INFO[]>(bufferCount);
		sortingOrderArray = std::make_unique<int[]>(bufferCount);
	}

	Renderer::Frame::~Frame()
	{
	}

	// 프레임 초기화 함수.
	void Renderer::Frame::Clear(const Vector2& screenSize)
	{
		// 이중 루프를 순회하면서 값 초기화.
		const int width = screenSize.x;
		const int height = screenSize.y;

		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				// 1차원 배열을 2차원 배열로 사용할 때 
				// 필요한 인덱스 좌표 변환.
				const int index = (y * width) + x;

				// 글자 항목 초기화.
				CHAR_INFO& info = charInfoArray[index];
				// 빈문자 설정 - 기존의 설정된 값 지우기.
				info.Char.AsciiChar = ' ';
				// 색상 표기 안함.
				info.Attributes = 0;

				// 그리기 순서 배열 항목 초기화.
				sortingOrderArray[index] = -1;
			}
		}
	}
	// ------------- Frame --------------- //

	// static 변수 초기화.
	Renderer* Renderer::instance = nullptr;

	Renderer::Renderer(const Vector2& screenSize)
		:screenSize(screenSize)
	{
		// 어서트.
		assert(!instance && "instance should be null");
		instance = this;
		
		// 프레임 객체 생성.
		const int bufferCount = screenSize.x * screenSize.y;
		frame = std::make_unique<Frame>(bufferCount);
		
		// 생성 후 프레임 지우기.
		frame->Clear(screenSize);
	
		// 이중 버퍼 구현을 위한 콘솔 버퍼 생성 및 초기화.
		screenBufferArray[0] = std::make_unique<ScreenBuffer>(screenSize);
		screenBufferArray[0]->Clear();
		
		screenBufferArray[1] = std::make_unique<ScreenBuffer>(screenSize);
		screenBufferArray[1]->Clear();
	
		// 화면에 0번 콘솔 버퍼 활성화.
		SetConsoleActiveScreenBuffer(screenBufferArray[0]->GetBuffer());
	}

	Renderer::~Renderer()
	{
		instance = nullptr;

		// 콘솔 창 원래대로 복구.
		SetConsoleActiveScreenBuffer(GetStdHandle(STD_OUTPUT_HANDLE));

		}

	void Renderer::Submit(
		const std::string& image,
		const Vector2& position,
		Color color,
		int sortingOrder)
	{
		// 렌더 명령 생성 및 값 설정.
		RenderCommand command;
		command.image = image;
		command.position = position;
		command.color = color;
		command.sortingOrder = sortingOrder;

		// 렌더 큐에 명령 추가.
		renderQueue.emplace_back(command);
	}

	void Renderer::Draw()
	{
		// 화면(이미지/프레임) 지우기.
		Clear();

		// 프레임 그리기.
		DrawRenderQueue();

		// 화면(이미지/프레임) 표시.
		Present();
	}

	Renderer& Renderer::Get()
	{
		// 어서트.
		assert(instance && "instance should not be null");
		return *instance;
	}

	void Renderer::Clear()
	{
		// 프레임 값 초기화.
		frame->Clear(screenSize);
		
		// 콘솔 버퍼 초기화.
		GetCurrentBuffer()->Clear();
	}

	void Renderer::DrawRenderQueue()
	{
		// 렌더 큐를 순회하면서 그리기 명령 실행.
		for (const RenderCommand& command : renderQueue)
		{
			// 그릴 문자값이 없으면 건너뛰기.
			if (command.image.empty())
			{
				continue;
			}

			// y 위치가 화면을 벗어났으면 건너뛰기.
			if (command.position.y < 0
				|| command.position.y >= screenSize.y)
			{
				continue;
			}

			// 그리려는 문자열 길이 값.
			const int length = static_cast<int>(command.image.length());

			// 글자의 시작 위치.
			const int startX = command.position.x;

			// 글자의 끝 위치.
			const int endX = startX + length - 1;

			// x 위치가  화면을 벗어났는지 확인.
			if (endX < 0 || startX >= screenSize.x)
			{
				continue;
			}

			// 실제 그릴 글자의 위치 구하기.
			// 삼항 연산자.
			const int visibleStart = startX < 0 ? 0 : startX;
			const int visibleEnd = 
				endX >= screenSize.x ? screenSize.x - 1 : endX;

			// 문자열을 루프 순회하면서 글자를 2차원 배열에 하나씩 기록.
			for (int x = visibleStart; x <= visibleEnd; ++x)
			{
				// 문자열에서 글자값을 가져올때 사용할 인덱스.
				const int sourceIndex = x - startX;

				// 글자 2차원 배열의 인덱스.
				// (y * width) + x
				const int index = (command.position.y * screenSize.x) + x;
			
				// 정렬 순서를 비교해서 그릴지 말지를 판정.
				// 이미 그려진 값이 우선순위가 높으면 건너뛰기.
				// 같거나 새로 그리려는 값이 우선순위가 높으면 덮어쓰기.
				if (frame->sortingOrderArray[index] > command.sortingOrder)
				{
					continue;
				}

				// 2차원 배열 글자, 속성 설정.
				frame->charInfoArray[index].Char.AsciiChar
					= command.image[sourceIndex];

				// 글자 색상 값 설정.
				frame->charInfoArray[index].Attributes
					= static_cast<DWORD>(command.color);

				// 그리기 우선순위 값도 설정.
				frame->sortingOrderArray[index] = command.sortingOrder;
			}
		}

		// 앞에서 설정한 2차원 배열을 콘솔에 그리기.
		GetCurrentBuffer()->Draw(frame->charInfoArray.get());

		// 렌더큐 비우기.
		renderQueue.clear();

		// 콘솔 색상 초기화.
		SetConsoleTextAttribute(
			GetCurrentBuffer()->GetBuffer(),
			static_cast<DWORD>(Color::White)
		);
	}

	void Renderer::Present()
	{
		// 현재 순번의 콘솔 버퍼를 활성화.
		SetConsoleActiveScreenBuffer(GetCurrentBuffer()->GetBuffer());
	
		// 인덱스 업데이트(갱신).
		// currentBufferIndex = (currentBufferIndex + 1) % 2;
		// 0 -> 1 - > 0 -> 1 ...
		// 마법의 공식 -> One Minus...
		currentBufferIndex = 1 - currentBufferIndex;
	}

	const ScreenBuffer* const Renderer::GetCurrentBuffer() const
	{
		return screenBufferArray[currentBufferIndex].get();
	}
}