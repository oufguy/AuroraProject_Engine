#pragma once

#include <U_Variables.h>

namespace ConfigDefaults {
	// Default version of Settings.ini written to disk if the requested config file doesn't exist yet
	inline const String Settings =
		"[Graphics]\n"
		"API = Vulkan                ; Options: Vulkan / DirectX11\n"
		"VSync = false               ; Options: true / false\n"
		"\n"
		"[Window]\n"
		"Mode = Windowed             ; Options: Windowed / Borderless / Fullscreen\n"
		"Width = 1280\n"
		"Height = 720\n"
		"\n"
		"[Input]\n"
		"Sensitivity = 1.0000\n";
};