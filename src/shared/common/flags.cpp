#include "std_include.hpp"
#include "flags.hpp"
#include "mINI\ini.h"
#include <tchar.h>

namespace shared::common
{
	std::vector<std::string> flags::m_enabled_flags;
	mINI::INIStructure iniconf;

	// gets the singleton instance
	flags& flags::get()
	{
		static flags instance;
		return instance;
	}

	bool flags::has_flag(const std::string& flag)
	{
		get().parse_flags();
		for (const auto& entry : m_enabled_flags)
		{
			if (utils::str_to_lower(entry) == utils::str_to_lower(flag)) {
				return true;
			}
		}

		if (iniconf.has("Flags"))
		{
			if (iniconf["Flags"].has(flag) && iniconf["Flags"].get(flag).compare("0") != 0) {
				return true;
			}
		}

		return false;
	}

	int flags::get_config(const std::string& flag, int defval)
	{
		int ret = defval;
		if (iniconf.has("Config"))
		{
			if (iniconf["Config"].has(flag))
			{
				ret = strtol(iniconf["Config"].get(flag).c_str(), nullptr, 10);
			}
		}

		return ret;
	}

	bool flags::is_shader_ignored(uint32_t hash)
	{
		get().parse_flags();

		if (iniconf.has("Shader_Ignore"))
		{
			char hexstr[9];
			snprintf(hexstr, sizeof(hexstr), "%08x", hash);

			if (iniconf["Shader_Ignore"].has("vertex")) {
				std::string vs = iniconf["Shader_Ignore"].get("vertex");
				if(vs.contains(hexstr))
					return true;
			}
			if (iniconf["Shader_Ignore"].has("pixel")) {
				std::string vs = iniconf["Shader_Ignore"].get("pixel");
				if (vs.contains(hexstr))
					return true;
			}
		}

		return false;
	}

	void flags::parse_flags()
	{
		// only parse flags once
		if (static auto flags_parsed = false; !flags_parsed)
		{
			flags_parsed = true;

			int num_args;
			auto* const argv = CommandLineToArgvW(GetCommandLineW(), &num_args);

			if (argv)
			{
				for (auto i = 0; i < num_args; ++i)
				{
					std::wstring wide_flag(argv[i]);
					if (wide_flag[0] == L'-')
					{
						wide_flag.erase(wide_flag.begin());
						auto str = utils::convert_wstring(wide_flag);
						shared::common::log("Flags", std::format("Found flag: {}", str), shared::common::LOG_TYPE::LOG_TYPE_STATUS, true);
						m_enabled_flags.emplace_back(std::move(str));
					}
				}

				LocalFree(argv);
			}

			const int PATH_SZ = 1024;
			char exename[PATH_SZ] = { 0 };
			std::string game_cfg;
			DWORD ercd = GetModuleFileNameA(NULL, exename, PATH_SZ);
			if (ercd > 0)
			{
				char* name = strrchr(exename, '\\');
				if (name) {
					name++;
					size_t count = strlen(name);
					char* tmp = strrchr(exename, '.');
					if (tmp) {
						count = tmp - name;
					}
					game_cfg.append(name, count);
				}
			}
			else
			{
				game_cfg.assign("game");
			}
			game_cfg.append(".ini");

			mINI::INIFile inifile(game_cfg);
			inifile.read(iniconf);
		}
	}
}
