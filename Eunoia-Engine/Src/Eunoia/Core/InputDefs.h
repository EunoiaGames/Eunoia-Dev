#pragma once

#include "../Common.h"

namespace Eunoia {

	EU_REFLECT()
	enum Gamepad
	{
		EU_GAMEPAD_0,
		EU_GAMEPAD_1,
		EU_GAMEPAD_2,
		EU_GAMEPAD_3,
		EU_NUM_GAMEPADS,
		EU_FIRST_GAMEPAD = EU_GAMEPAD_0,
		EU_LAST_GAMEPAD = EU_GAMEPAD_3
	};

	EU_REFLECT()
	enum Key
	{
		EU_KEY_ERROR = 0x00,
		EU_KEY_ESC = 0x01,
		EU_KEY_1 = 0x02,
		EU_KEY_2 = 0x03,
		EU_KEY_3 = 0x04,
		EU_KEY_4 = 0x05,
		EU_KEY_5 = 0x06,
		EU_KEY_6 = 0x07,
		EU_KEY_7 = 0x08,
		EU_KEY_8 = 0x09,
		EU_KEY_9 = 0x0a,
		EU_KEY_0 = 0x0b,
		EU_KEY_DASH = 0x0c,
		EU_KEY_EQUALS = 0x0d,
		EU_KEY_BACKSPACE = 0x0e,
		EU_KEY_TAB = 0x0f,
		EU_KEY_Q = 0x10,
		EU_KEY_W = 0x11,
		EU_KEY_E = 0x12,
		EU_KEY_R = 0x13,
		EU_KEY_T = 0x14,
		EU_KEY_Y = 0x15,
		EU_KEY_U = 0x16,
		EU_KEY_I = 0x17,
		EU_KEY_O = 0x18,
		EU_KEY_P = 0x19,
		EU_KEY_LEFT_BRACKET = 0x1a,
		EU_KEY_RIGHT_BRACKET = 0x1,
		EU_KEY_ENTER = 0x1c,
		EU_KEY_CTL = 0x1d,
		EU_KEY_A = 0x1e,
		EU_KEY_S = 0x1f,
		EU_KEY_D = 0x20,
		EU_KEY_F = 0x21,
		EU_KEY_G = 0x22,
		EU_KEY_H = 0x23,
		EU_KEY_J = 0x24,
		EU_KEY_K = 0x25,
		EU_KEY_L = 0x26,
		EU_KEY_SEMI_COLON = 0x27,
		EU_KEY_QUOTE = 0x28,
		EU_KEY_TILDE = 0x29,
		EU_KEY_LEFT_SHIFT = 0x2a,
		EU_KEY_BACK_SLASH = 0x2b,
		EU_KEY_Z = 0x2c,
		EU_KEY_X = 0x2d,
		EU_KEY_C = 0x2e,
		EU_KEY_V = 0x2f,
		EU_KEY_B = 0x30,
		EU_KEY_N = 0x31,
		EU_KEY_M = 0x32,
		EU_KEY_COMMA = 0x33,
		EU_KEY_PERIOD = 0x34,
		EU_KEY_SLASH = 0x35,
		EU_KEY_RIGHT_SHIFT = 0x36,
		EU_KEY_KP_ASTERISK = 0x37,
		EU_KEY_LEFT_ALT = 0x38,
		EU_KEY_SPACE = 0x39,
		EU_KEY_CAPS_LOCK = 0x3a,
		EU_KEY_F1 = 0x3b,
		EU_KEY_F2 = 0x3c,
		EU_KEY_F3 = 0x3d,
		EU_KEY_F4 = 0x3e,
		EU_KEY_F5 = 0x3f,
		EU_KEY_F6 = 0x40,
		EU_KEY_F7 = 0x41,
		EU_KEY_F8 = 0x42,
		EU_KEY_F9 = 0x43,
		EU_KEY_F10 = 0x44,
		EU_KEY_NUM_LOCK = 0x45,
		EU_KEY_SCROLL_LOCK = 0x46,
		EU_KEY_KP_7 = 0x47,
		EU_KEY_KP_8 = 0x48,
		EU_KEY_KP_9 = 0x49,
		EU_KEY_KP_MINUS = 0x4a,
		EU_KEY_KP_4  = 0x4b,
		EU_KEY_KP_5 = 0x4c,
		EU_KEY_KP_6 = 0x4d,
		EU_KEY_KP_PLUS = 0x4e,
		EU_KEY_KP_1 = 0x4f,
		EU_KEY_KP_2 = 0x50,
		EU_KEY_KP_3 = 0x51,
		EU_KEY_KP_0 = 0x52,
		EU_KEY_KP_DEL = 0x53,
		EU_KEY_SYS_RQ = 0x54,
		EU_KEY_BLANK = 0x56,
		EU_KEY_F11 = 0x57,
		EU_KEY_F12 = 0x58,
		EU_KEY_UP = EU_KEY_KP_8,
		EU_KEY_DOWN = EU_KEY_KP_2,
		EU_KEY_LEFT = EU_KEY_KP_4,
		EU_KEY_RIGHT = EU_KEY_KP_6,

		EU_NUM_KEYS
	};

	EU_REFLECT()
	enum MouseButton
	{
		EU_BUTTON_LEFT,
		EU_BUTTON_RIGHT,
		EU_BUTTON_EXTRA_1,
		EU_BUTTON_EXTRA_2,
		EU_BUTTON_EXTRA_3,
		EU_MOUSE_WHEEL,

		EU_NUM_MOUSE_BUTTONS
	};

	EU_REFLECT()
	enum GamepadButton
	{
		//Copied values from <XInput.h>
		EU_GAMEPAD_XBOX360_BUTTON_DPAD_UP          =		0x0001,
		EU_GAMEPAD_XBOX360_BUTTON_DPAD_DOWN        =		0x0002,
		EU_GAMEPAD_XBOX360_BUTTON_DPAD_LEFT        =		0x0004,
		EU_GAMEPAD_XBOX360_BUTTON_DPAD_RIGHT       =		0x0008,
		EU_GAMEPAD_XBOX360_BUTTON_START            =		0x0010,
		EU_GAMEPAD_XBOX360_BUTTON_BACK             =		0x0020,
		EU_GAMEPAD_XBOX360_BUTTON_LEFT_THUMB       =		0x0040,
		EU_GAMEPAD_XBOX360_BUTTON_RIGHT_THUMB      =		0x0080,
		EU_GAMEPAD_XBOX360_BUTTON_LEFT_SHOULDER    =		0x0100,
		EU_GAMEPAD_XBOX360_BUTTON_RIGHT_SHOULDER   =		0x0200,
		EU_GAMEPAD_XBOX360_BUTTON_A                =		0x1000,
		EU_GAMEPAD_XBOX360_BUTTON_B                =		0x2000,
		EU_GAMEPAD_XBOX360_BUTTON_X                =		0x4000,
		EU_GAMEPAD_XBOX360_BUTTON_Y                =		0x8000,
		EU_GAMEPAD_XBOX360_BUTTON_FIRST_BUTTON		=		EU_GAMEPAD_XBOX360_BUTTON_DPAD_UP,
		EU_GAMEPAD_XBOX360_BUTTON_LAST_BUTTON		=		EU_GAMEPAD_XBOX360_BUTTON_Y,
		EU_GAMEPAD_XBOX360_BUTTON_LT,
		EU_GAMEPAD_XBOX360_BUTTON_RT,
		EU_NUM_GAMEPAD_BUTTONS				=		14
	};

	EU_REFLECT()
	enum GamepadTrigger
	{
		EU_GAMEPAD_XBOX360_TRIGGER_LT,
		EU_GAMEPAD_XBOX360_TRIGGER_RT,
		EU_NUM_GAMEPAD_TRIGGERS
	};

	EU_REFLECT()
	enum GamepadThumbstick
	{
		EU_GAMEPAD_XBOX360_THUMBSTICK_LEFT,
		EU_GAMEPAD_XBOX360_THUMBSTICK_RIGHT,
		EU_NUM_GAMEPAD_THUMBSTICKS
	};
}