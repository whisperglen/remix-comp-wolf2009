#include "std_include.hpp"
#include "shared/common/flags.hpp"
#include "chimera/extend_limits.hpp"

namespace comp::game
{

	uintptr_t create_mouse_device_addr = 0;
	CameraRenderMatrices* CameraRenderMatrices_ptr = nullptr;


#define PATTERN_OFFSET_SIMPLE(var, pattern, byte_offset, static_addr) \
		if (const auto offset = shared::utils::mem::find_pattern(##pattern, byte_offset, #var, use_pattern, static_addr); offset) { \
			(var) = offset; found_pattern_count++; \
		} total_pattern_count++;

#define PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(var, type, pattern, byte_offset, static_addr) \
		if (const auto offset = shared::utils::mem::find_pattern(##pattern, byte_offset, #var, use_pattern, static_addr); offset) { \
			(var) = (type)*(DWORD*)offset; found_pattern_count++; \
		} total_pattern_count++;

	void init_game_addresses()
	{
		
		const bool use_pattern = !shared::common::flags::has_flag("no_pattern");
		if (use_pattern) {
			shared::common::log("Game", "Getting offsets ...", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
		}

		std::uint32_t total_pattern_count = 0u;
		std::uint32_t found_pattern_count = 0u;


#pragma region GAME_VARIABLES

		PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(
			CameraRenderMatrices_ptr,
			CameraRenderMatrices*,
			"81 ec a0 02 00 00 53 55 8b ac 24 ac 02 00 00 56 8b 35",
			0x5A,
			0
		);

#pragma endregion // GAME_VARIABLES


#pragma region GAME_FUNCTIONS

#pragma endregion // GAME_FUNCTIONS


#pragma region GAME_ASM_OFFSETS

			PATTERN_OFFSET_SIMPLE(
			create_mouse_device_addr,
			"6a 17 ff 15 ?? ?? ?? ?? 85 c0 74 ?? 66 c7 05 ?? ?? ?? ?? 02 00",
			0,
			0
		);

#pragma endregion // GAME_ASM_OFFSETS


		if (use_pattern)
		{
			if (found_pattern_count == total_pattern_count) {
				shared::common::log("Game", std::format("Found all '{:d}' Patterns.", total_pattern_count), shared::common::LOG_TYPE::LOG_TYPE_GREEN, true);
			}
			else
			{
				shared::common::log("Game", std::format("Only found '{:d}' out of '{:d}' Patterns.", found_pattern_count, total_pattern_count), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
				shared::common::log("Game", ">> Please create an issue on GitHub and attach this console log and information about your game (version, platform etc.)\n", shared::common::LOG_TYPE::LOG_TYPE_STATUS, true);
			}
		}
	}

#undef PATTERN_OFFSET_SIMPLE

	static void write_byte(uintptr_t addr, uint8_t value)
	{
		DWORD old_prot;
		VirtualProtect(reinterpret_cast<LPVOID>(addr), 1, PAGE_EXECUTE_READWRITE, &old_prot);
		*reinterpret_cast<uint8_t*>(addr) = value;
		if (old_prot != PAGE_EXECUTE_READWRITE)
		{
			DWORD dummy;
			VirtualProtect(reinterpret_cast<LPVOID>(addr), 1, old_prot, &dummy);
		}
	}


	void apply_patches()
	{
		// ----------------------------------------------------------------
		// Chimera engine limit extensions
		// ----------------------------------------------------------------
		comp::chimera::extend_limits();


		// ----------------------------------------------------------------
		// P_DontStealMouse pulled from the Halo CE VR mod
		// ----------------------------------------------------------------
		if (create_mouse_device_addr)
		{
			write_byte(create_mouse_device_addr + 0x5B, 0x06);
			shared::common::log("Game",
				"P_DontStealMouse: patched DirectInput to DISCL_NONEXCLUSIVE | DISCL_FOREGROUND",
				shared::common::LOG_TYPE::LOG_TYPE_GREEN, true);
		}
		else
		{
			shared::common::log("Game",
				"P_DontStealMouse: CreateMouseDevice address not found – skipping patch",
				shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
		}

		
		// ----------------------------------------------------------------
		// NoCull – disable frustum/portal/PVS culling
		// ----------------------------------------------------------------

		// debug_no_frustum_clip
		shared::utils::hook::set(reinterpret_cast<void*>(0x0050CB75), (BYTE)0xB0, (BYTE)0x01, (BYTE)0x90, (BYTE)0x90, (BYTE)0x90);
		shared::common::log("Game", "NoCull: debug_no_frustum_clip applied",
			shared::common::LOG_TYPE::LOG_TYPE_GREEN, true);

		// AABB frustum test
		shared::utils::hook::set(reinterpret_cast<void*>(0x0050D5B0), (BYTE)0x31, (BYTE)0xC0, (BYTE)0xB0, (BYTE)0x02, (BYTE)0xC3, (BYTE)0x90);
		shared::common::log("Game", "NoCull: AABB frustum test applied",
			shared::common::LOG_TYPE::LOG_TYPE_GREEN, true);

		// Point frustum test
		shared::utils::hook::set(reinterpret_cast<void*>(0x0050D4C0), (BYTE)0x31, (BYTE)0xC0, (BYTE)0xC3);
		shared::common::log("Game", "NoCull: Point frustum test applied",
			shared::common::LOG_TYPE::LOG_TYPE_GREEN, true);

		// Portal visibility test
		shared::utils::hook::set(reinterpret_cast<void*>(0x005549C0), (BYTE)0xB8, (BYTE)0x02, (BYTE)0x00, (BYTE)0x00, (BYTE)0x00, (BYTE)0xC3);
		shared::common::log("Game", "NoCull: Portal visibility test applied",
			shared::common::LOG_TYPE::LOG_TYPE_GREEN, true);

		// PVS gate in portal walk
		shared::utils::hook::set(reinterpret_cast<void*>(0x00554758), (BYTE)0x90, (BYTE)0x90, (BYTE)0x90, (BYTE)0x90, (BYTE)0x90, (BYTE)0x90);	
		shared::common::log("Game", "NoCull: PVS gate in portal walk applied",
			shared::common::LOG_TYPE::LOG_TYPE_GREEN, true);

		// Bounding sphere frustum test
		shared::utils::hook::set(reinterpret_cast<void*>(0x0050D890), (BYTE)0x31, (BYTE)0xC0, (BYTE)0xB0, (BYTE)0x02, (BYTE)0xC3, (BYTE)0x90);
		shared::common::log("Game", "NoCull: Bounding sphere frustum test applied",
			shared::common::LOG_TYPE::LOG_TYPE_GREEN, true);

		// DFS backtrack fix
		shared::utils::hook::set(reinterpret_cast<void*>(0x0055483C), (BYTE)0x90, (BYTE)0x90, (BYTE)0x90);
		shared::common::log("Game", "NoCull: DFS backtrack fix applied",
			shared::common::LOG_TYPE::LOG_TYPE_GREEN, true);


		// ----------------------------------------------------------------
		// Resolution – patch Halo's internal resolution to native monitor
		// dimensions so the D3D viewport matches the backbuffer.
		// Three sites mirror Chimera's set_up_video_mode() strategy.
		// All addresses are v1.0.10 halo.exe (preferred base 0x400000, no ASLR).
		// ----------------------------------------------------------------
		{
			const UINT w = static_cast<UINT>(GetSystemMetrics(SM_CXSCREEN));
			const UINT h = static_cast<UINT>(GetSystemMetrics(SM_CYSCREEN));

			// default_resolution_pc_sig (0x5169F1): C7 44 24 20 [W] C7 44 24 24 [H] C7 44 24 28 [Hz]
			// Overwrites the 800-wide DWORD at +4 and the 600-tall DWORD at +12.
			shared::utils::hook::set<DWORD>(reinterpret_cast<void*>(0x005169F5), w);  // +4
			shared::utils::hook::set<DWORD>(reinterpret_cast<void*>(0x005169FD), h);  // +12
			shared::common::log("Game",
				std::format("Resolution: default slot patched to {}x{}", w, h),
				shared::common::LOG_TYPE::LOG_TYPE_GREEN, true);

			// fallback_resolution_sig (0x51701A): NOP 16 bytes — removes the 800x600 fallback path.
			static const BYTE nop16[16] = {
				0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,
				0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90
			};
			shared::utils::hook::set(reinterpret_cast<void*>(0x0051701A), nop16, sizeof(nop16));
			shared::common::log("Game", "Resolution: fallback 800x600 NOP'd",
				shared::common::LOG_TYPE::LOG_TYPE_GREEN, true);

			// load_profile_resolution_sig (0x4956BE): 74 (jz) → EB (jmp)
			// Forces unconditional skip so the saved profile can't restore 640x480.
			shared::utils::hook::set(reinterpret_cast<void*>(0x004956BE), (BYTE)0xEB);
			shared::common::log("Game", "Resolution: profile-load resolution skip patched",
				shared::common::LOG_TYPE::LOG_TYPE_GREEN, true);
		}

	}

}

namespace comp::game::Helpers {

	CameraRenderMatrices* GetActiveCameraMatrices() {
		return CameraRenderMatrices_ptr;
	}

}
