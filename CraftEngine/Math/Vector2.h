#pragma once

#include <Core/Core.h>
#include <Windows.h>

namespace Craft
{
	class CRAFT_API Vector2
	{
	public:
		Vector2(int x = 0, int y = 0);
		~Vector2() = default;

		// 연산자 오버로딩.

		// Windows 콘솔 좌표계로 변환하는 연산자 오버로딩.
		operator COORD() const;
		operator COORD();

		// 사칙 연산자 오버로딩.
		Vector2 operator+(const Vector2& other) const;
		Vector2 operator-(const Vector2& other) const;
		Vector2 operator*(const Vector2& other) const;
		Vector2 operator/(const Vector2& other) const;

		// 대입 연산자 오버로딩.
		Vector2& operator=(const Vector2& other);

		// 비교 연산자 오버로딩.
		bool operator==(const Vector2& other) const;
		bool operator!=(const Vector2& other) const;

		// 자주 사용할 값을 전역 변수로 선언.
		static Vector2 Zero;
		static Vector2 One;
		static Vector2 Right;
		static Vector2 Up;

	public:
		// 좌표계 값으로 사용하기 위한 변수.
		int x = 0;
		int y = 0;
	};
}