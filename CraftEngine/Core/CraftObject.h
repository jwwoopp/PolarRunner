#pragma once

#include <Core/Core.h>
#include <memory>

namespace Craft
{
	// 커스텀 타입 시스템을 제공하는 최상위 클래스.
	// 단일 상속만 지원.
	class CRAFT_API CraftObject
	{
	public:
		// 가상 소멸자.
		virtual ~CraftObject() = default;

		// 현재 객체 타입의 ID를 반환하는 함수.
		// 순수 가상함수로 만들기 -> 상속하는 계층에 구현을 강제.
		// C++에서 인터페이스를 사용할 때 이렇게 구성.
		virtual size_t GetType() const = 0;

		// 전달된 다른 타입 ID와 현재 객체(부모 포함)의 타입 비교.
		virtual bool Is(size_t id) const
		{
			// 최상위 객체인 CraftObject의 함수까지 접근했다는 건
			// 타입이 다르다는 것과 같은 의미.
			return false;
		}

		// 타입 질문 함수.
		// 이 함수를 사용하기 위해서는 T 타입이 static TypeId 함수를 가져야만 함.
		template<typename T>
		bool IsTypeOf() const
		{
			return Is(T::TypeId());
		}
	};

	// 형변환 함수 - 스마트 포인터 간의 변환 처리.
	template<typename T, typename U>
	std::shared_ptr<T> Cast(const std::shared_ptr<U>& object)
	{
		// 예외 처리.
		if (!object)
		{
			return nullptr;
		}

		// object의 실제 타입이 T(또는 T의 파생)인지 확인 후 형변환(캐스팅).
		if (object->Is(T::TypeId()))
		{
			// 가벼운 형변환을 통해 반환.
			// static_pointer_cast는 런타임에 타입 검증을 하지 않기 때문에
			// 상대적으로 가벼움.
			return std::static_pointer_cast<T>(object);
		}

		// 타입 검사에 실패하면 null 반환.
		return nullptr;
	}
}

// 타입 시스템을 사용하는 클래스에 배치할 매크로.
// 반복적인 코드 자동화를 할 때 많이 활용됨.
#define TYPE_DECLARATIONS(Type, ParentType)							\
	using super = ParentType;										\
protected:															\
	/* 전역 지역 변수의 주소를 활용해 유니크한 id를 반환하는 함수 */			\
	static size_t TypeIdClass()										\
	{																\
		static int runTimeTypeId = 0;								\
		return reinterpret_cast<size_t>(&runTimeTypeId);			\
	}																\
public:																\
	static size_t TypeId()											\
	{																\
		return Type::TypeIdClass();									\
	}																\
	virtual size_t GetType() const override							\
	{																\
		return Type::TypeIdClass();									\
	}																\
	virtual bool Is(size_t id) const override						\
	{																\
		return (id == TypeIdClass()) ? true : ParentType::Is(id);	\
	}