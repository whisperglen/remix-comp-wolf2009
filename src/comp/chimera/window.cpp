#include "std_include.hpp"
#include "window.hpp"

/*
 * Borderless fullscreen for Halo CE.
 *
 * Halo loads d3d9.dll dynamically after its window is already created, so
 * there is no opportunity to intercept CreateWindowExA from DllMain.
 * Instead, comp::main() calls borderless::apply_to_window() once
 * find_game_window() has located the HWND — restyle and reposition the
 * existing window here.
 */

namespace comp::chimera::window
{
	void apply_to_window(HWND hwnd)
	{
		const int w = GetSystemMetrics(SM_CXSCREEN);
		const int h = GetSystemMetrics(SM_CYSCREEN);

		SetWindowLongA(hwnd, GWL_STYLE,   static_cast<LONG>(WS_POPUP | WS_VISIBLE));
		SetWindowLongA(hwnd, GWL_EXSTYLE, 0);

		SetWindowPos(hwnd, HWND_TOP,
		             0, 0, w, h,
		             SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOZORDER);

		shared::common::log("Borderless",
		    std::format("Applied borderless {}x{} to HWND {:p}", w, h, static_cast<void*>(hwnd)),
		    shared::common::LOG_TYPE::LOG_TYPE_GREEN, true);
	}

} // namespace comp::chimera::window
