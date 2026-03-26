#pragma once

namespace comp::chimera::window
{
	// Retroactively restyle an already-created window to borderless fullscreen.
	// Called from comp::main() once shared::globals::main_window is valid.
	void apply_to_window(HWND hwnd);

} // namespace comp::chimera::window
