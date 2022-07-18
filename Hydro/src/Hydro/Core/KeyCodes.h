#pragma once
#include <iostream>

namespace Hydro
{
	typedef enum class KeyCode : uint16_t
	{
		// From glfw3.h
		Space = 32,
		Apostrophe = 39, /* ' */
		Comma = 44, /* , */
		Minus = 45, /* - */
		Period = 46, /* . */
		Slash = 47, /* / */

		D0 = 48, /* 0 */
		D1 = 49, /* 1 */
		D2 = 50, /* 2 */
		D3 = 51, /* 3 */
		D4 = 52, /* 4 */
		D5 = 53, /* 5 */
		D6 = 54, /* 6 */
		D7 = 55, /* 7 */
		D8 = 56, /* 8 */
		D9 = 57, /* 9 */

		Semicolon = 59, /* ; */
		Equal = 61, /* = */

		A = 65,
		B = 66,
		C = 67,
		D = 68,
		E = 69,
		F = 70,
		G = 71,
		H = 72,
		I = 73,
		J = 74,
		K = 75,
		L = 76,
		M = 77,
		N = 78,
		O = 79,
		P = 80,
		Q = 81,
		R = 82,
		S = 83,
		T = 84,
		U = 85,
		V = 86,
		W = 87,
		X = 88,
		Y = 89,
		Z = 90,

		LeftBracket = 91,  /* [ */
		Backslash = 92,  /* \ */
		RightBracket = 93,  /* ] */
		GraveAccent = 96,  /* ` */

		World1 = 161, /* non-US #1 */
		World2 = 162, /* non-US #2 */

		/* Function keys */
		Escape = 256,
		Enter = 257,
		Tab = 258,
		Backspace = 259,
		Insert = 260,
		Delete = 261,
		Right = 262,
		Left = 263,
		Down = 264,
		Up = 265,
		PageUp = 266,
		PageDown = 267,
		Home = 268,
		End = 269,
		CapsLock = 280,
		ScrollLock = 281,
		NumLock = 282,
		PrintScreen = 283,
		Pause = 284,
		F1 = 290,
		F2 = 291,
		F3 = 292,
		F4 = 293,
		F5 = 294,
		F6 = 295,
		F7 = 296,
		F8 = 297,
		F9 = 298,
		F10 = 299,
		F11 = 300,
		F12 = 301,
		F13 = 302,
		F14 = 303,
		F15 = 304,
		F16 = 305,
		F17 = 306,
		F18 = 307,
		F19 = 308,
		F20 = 309,
		F21 = 310,
		F22 = 311,
		F23 = 312,
		F24 = 313,
		F25 = 314,

		/* Keypad */
		KP0 = 320,
		KP1 = 321,
		KP2 = 322,
		KP3 = 323,
		KP4 = 324,
		KP5 = 325,
		KP6 = 326,
		KP7 = 327,
		KP8 = 328,
		KP9 = 329,
		KPDecimal = 330,
		KPDivide = 331,
		KPMultiply = 332,
		KPSubtract = 333,
		KPAdd = 334,
		KPEnter = 335,
		KPEqual = 336,

		LeftShift = 340,
		LeftControl = 341,
		LeftAlt = 342,
		LeftSuper = 343,
		RightShift = 344,
		RightControl = 345,
		RightAlt = 346,
		RightSuper = 347,
		Menu = 348
	} Key;

	inline std::ostream& operator<<(std::ostream& os, KeyCode keyCode)
	{
		os << static_cast<int32_t>(keyCode);
		return os;
	}
}

// From glfw3.h
#define KEY_SPACE           ::Hydro::Key::Space
#define KEY_APOSTROPHE      ::Hydro::Key::Apostrophe    /* ' */
#define KEY_COMMA           ::Hydro::Key::Comma         /* , */
#define KEY_MINUS           ::Hydro::Key::Minus         /* - */
#define KEY_PERIOD          ::Hydro::Key::Period        /* . */
#define KEY_SLASH           ::Hydro::Key::Slash         /* / */
#define KEY_0               ::Hydro::Key::D0
#define KEY_1               ::Hydro::Key::D1
#define KEY_2               ::Hydro::Key::D2
#define KEY_3               ::Hydro::Key::D3
#define KEY_4               ::Hydro::Key::D4
#define KEY_5               ::Hydro::Key::D5
#define KEY_6               ::Hydro::Key::D6
#define KEY_7               ::Hydro::Key::D7
#define KEY_8               ::Hydro::Key::D8
#define KEY_9               ::Hydro::Key::D9
#define KEY_SEMICOLON       ::Hydro::Key::Semicolon     /* ; */
#define KEY_EQUAL           ::Hydro::Key::Equal         /* = */
#define KEY_A               ::Hydro::Key::A
#define KEY_B               ::Hydro::Key::B
#define KEY_C               ::Hydro::Key::C
#define KEY_D               ::Hydro::Key::D
#define KEY_E               ::Hydro::Key::E
#define KEY_F               ::Hydro::Key::F
#define KEY_G               ::Hydro::Key::G
#define KEY_H               ::Hydro::Key::H
#define KEY_I               ::Hydro::Key::I
#define KEY_J               ::Hydro::Key::J
#define KEY_K               ::Hydro::Key::K
#define KEY_L               ::Hydro::Key::L
#define KEY_M               ::Hydro::Key::M
#define KEY_N               ::Hydro::Key::N
#define KEY_O               ::Hydro::Key::O
#define KEY_P               ::Hydro::Key::P
#define KEY_Q               ::Hydro::Key::Q
#define KEY_R               ::Hydro::Key::R
#define KEY_S               ::Hydro::Key::S
#define KEY_T               ::Hydro::Key::T
#define KEY_U               ::Hydro::Key::U
#define KEY_V               ::Hydro::Key::V
#define KEY_W               ::Hydro::Key::W
#define KEY_X               ::Hydro::Key::X
#define KEY_Y               ::Hydro::Key::Y
#define KEY_Z               ::Hydro::Key::Z
#define KEY_LEFT_BRACKET    ::Hydro::Key::LeftBracket   /* [ */
#define KEY_BACKSLASH       ::Hydro::Key::Backslash     /* \ */
#define KEY_RIGHT_BRACKET   ::Hydro::Key::RightBracket  /* ] */
#define KEY_GRAVE_ACCENT    ::Hydro::Key::GraveAccent   /* ` */
#define KEY_WORLD_1         ::Hydro::Key::World1        /* non-US #1 */
#define KEY_WORLD_2         ::Hydro::Key::World2        /* non-US #2 */

/* Function keys */
#define KEY_ESCAPE          ::Hydro::Key::Escape
#define KEY_ENTER           ::Hydro::Key::Enter
#define KEY_TAB             ::Hydro::Key::Tab
#define KEY_BACKSPACE       ::Hydro::Key::Backspace
#define KEY_INSERT          ::Hydro::Key::Insert
#define KEY_DELETE          ::Hydro::Key::Delete
#define KEY_RIGHT           ::Hydro::Key::Right
#define KEY_LEFT            ::Hydro::Key::Left
#define KEY_DOWN            ::Hydro::Key::Down
#define KEY_UP              ::Hydro::Key::Up
#define KEY_PAGE_UP         ::Hazel::Key::PageUp
#define KEY_PAGE_DOWN       ::Hydro::Key::PageDown
#define KEY_HOME            ::Hydro::Key::Home
#define KEY_END             ::Hydro::Key::End
#define KEY_CAPS_LOCK       ::Hydro::Key::CapsLock
#define KEY_SCROLL_LOCK     ::Hydro::Key::ScrollLock
#define KEY_NUM_LOCK        ::Hydro::Key::NumLock
#define KEY_PRINT_SCREEN    ::Hydro::Key::PrintScreen
#define KEY_PAUSE           ::Hydro::Key::Pause
#define KEY_F1              ::Hydro::Key::F1
#define KEY_F2              ::Hydro::Key::F2
#define KEY_F3              ::Hydro::Key::F3
#define KEY_F4              ::Hydro::Key::F4
#define KEY_F5              ::Hydro::Key::F5
#define KEY_F6              ::Hydro::Key::F6
#define KEY_F7              ::Hydro::Key::F7
#define KEY_F8              ::Hydro::Key::F8
#define KEY_F9              ::Hydro::Key::F9
#define KEY_F10             ::Hydro::Key::F10
#define KEY_F11             ::Hydro::Key::F11
#define KEY_F12             ::Hydro::Key::F12
#define KEY_F13             ::Hydro::Key::F13
#define KEY_F14             ::Hydro::Key::F14
#define KEY_F15             ::Hydro::Key::F15
#define KEY_F16             ::Hydro::Key::F16
#define KEY_F17             ::Hydro::Key::F17
#define KEY_F18             ::Hydro::Key::F18
#define KEY_F19             ::Hydro::Key::F19
#define KEY_F20             ::Hydro::Key::F20
#define KEY_F21             ::Hydro::Key::F21
#define KEY_F22             ::Hydro::Key::F22
#define KEY_F23             ::Hydro::Key::F23
#define KEY_F24             ::Hydro::Key::F24
#define KEY_F25             ::Hydro::Key::F25

/* Keypad */
#define KEY_KP_0            ::Hydro::Key::KP0
#define KEY_KP_1            ::Hydro::Key::KP1
#define KEY_KP_2            ::Hydro::Key::KP2
#define KEY_KP_3            ::Hydro::Key::KP3
#define KEY_KP_4            ::Hydro::Key::KP4
#define KEY_KP_5            ::Hydro::Key::KP5
#define KEY_KP_6            ::Hydro::Key::KP6
#define KEY_KP_7            ::Hydro::Key::KP7
#define KEY_KP_8            ::Hydro::Key::KP8
#define KEY_KP_9            ::Hydro::Key::KP9
#define KEY_KP_DECIMAL      ::Hydro::Key::KPDecimal
#define KEY_KP_DIVIDE       ::Hydro::Key::KPDivide
#define KEY_KP_MULTIPLY     ::Hydro::Key::KPMultiply
#define KEY_KP_SUBTRACT     ::Hydro::Key::KPSubtract
#define KEY_KP_ADD          ::Hydro::Key::KPAdd
#define KEY_KP_ENTER        ::Hydro::Key::KPEnter
#define KEY_KP_EQUAL        ::Hydro::Key::KPEqual

#define KEY_LEFT_SHIFT      ::Hydro::Key::LeftShift
#define KEY_LEFT_CONTROL    ::Hydro::Key::LeftControl
#define KEY_LEFT_ALT        ::Hydro::Key::LeftAlt
#define KEY_LEFT_SUPER      ::Hydro::Key::LeftSuper
#define KEY_RIGHT_SHIFT     ::Hydro::Key::RightShift
#define KEY_RIGHT_CONTROL   ::Hydro::Key::RightControl
#define KEY_RIGHT_ALT       ::Hydro::Key::RightAlt
#define KEY_RIGHT_SUPER     ::Hydro::Key::RightSuper
#define KEY_MENU            ::Hydro::Key::Menu

// Mouse (TODO: move into separate file probably)
#define MOUSE_BUTTON_LEFT    0
#define MOUSE_BUTTON_RIGHT   1
#define MOUSE_BUTTON_MIDDLE  2
