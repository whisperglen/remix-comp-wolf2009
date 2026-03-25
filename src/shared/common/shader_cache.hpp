#pragma once
#include <string>
#include <vector>

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

#if DEBUG
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

#if DEBUG
			// log shaders
			static std::set<uint32_t> seen_hashes;
			if (seen_hashes.insert(hash).second) {
				shared::common::log("shader", std::format("New shader hash : {:08X}", hash), shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, true);
			}
#endif

			return hash;
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
						char fname[64];
						snprintf(fname, sizeof(fname), "shaders\\vs_%x.txt", hash);
						FILE* f;
						if ((f = fopen(fname, "r"))) {
							fclose(f);
						}
						else
						{
							f = fopen(fname, "wb");
							if (f)
							{
								int retry = 3;
								while (1 != fwrite(decompbuf->GetBufferPointer(), decompbuf->GetBufferSize(), 1, f))
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
					if (decomp)
					{
						decomp->assign((const char*)decompbuf->GetBufferPointer(), decompbuf->GetBufferSize());
					}
					decompbuf->Release();
				}
				else
				{
					shared::common::log("shader", std::format("Shader {:X} failed to decompile\n", hash), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
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
						char fname[64];
						snprintf(fname, sizeof(fname), "shaders\\ps_%x.txt", hash);
						FILE* f;
						if ((f = fopen(fname, "r"))) {
							fclose(f);
						}
						else
						{
							f = fopen(fname, "wb");
							if (f)
							{
								int retry = 3;
								while (1 != fwrite(decompbuf->GetBufferPointer(), decompbuf->GetBufferSize(), 1, f))
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
					if (decomp)
					{
						decomp->assign((const char*)decompbuf->GetBufferPointer(), decompbuf->GetBufferSize());
					}
					decompbuf->Release();
				}
				else
				{
					shared::common::log("shader", std::format("Shader {:X} failed to decompile\n", hash), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
				}
			}

			return hash;
		}

		enum EShaderType
		{
			SHADER_UNKNOWN,
			SHADER_GEO,
			SHADER_MODEL,
			SHADER_SKINNING,
			SHADER_IGNORE,
			SHADER_LIGHT,
			SHADER_UI
		};

		// check if shader is whitelisted
		EShaderType is_shader_whitelisted(IDirect3DVertexShader9* shader)
		{
			EShaderType type = SHADER_UNKNOWN;

			const uint32_t hash = get_shader_hash(shader);
			if (hash)
			{
				if (shader_whitelist_.contains(hash))
					type = shader_whitelist_[hash];

			}
			return type;
		}

		EShaderType is_shader_whitelisted(IDirect3DPixelShader9* shader)
		{
			EShaderType type = SHADER_UNKNOWN;

			const uint32_t hash = get_shader_hash(shader);
			if (hash)
			{
				if (shader_whitelist_.contains(hash))
					type = shader_whitelist_[hash];

			}
			return type;
		}

		// clear cache
		void clear_cache() {
			shader_hash_cache_.clear();
		}

		// add hash to whitelist
		void add_to_whitelist(uint32_t hash, EShaderType type) {
			shader_whitelist_[hash] = type;
		}

	private:
		std::unordered_map<void*, uint32_t> shader_hash_cache_;
		std::unordered_map<uint32_t, EShaderType> shader_whitelist_ = {};
	};

	// global instance
	inline ShaderCache g_shader_cache;
}
