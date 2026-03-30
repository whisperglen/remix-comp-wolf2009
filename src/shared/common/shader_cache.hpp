#pragma once
#include <string>
#include <vector>
#include <direct.h>

namespace shared::common
{
	// shader cache and whitelist manager
	class ShaderCache
	{
	public:
		ShaderCache() = default;

		// get or compute shader hash
		uint32_t get_shader_hash(IDirect3DVertexShader9* shader)
		{
			if (!shader) {
				return 0;
			}

			// check cache
			if (const auto it = shader_hash_cache_.find(shader); it != shader_hash_cache_.end()) {
				return it->second;
			}

			// get bytecode
			UINT bytecode_size = 0;
			shader->GetFunction(nullptr, &bytecode_size);

			std::vector<BYTE> bytecode(bytecode_size);
			shader->GetFunction(bytecode.data(), &bytecode_size);

			// compute and cache hash
			const uint32_t hash = shared::utils::data_hash32(bytecode.data(), bytecode_size);
			shader_hash_cache_[shader] = hash;

#if DEBUG && 0
			// log shaders
			static std::set<uint32_t> seen_hashes;
			if (seen_hashes.insert(hash).second) {
				shared::common::log("shader", std::format("New shader hash : {:08X}", hash), shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, true);
			}
#endif

			return hash;
		}

		uint32_t get_shader_hash(IDirect3DPixelShader9* shader)
		{
			if (!shader) {
				return 0;
			}

			// check cache
			if (const auto it = shader_hash_cache_.find(shader); it != shader_hash_cache_.end()) {
				return it->second;
			}

			// get bytecode
			UINT bytecode_size = 0;
			shader->GetFunction(nullptr, &bytecode_size);

			std::vector<BYTE> bytecode(bytecode_size);
			shader->GetFunction(bytecode.data(), &bytecode_size);

			// compute and cache hash
			const uint32_t hash = shared::utils::data_hash32(bytecode.data(), bytecode_size);
			shader_hash_cache_[shader] = hash;

#if DEBUG && 0
			// log shaders
			static std::set<uint32_t> seen_hashes;
			if (seen_hashes.insert(hash).second) {
				shared::common::log("shader", std::format("New shader hash : {:08X}", hash), shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, true);
			}
#endif

			return hash;
		}

		void write_shader_to_file(const char *prefix, uint32_t hash, const void *data, size_t sz)
		{
			char fname[64];
			snprintf(fname, sizeof(fname), "shaders\\%s_%x.txt", prefix, hash);
			FILE* f;
			if ((f = fopen(fname, "r"))) {
				fclose(f);
			}
			else
			{
				f = fopen(fname, "wb");
				if (!f)
				{
					_CRT_UNUSED(_mkdir("shaders"));
					f = fopen(fname, "wb");
				}

				if (f)
				{
					int retry = 3;
					while (1 != fwrite(data, sz, 1, f))
					{
						retry--;
						if (retry <= 0) break;
					}
					fclose(f);
				}
				else
					shared::common::log("shader", std::format("Cannot open file for dumps {:s}", fname), shared::common::LOG_TYPE::LOG_TYPE_WARN, true);
			}
		}

		uint32_t get_shader_decomp(IDirect3DVertexShader9* shader, std::string* decomp, bool data_dump)
		{
			if (!shader) {
				return 0;
			}

			uint32_t hash = 0;

			// check cache
			if (const auto it = shader_hash_cache_.find(shader); it != shader_hash_cache_.end()) {
				hash = it->second;
			}
			
			// get bytecode
			UINT bytecode_size = 0;
			shader->GetFunction(nullptr, &bytecode_size);

			std::vector<BYTE> bytecode(bytecode_size);
			shader->GetFunction(bytecode.data(), &bytecode_size);

			if(hash == 0)
			{
				// compute and cache hash
				hash = shared::utils::data_hash32(bytecode.data(), bytecode_size);
				shader_hash_cache_[shader] = hash;
			}

			if (decomp || data_dump)
			{
				HRESULT res;
				LPD3DXBUFFER decompbuf;

				if (SUCCEEDED((res = D3DXDisassembleShader((DWORD*)bytecode.data(), FALSE, NULL, &decompbuf))))
				{
					if (data_dump)
					{
						write_shader_to_file("vs", hash, decompbuf->GetBufferPointer(), decompbuf->GetBufferSize());
					}
					if (decomp)
					{
						decomp->assign((const char*)decompbuf->GetBufferPointer(), decompbuf->GetBufferSize());
					}
					decompbuf->Release();
				}
				else
				{
					shared::common::log("shader", std::format("Shader {:08X} failed to decompile\n", hash), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
				}
			}

			return hash;
		}

		uint32_t get_shader_decomp(IDirect3DPixelShader9* shader, std::string* decomp, bool data_dump)
		{
			if (!shader) {
				return 0;
			}

			uint32_t hash = 0;

			// check cache
			if (const auto it = shader_hash_cache_.find(shader); it != shader_hash_cache_.end()) {
				hash = it->second;
			}

			// get bytecode
			UINT bytecode_size = 0;
			shader->GetFunction(nullptr, &bytecode_size);

			std::vector<BYTE> bytecode(bytecode_size);
			shader->GetFunction(bytecode.data(), &bytecode_size);

			if (hash == 0)
			{
				// compute and cache hash
				hash = shared::utils::data_hash32(bytecode.data(), bytecode_size);
				shader_hash_cache_[shader] = hash;
			}

			if (decomp || data_dump)
			{
				HRESULT res;
				LPD3DXBUFFER decompbuf;

				if (SUCCEEDED((res = D3DXDisassembleShader((DWORD*)bytecode.data(), FALSE, NULL, &decompbuf))))
				{
					if (data_dump)
					{
						write_shader_to_file("ps", hash, decompbuf->GetBufferPointer(), decompbuf->GetBufferSize());
					}
					if (decomp)
					{
						decomp->assign((const char*)decompbuf->GetBufferPointer(), decompbuf->GetBufferSize());
					}
					decompbuf->Release();
				}
				else
				{
					shared::common::log("shader", std::format("Shader {:08X} failed to decompile\n", hash), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
				}
			}

			return hash;
		}

		enum EShaderType
		{
			SHADER_NEW,
			SHADER_UNKNOWN,
			SHADER_GEO,
			SHADER_MODEL,
			SHADER_SKINNING,
			SHADER_SKINNING_2,
			SHADER_IGNORE,
			SHADER_LIGHT,
			SHADER_UI,
			SHADER_UI_FF,
			SHADER_SKY
		};

		struct SShaderClasify
		{
			uint8_t type;
			uint8_t albedoStage;
			SShaderClasify() : type(SHADER_NEW), albedoStage(0) {}
		};

		const char* get_shader_type_str(EShaderType type)
		{
			switch (type)
			{
			case SHADER_NEW:
				return "NEW";
			case SHADER_UNKNOWN:
				return "UNKNOWN";
			case SHADER_GEO:
				return "GEO";
			case SHADER_MODEL:
				return "MODEL";
			case SHADER_SKINNING:
				return "SKINNING";
			case SHADER_SKINNING_2:
				return "SKINNING_2";
			case SHADER_IGNORE:
				return "IGNORE";
			case SHADER_LIGHT:
				return "LIGHT";
			case SHADER_UI:
				return "UI";
			case SHADER_UI_FF:
				return "UI_FF";
			case SHADER_SKY:
				return "SKY";
			}
			return "__FAIL__";
		}

		// check if shader is whitelisted
		bool is_shader_info_cached(IDirect3DVertexShader9* shader, SShaderClasify& info)
		{
			const uint32_t hash = get_shader_hash(shader);
			if (hash)
			{
				if (shader_storage_.contains(hash))
				{
					info = shader_storage_[hash];
					return true;
				}

			}
			return false;
		}

		bool is_shader_info_cached(IDirect3DPixelShader9* shader, SShaderClasify& info)
		{
			const uint32_t hash = get_shader_hash(shader);
			if (hash)
			{
				if (shader_storage_.contains(hash))
				{
					info = shader_storage_[hash];
					return true;
				}

			}
			return false;
		}

		// clear cache
		void clear_cache() {
			shader_hash_cache_.clear();
		}

		// add hash to whitelist
		void add_to_cache(uint32_t hash, SShaderClasify& info) {
			shader_storage_[hash] = info;
			shared::common::log("shader", std::format("Shader {:08X} marked as {:s}", hash, get_shader_type_str(EShaderType(info.type))), shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, true);
		}

	private:
		std::unordered_map<void*, uint32_t> shader_hash_cache_;
		std::unordered_map<uint32_t, SShaderClasify> shader_storage_ = {};
	};

	// global instance
	inline ShaderCache g_shader_cache;
}
