#include "std_include.hpp"
#include "shared/common/flags.hpp"
#include "comp\modules\renderer.hpp"

#include <Windows.h>
#include <Psapi.h>

namespace comp::game
{
	// --------------
	// game variables

	using comp::gstate;

	//DWORD* d3d_dev_sample_addr = nullptr;

	// --------------
	// game functions

	// SampleTemplate_t SampleTemplate = nullptr;
	void(*original_RB_DrawView)(const void* data) = nullptr;


	// --------------
	// game asm offsets

	//uint32_t retn_addr__func1 = 0u;
	//uint32_t nop_addr__func2 = 0u;
	//uint32_t retn_addr__pre_draw_something = 0u;
	//uint32_t hk_addr__post_draw_something = 0u;

	static byte g_pattern_exec_be_cmds[] = { 0x83, 0xec, 0x10, 0x53, 0x56, 0x8b, 0x74, 0x24, 0x1c, 0x83, 0x3e, 0x00 };
	static byte g_pattern_call_drawview[] = { 0x56, 0x32, 0xdb };
	// --------------

#define PATTERN_OFFSET_SIMPLE(var, pattern, byte_offset, static_addr) \
		if (const auto offset = shared::utils::mem::find_pattern(##pattern, byte_offset, #var, use_pattern, static_addr); offset) { \
			(var) = offset; found_pattern_count++; \
		} total_pattern_count++;

#define PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(var, type, pattern, byte_offset, static_addr) \
		if (const auto offset = shared::utils::mem::find_pattern(##pattern, byte_offset, #var, use_pattern, static_addr); offset) { \
			(var) = (type)*(DWORD*)offset; found_pattern_count++; \
		} total_pattern_count++;

	static void hook_do_init();
	static void hook_RB_DrawView(const void* data);
	const void* hook_find_pattern(const void* start, const unsigned char* pat, int patsz);
	int hook_unprotect(void* ptr, int size, unsigned long* restore);
	int hook_protect(void* ptr, int size, unsigned long restore);

	// init any adresses here
	void init_game_addresses()
	{
		const bool use_pattern = !shared::common::flags::has_flag("no_pattern");
		if (use_pattern) {
			shared::common::log("Game", "Getting offsets ...", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
		}

		std::uint32_t total_pattern_count = 0u;
		std::uint32_t found_pattern_count = 0u;

		renderer::prepare_structs();


#pragma region GAME_VARIABLES

		// Find code that references the global var you are interested in, grab the address of the instruction + pattern
		// Figure out the byte offset that's needed until your global var address starts in the instruction 
		// -> 'mov eax, d3d_dev_sample_addr' == A1 D8 D8 7E 01 where A1 is the mov instruction and the following 4 bytes the addr of the global var -> so offset 1

		// Patterns are quite slow on DEBUG builds. The last argument in find_pattern allows you to declare a static offset which will be used
		// when the game gets started with `-no_pattern` in the commandline

		// ----

		// Example verbose
			//if (const auto offset = shared::utils::mem::find_pattern("? ? ? ? ?", 1, "d3d_dev_sample_addr", use_pattern, 0xDEADBEEF); offset) {
			//	d3d_dev_sample_addr = (DWORD*)*(DWORD*)offset; found_pattern_count++; // cast mem at offset
			//} total_pattern_count++;

		// Or via macro
			//PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(d3d_dev_sample_addr, DWORD*, "? ? ? ? ?", 1, 0xDEADBEEF);


		// Another example with a structure object
			//PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(vp, state_s*, "? ? ? ? ?", 0, 0xDEADBEEF);

		// end GAME_VARIABLES
#pragma endregion

		// ---


#pragma region GAME_FUNCTIONS

		// cast func template
		//PATTERN_OFFSET_DWORD_PTR_CAST_TYPE(SampleTemplate, SampleTemplate_t, "? ? ? ? ?", 0, 0xDEADBEEF);

		hook_do_init();

		//let's find RB_ExecuteBackEndCommands then RB_DrawView which is called from the former
		//I'm doing this because RB_DrawView is only calling into a C++ object,
		// and I don't think it cam be found by pattern (too few bytes)
		const void* ptr = hook_find_pattern(NULL, g_pattern_exec_be_cmds, sizeof(g_pattern_exec_be_cmds));
		if (!ptr)
		{
			//quit
			return;
		}
		ptr = hook_find_pattern(ptr, g_pattern_call_drawview, sizeof(g_pattern_call_drawview));
		if (!ptr)
		{
			return;
		}

		byte* code = (byte*)ptr + sizeof(g_pattern_call_drawview);
		//right, we're now at the CALL RB_DrawView location
		//e8 e9 bf 00 00 <- I'm seeing this in the exe, but I can only count on e8, rest of bytes can vary
		if (code[0] == 0xe8)
		{
			//first store the address of original RB_DrawView
			intptr_t calladdr = 0;
			memcpy(&calladdr, &code[1], sizeof(calladdr));
			original_RB_DrawView = (void(*)(const void*))(calladdr + (intptr_t)&code[5]);

			//now lets put-in our hook RB_DrawView
			calladdr = (intptr_t)hook_RB_DrawView - (intptr_t)&code[5];
			unsigned long restore;
			if (hook_unprotect(code, 5, &restore))
			{
				memcpy(&code[1], &calladdr, sizeof(calladdr));
				hook_protect(code, 5, restore);

				shared::common::log("Game", std::format("RB_DrawView was hooked"), shared::common::LOG_TYPE::LOG_TYPE_GREEN, true);;
				found_pattern_count++;
			}
		}
		total_pattern_count++;

		//if (const auto offset = shared::utils::mem::find_pattern("? ? ? ? ?", 1, "d3d_dev_sample_addr", use_pattern, 0xDEADBEEF); offset) {
		//	d3d_dev_sample_addr = (DWORD*)*(DWORD*)offset; found_pattern_count++; // cast mem at offset
		//} total_pattern_count++;

		// end GAME_FUNCTIONS
#pragma endregion

		// ---


#pragma region GAME_ASM_OFFSETS

		// Assembly offsets are simple offsets that do not require additional casting

		// Example verbose
			//if (const auto offset = shared::utils::mem::find_pattern(" ? ? ? ", 0, "nop_addr__func2", use_pattern, 0xDEADBEEF); offset) {
			//	nop_addr__func2 = offset; found_pattern_count++;
			//} total_pattern_count++;

		// Or via macro
			//PATTERN_OFFSET_SIMPLE(retn_addr__pre_draw_something, "? ? ? ?", 0, 0xDEADBEEF);
			//PATTERN_OFFSET_SIMPLE(hk_addr__post_draw_something, "? ? ? ?", 0, 0xDEADBEEF); 

		// end GAME_ASM_OFFSETS
#pragma endregion


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

	static void hook_RB_DrawView(const void* data)
	{
		const int* viewDef = (const int*)((const byte*)data + 8);

		const float* pm = (const float*)(*viewDef + 112);
		//const float* mm = (const float*)(*viewDef + 304);
		const float* mvm = (const float*)(*viewDef + 368);

		if (0 != memcmp(gstate.proj.m, pm, sizeof(gstate.proj.m)))
		{
			memcpy(gstate.proj.m, pm, sizeof(gstate.proj.m));
			gstate.proj_inv_dirty = 1;
		}
		if (0 != memcmp(gstate.view.m, mvm, sizeof(gstate.view.m)))
		{
			memcpy(gstate.view.m, mvm, sizeof(gstate.view.m));
			gstate.view_inv_dirty = 1;
		}

#if 0
		log_printf("\n\nproj %f %f %f %f\n", pm[0], pm[1], pm[2], pm[3]);
		log_printf("proj %f %f %f %f\n", pm[4], pm[5], pm[6], pm[7]);
		log_printf("proj %f %f %f %f\n", pm[8], pm[9], pm[10], pm[11]);
		log_printf("proj %f %f %f %f\n", pm[12], pm[13], pm[14], pm[15]);

		log_printf("\nmod %f %f %f %f\n", mm[0], mm[1], mm[2], mm[3]);
		log_printf("mod %f %f %f %f\n", mm[4], mm[5], mm[6], mm[7]);
		log_printf("mod %f %f %f %f\n", mm[8], mm[9], mm[10], mm[11]);
		log_printf("mod %f %f %f %f\n", mm[12], mm[13], mm[14], mm[15]);

		log_printf("\nmvm %f %f %f %f\n", mvm[0], mvm[1], mvm[2], mvm[3]);
		log_printf("mvm %f %f %f %f\n", mvm[4], mvm[5], mvm[6], mvm[7]);
		log_printf("mvm %f %f %f %f\n", mvm[8], mvm[9], mvm[10], mvm[11]);
		log_printf("mvm %f %f %f %f\n", mvm[12], mvm[13], mvm[14], mvm[15]);
		//log_floats("pmat", projectionMatrix, 16);
		//log_floats("mmat", modelMatrix, 16);
		//log_floats("mvmat", modelViewMatrix, 16);
#endif

		//call original function
		original_RB_DrawView(data);
	}

	static MODULEINFO exedata = { 0 };

	int hook_unprotect(void* ptr, int size, unsigned long* restore)
	{
		DWORD error;
		DWORD dwOld = 0;
		if (!VirtualProtect(ptr, size, PAGE_EXECUTE_READWRITE, &dwOld)) {
			error = GetLastError();
			//log_printf("VirtualProtect failed RW for %p with 0x%x\n", ptr, error);
			shared::common::log("Game", std::format("VirtualProtect failed RW for {:P} with {:X}", ptr, error), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			return FALSE;
		}
		if (restore)
			*restore = dwOld;

		return TRUE;
	}

	int hook_protect(void* ptr, int size, unsigned long restore)
	{
		DWORD error;
		DWORD dwOld = 0;
		if (!VirtualProtect(ptr, size, restore, &dwOld)) {
			error = GetLastError();
			//log_printf("VirtualProtect failed op:%x for %p with 0x%x\n", restore, ptr, error);
			shared::common::log("Game", std::format("VirtualProtect failed op:{:X} for {:P} with {:X}", restore, ptr, error), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			return FALSE;
		}
		return TRUE;
	}

	static void hook_do_init()
	{
		const char* binname = nullptr;

		ZeroMemory(&exedata, sizeof(exedata));
		if (GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(nullptr), &exedata, sizeof(exedata)))
		{
			shared::common::log("Game", std::format("moduleinfo({:s}) base:{:p} size:{:d} ep:{:p}",
				(binname ? binname : "exe"), exedata.lpBaseOfDll, exedata.SizeOfImage, exedata.EntryPoint), shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, true);
		}
		else
		{
			shared::common::log("Game", std::format("Cannot get executable info {:s} ({:d})\n",
				(binname ? binname : "exe"), GetLastError()), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
		}
	}

	const void* hook_find_pattern(const void* start, const unsigned char* pat, int patsz)
	{
		const void* ret = 0;
		const void* haystack = start ? start : exedata.lpBaseOfDll;
		int haystacksz = exedata.SizeOfImage;
		const byte* ptr;

		ptr = (const byte*)memchr(haystack, pat[0], haystacksz);
		while (ptr)
		{
			haystacksz -= (ptrdiff_t)ptr - (ptrdiff_t)haystack;
			if (haystacksz < patsz)
			{
				//nothing found
				break;
			}

			int i;
			for (i = 1; i < patsz; i++)
			{
				if (ptr[i] != pat[i])
				{
					break;
				}
			}
			if (i == patsz)
			{
				ret = ptr;
				break;
			}
			haystack = (void*)((ptrdiff_t)ptr + i);
			haystacksz -= i;
			ptr = (const byte*)memchr(haystack, pat[0], haystacksz);
		}

		return ret;
	}

	void* hook_loadptr(const void* addr)
	{
		void* ret = 0;
		memcpy(&ret, addr, sizeof(ret));
		return ret;
	}

	void* hook_offset_to_addr(void* offset)
	{
		void* ret = 0;
		if ((intptr_t)exedata.SizeOfImage > (intptr_t)offset)
		{
			ret = (uint8_t*)exedata.lpBaseOfDll + (intptr_t)offset;
		}

		return ret;
	}
}