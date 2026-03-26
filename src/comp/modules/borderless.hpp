#pragma once

namespace comp
{
	class borderless final : public shared::common::loader::component_module
	{
	public:
		borderless() = default;

		// Called from comp::main() once shared::globals::main_window is valid.
		static void apply_to_window(HWND hwnd);
	};
}
