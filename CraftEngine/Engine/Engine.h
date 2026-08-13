#pragma once

#include <Core/Core.h>
#include <memory>		// 스마트 포인터 사용을 위해.
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>

enum VarType
{
	None,
	Integer,
	Float,
	String,
	Boolean
};

class Variable
{
public:
	const char* GetName() const { return name; }

private:
	// 변수 이름 (framerate).
	char* name = nullptr;

	// 변수 타입 (열거형).
	VarType type = VarType::None;

	// 값
	void* value = nullptr;

};

inline std::unordered_map<std::string, Variable> variableMap;
inline std::vector<Variable> variableList;


// CraftEngine 프로젝트 안의 클래스는 Craft 네임 스페이스 사용.
namespace Craft
{
	// 전방 선언.
	class Level;
	class Input;
	class Renderer;
	class CollisionSystem;

	// 메인 엔진 클래스.
	// 엔진 루프를 제공.
	// 게임 엔진의 핵심 기능 제공.
	class CRAFT_API Engine
	{
		// 엔진 설정 (데이터).
		struct Setting
		{

			// 목표 프레임 수 (초당 프레임).
			float framerate = 0.0f;

			// 사용할 콘솔 화면 너비.
			int width = 0;

			// 사용할 콘솔 화면 높이.
			int height = 0;
		};

	public:
		Engine();
		virtual ~Engine();
		// 상속을 염두해두면 소멸자에 virtual을 추가 요망.

		// 엔진 실행 함수.
		void Run();

		// 엔진 종료 함수.
		void Quit();

		// 레벨 추가 요청 함수.
		// 1. std::is_based_of 하는 일이 무엇인지
		// 2. std::enable_if_t 하는 일이 무엇인지
		// 3. typename = std::enable_if_t<std::is_base_of<Level, T>::value>>
		template<typename T,
			typename = std::enable_if_t<std::is_base_of<Level, T>::value>>
			void AddNewLevel()
		{
			// 추가 요청 레벨 객체 생성.
			nextLevel = std::make_shared<T>();
		}

		// 메인 레벨과 별개로 겹쳐서 띄우는(오버레이) 보조 레벨 생성 함수.
		// 예) 일시정지 메뉴처럼 메인 레벨 상태를 유지한 채 보여줘야 하는 화면.
		template<typename T, typename ...Args,
			typename = std::enable_if_t<std::is_base_of<Level, T>::value>>
			void SetSecondaryLevel(Args&& ...args)
		{
			secondaryLevel = std::make_shared<T>(std::forward<Args>(args)...);
		}

		// 보조 레벨을 새로 만들지 않고 표시 여부만 켜고 끄는 함수.
		// (보조 레벨이 켜져 있으면 메인 레벨은 Tick이 멈추고 Draw만 계속되어
		// 배경으로 남는다.)
		inline void SetSecondaryLevelActive(bool active)
		{
			secondaryLevelActive = active;
		}
		inline bool IsSecondaryLevelActive() const
		{
			return secondaryLevelActive;
		}
		inline std::shared_ptr<Level> GetSecondaryLevel() const
		{
			return secondaryLevel;
		}

		// 전역 접근 함수.
		static Engine& Get();

		// Getter.
		inline int GetWidth() const { return setting.width; }
		inline int GetHeight() const { return setting.height; }

	protected:
		// 입력 처리 함수 (입력 폴링). 반대는 이벤트 기반 (특정 이벤트가 들어올 때 처리).
		void ProcessInput();

		// 초기화 함수.
		void OnInitialized();

		// 게임 플레이 이벤트 함수.
		
		// 게임 플레이 초기화 함수.
		void BeginPlay();

		// 게임 플레이 업데이트 함수.
		void Tick(float deltaTime);

		// 레벨 그리기 함수.
		void Draw();

		// 충돌 처리 함수.
		void ProcessCollision();

		// 프레임 간 입력 값 저장을 위한 함수.
		void SavePreviousInputStates();

		// 엔진 종료 시 정리가 필요할 때 사용할 함수.
		void Shutdown();

		// 엔진 설정 로드 함수.
		void LoadEngineSetting();

	protected:
		// 엔진 종료 요청 여부 프래그.
		bool isQuit = false;

		// 엔진 설정 함수.
		Setting setting;

		// 전역 접근이 가능하도록 변수 선언.
		static Engine* instance;

		// 메인 레벨.
		std::shared_ptr<Level> mainLevel;

		// 추가 요청된 레벨.
		std::shared_ptr<Level> nextLevel;

		// 메인 레벨 위에 겹쳐 띄우는 보조 레벨과 표시 여부.
		std::shared_ptr<Level> secondaryLevel;
		bool secondaryLevelActive = false;

		// 입력 시스템 변수.
		std::unique_ptr<Input> input;

		// 렌더러.
		std::unique_ptr<Renderer> renderer;

		// 콜리전 시스템.
		std::unique_ptr<CollisionSystem> collisionSystem;
	};
}

