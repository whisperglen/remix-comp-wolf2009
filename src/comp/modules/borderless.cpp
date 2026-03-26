#include "std_include.hpp"
#include "borderless.hpp"
#include "chimera/window.hpp"

namespace comp
{
	void borderless::apply_to_window(HWND hwnd)
	{
		comp::chimera::window::apply_to_window(hwnd);
	}
}
