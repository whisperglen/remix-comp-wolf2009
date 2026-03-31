#include "std_include.hpp"
#include "renderer.hpp"
#include "game/structs.hpp"
#include "shared/common/shader_cache.hpp"
#include "shared/common/flags.hpp"

#include "imgui.hpp"

namespace comp
{
	bool g_rendered_first_primitive = false;
	int g_is_rendering_something = 0;

	comp::game::state_s gstate;
	shared::common::ShaderCache shaders;

	namespace tex_addons
	{
		bool initialized = false;
		LPDIRECT3DTEXTURE9 berry = nullptr;

		void init_texture_addons(bool release)
		{
			if (release)
			{
				if (tex_addons::berry) tex_addons::berry->Release();
				return;
			}

			shared::common::log("Renderer", "Loading CompMod Textures ...", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);

			auto load_texture = [](IDirect3DDevice9* dev, const char* path, LPDIRECT3DTEXTURE9* tex)
				{
					HRESULT hr;
					hr = D3DXCreateTextureFromFileA(dev, path, tex);
					if (FAILED(hr)) shared::common::log("Renderer", std::format("Failed to load {}", path), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
				};

			const auto dev = shared::globals::d3d_device;
			load_texture(dev, "rtx_comp\\textures\\berry.png", &tex_addons::berry);
			tex_addons::initialized = true;
		}
	}


	// ----

	drawcall_mod_context& setup_context(IDirect3DDevice9* dev)
	{
		auto& ctx = renderer::dc_ctx;
		ctx.info.device_ptr = dev;

		// any additional info about the current drawcall here

		return ctx;
	}

	void renderer::process_gpu_skinning(IDirect3DDevice9* dev, drawcall_mod_context& ctx, const INT BaseVertexIndex, const UINT MinVertexIndex, const UINT NumVertices)
	{
		handle_mats_inversion();

		dev->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, 1);
		dev->SetRenderState(D3DRS_VERTEXBLEND, gstate.skinWeights);


		dev->SetRenderState(D3DRS_ALPHATESTENABLE, 0);

		//modify blend skinWeights, since the first weight is signed
		if(gstate.skinWeights_offset >= 0)
		{
			IDirect3DVertexBuffer9* pStreamData = nullptr;
			UINT iOffsetInBytes = 0;
			UINT iStride = 0;

			dev->GetStreamSource(gstate.skinWeight_streamNum, &pStreamData, &iOffsetInBytes, &iStride);

			if (pStreamData != nullptr) {
				// 1. Calculate the absolute lowest index accessed
				UINT AbsoluteMinIndex = BaseVertexIndex + MinVertexIndex;

				// 2. Calculate the exact byte offset in the Vertex Buffer
				UINT StartByteOffset = iOffsetInBytes + (AbsoluteMinIndex * iStride);

				// 3. Calculate the total size of the vertex block being used
				UINT BlockSizeToLock = NumVertices * iStride;

				// 4. Lock the buffer to read/modify the vertices
				void* pVertices = nullptr;
				if (SUCCEEDED(pStreamData->Lock(StartByteOffset, BlockSizeToLock, &pVertices, 0))) {

					BYTE* weights = (BYTE*)pVertices + gstate.skinWeights_offset;

					if (gstate.skinWeight_replace0w == false)
					{
						// pVertices now points to the very first vertex used in this DIP call
						for (uint32_t i = 0; i < NumVertices; i++, weights += iStride)
						{
							*((UINT32*)weights) &= 0x7fffffff;//signbit to 0
						}
					}
					else
					{
						// need to put in skinWeights 1.0, 0.0 .. 0.0
						for (uint32_t i = 0; i < NumVertices; i++, weights += iStride)
						{
							float* w = (float*)weights;
							switch (gstate.skinWeights)
							{
							case D3DVBF_3WEIGHTS:
								w[2] = 0.0f;
							case D3DVBF_2WEIGHTS:
								w[1] = 0.0f;
							case D3DVBF_1WEIGHTS:
								w[0] = 1.0f;
							}
						}
					}

					pStreamData->Unlock();
				}

				pStreamData->Release();
			}
		}

		const int NUM_BONES = 75;
		const int BONE_MAT_SZ = 4 * 3;
		D3DXMATRIX bone, bone_shader, *bonemul, local;
		bone_shader._14 = 0;
		bone_shader._24 = 0;
		bone_shader._34 = 0;
		bone_shader._44 = 1;
		float* src = &gstate.vs_constants[0][0];
		float* mat = &bone_shader.m[0][0];

		if (ctx.modifiers.do_gpu_skinning == 1)
		{
			//bones are skin to modelview transforms, so we need inv camera to make them skin to world
			bonemul = &gstate.view_inv;
		}
		else
		{
			//bone are skin to model transforms, so we need model matrix to convert them to skin to world
			float *rmv = &gstate.vs_constants[229][0];
			mat[0] = rmv[0]; mat[1] = rmv[4]; mat[2] = rmv[8];
			mat[4] = rmv[1]; mat[5] = rmv[5]; mat[6] = rmv[9];
			mat[8] = rmv[2]; mat[9] = rmv[6]; mat[10] = rmv[10];
			mat[12] = rmv[3]; mat[13] = rmv[7]; mat[14] = rmv[11];

			D3DXMatrixMultiply(&local, &bone_shader, &gstate.view_inv);
			bonemul = &local;

		}

		for (int i = 0; i < NUM_BONES; i++, src += BONE_MAT_SZ)
		{
			mat[0] = src[0]; mat[1] = src[4]; mat[2] = src[8];
			mat[4] = src[1]; mat[5] = src[5]; mat[6] = src[9];
			mat[8] = src[2]; mat[9] = src[6]; mat[10] = src[10];
			mat[12] = src[3]; mat[13] = src[7]; mat[14] = src[11];

			D3DXMatrixMultiply(&bone, &bone_shader, bonemul);
			dev->SetTransform(D3DTS_WORLDMATRIX(i), &bone);
		}
	}

	void renderer::prepare_ff_texture_stages(IDirect3DDevice9* dev, drawcall_mod_context& ctx)
	{
		/* Alpha test: the game's pixel shader handled alpha discard; without a PS,
		 * FFP needs the render state so Remix classifies cutout materials correctly. */
		{
			dev->SetRenderState(D3DRS_ALPHATESTENABLE, 1);
			dev->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
			dev->SetRenderState(D3DRS_ALPHAREF, gstate.alphaRef);
		}

		/*
		 * Setup texture stages for FFP mode.
		 * Stage 0: modulate texture color with vertex/material diffuse.
		 * Stage 1+: disabled.
		 */
		dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
		dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		dev->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, gstate.albedoStage);
		dev->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, 0);

		if (gstate.albedoStage)
		{
			IDirect3DBaseTexture9* tex;
			dev->GetTexture(gstate.albedoStage, &tex);
			dev->SetTexture(0, tex);
			tex->Release();
		}

		/* Disable stages 1-7: the game binds shadow maps, LUTs, normal maps etc.
		 * on higher stages for its pixel shaders. In FFP mode those stages become
		 * active and Remix may consume the wrong textures. */
		for (int i = 1; i < 8; i++) {
			dev->SetTextureStageState(i, D3DTSS_COLOROP, D3DTOP_DISABLE);
			dev->SetTextureStageState(i, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
			dev->SetTexture(i, nullptr);
		}
	}

	void renderer::set_transforms_mvp(IDirect3DDevice9* dev, drawcall_mod_context& ctx)
	{
		handle_mats_inversion();

		float* c0 = &gstate.vs_constants[0][0];
		D3DXMATRIX mvpmat, mv, model;
		float* mvp = &mvpmat.m[0][0];

		mvp[0] = c0[0]; mvp[1] = c0[4]; mvp[2] = c0[8]; mvp[3] = c0[12];
		mvp[4] = c0[1]; mvp[5] = c0[5]; mvp[6] = c0[9]; mvp[7] = c0[13];
		mvp[8] = c0[2]; mvp[9] = c0[6]; mvp[10] = c0[10]; mvp[11] = c0[14];
		mvp[12] = c0[3]; mvp[13] = c0[7]; mvp[14] = c0[11]; mvp[15] = c0[15];

		D3DXMatrixMultiply(&mv, &mvpmat, &gstate.proj_inv);
		D3DXMatrixMultiply(&model, &mv, &gstate.view_inv);

		dev->SetTransform(D3DTS_PROJECTION, &gstate.proj);
		dev->SetTransform(D3DTS_VIEW, &gstate.view);
#if 1
		dev->SetTransform(D3DTS_WORLD, &model);
#else
		D3DXMATRIX identity;
		D3DXMatrixIdentity(&identity);
		dev->SetTransform(D3DTS_WORLD, &identity);
#endif
	}

	void renderer::set_transforms_mv(IDirect3DDevice9* dev, drawcall_mod_context& ctx)
	{
		handle_mats_inversion();

		float* c4 = &gstate.vs_constants[4][0];
		D3DXMATRIX mvmat, model;
		float* mv = &mvmat.m[0][0];

		mv[0] = c4[0]; mv[1] = c4[4]; mv[2] = c4[8]; mv[3] = 0;
		mv[4] = c4[1]; mv[5] = c4[5]; mv[6] = c4[9]; mv[7] = 0;
		mv[8] = c4[2]; mv[9] = c4[6]; mv[10] = c4[10]; mv[11] = 0;
		mv[12] = c4[3]; mv[13] = c4[7]; mv[14] = c4[11]; mv[15] = 1;

		D3DXMatrixMultiply(&model, &mvmat, &gstate.view_inv);

		dev->SetTransform(D3DTS_PROJECTION, &gstate.proj);
		dev->SetTransform(D3DTS_VIEW, &gstate.view);
#if 1
		dev->SetTransform(D3DTS_WORLD, &model);
#else
		D3DXMATRIX identity;
		D3DXMatrixIdentity(&identity);
		dev->SetTransform(D3DTS_WORLD, &identity);
#endif
	}

	bool renderer::prepare_drawcall(IDirect3DDevice9* dev, drawcall_mod_context& ctx)
	{
		using shared::common::ShaderCache;

		bool with_ff = false;

		if (gstate.drawcallLimit && gstate.drawcallLimit < gstate.drawcallCount)
		{
			ctx.modifiers.do_not_render = true;
		}
		else if (gstate.vs_type == ShaderCache::SHADER_IGNORE || gstate.ps_type == ShaderCache::SHADER_IGNORE)
		{
			ctx.modifiers.do_not_render = true;
		}
		else if (gstate.vs_type == ShaderCache::SHADER_SKINNING && gstate.declHasSkinning)
		{
			dev->SetTransform(D3DTS_PROJECTION, &gstate.proj);
			dev->SetTransform(D3DTS_VIEW, &gstate.view);

			ctx.modifiers.do_gpu_skinning = 1;
			with_ff = true;
		}
		else if (gstate.vs_type == ShaderCache::SHADER_SKINNING_2 && gstate.declHasSkinning)
		{
			dev->SetTransform(D3DTS_PROJECTION, &gstate.proj);
			dev->SetTransform(D3DTS_VIEW, &gstate.view);

			ctx.modifiers.do_gpu_skinning = 2;
			with_ff = true;
		}
		else if (gstate.vs_type == ShaderCache::SHADER_UI || gstate.ps_type == ShaderCache::SHADER_UI || gstate.proj._34 == 0.0f || gstate.ps_type == ShaderCache::SHADER_UI_FF)
		{
			D3DXMATRIX identity;
			D3DXMatrixIdentity(&identity);

			dev->SetTransform(D3DTS_PROJECTION, &gstate.proj);
			dev->SetTransform(D3DTS_VIEW, &gstate.view);
			dev->SetTransform(D3DTS_WORLD, &identity);

			if (gstate.ps_type == ShaderCache::SHADER_UI_FF)
			{
				with_ff = true;
			}
		}
		else if (gstate.dontDrawUntextured && !gstate.numTextures)
		{
			// skip untextured tris, do this after UI filtering
			ctx.modifiers.do_not_render = true;
		}
		else if (gstate.vs_type == ShaderCache::SHADER_LIGHT || gstate.ps_type == ShaderCache::SHADER_LIGHT)
		{
			ctx.modifiers.do_not_render = true;
		}
		else if (gstate.vs_type == ShaderCache::SHADER_SKY)
		{
			//gstate.skyHasRendered = true;

			set_transforms_mvp(dev, ctx);

			with_ff = true;
		}
		else if (gstate.skyHasRendered && gstate.vs_type != ShaderCache::SHADER_SKY)
		{
			//skip all geo draws after sky
			ctx.modifiers.do_not_render = true;
		}
		else if (gstate.vs_type == ShaderCache::SHADER_GEO)
		{
			set_transforms_mvp(dev, ctx);

			with_ff = true;
		}
		else if (gstate.vs_type == ShaderCache::SHADER_MODEL)
		{
			set_transforms_mv(dev, ctx);

			with_ff = true;
		}

		return with_ff;
	}


	// ----
	void renderer::on_vertex_declaration(IDirect3DDevice9* dev, IDirect3DVertexDeclaration9* &pDecl)
	{
		BOOL foundSkinning = FALSE;
		D3DVERTEXBLENDFLAGS numWeights = D3DVBF_DISABLE;
		int indices_offset = -1;
		int weights_offset = -1;
		int streamNum = 0;
		int numTextures = 0;
		int replace0w_idx = -1;
		bool replace0w = false;
		D3DVERTEXELEMENT9 decinfo[10];
		UINT num = 0;
		pDecl->GetDeclaration(nullptr, &num);
		if (num <= ARRAYSIZE(decinfo))
		{
			pDecl->GetDeclaration(decinfo, &num);
			for (UINT i = 0; i < num; i++)
			{
				if (decinfo[i].Usage == D3DDECLUSAGE_BLENDINDICES && !foundSkinning)
				{
					foundSkinning = TRUE;

					indices_offset = decinfo[i].Offset;
					//sometimes we only have blendindices
					numWeights = D3DVBF_0WEIGHTS;
				}
				if (decinfo[i].Usage == D3DDECLUSAGE_BLENDWEIGHT)
				{
					foundSkinning = TRUE;

					weights_offset = decinfo[i].Offset;
					streamNum = decinfo[i].Stream;
					//this should tell us how many skinWeights we have
					switch (decinfo[i].Type)
					{
					case D3DDECLTYPE_FLOAT4:
						numWeights = D3DVBF_DISABLE;
						foundSkinning = FALSE;
						break;
					case D3DDECLTYPE_FLOAT3:
						numWeights = D3DVBF_3WEIGHTS;
						break;
					case D3DDECLTYPE_FLOAT2:
						numWeights = D3DVBF_2WEIGHTS;
						break;
					case D3DDECLTYPE_FLOAT1:
						numWeights = D3DVBF_1WEIGHTS;
						break;
					default:
						numWeights = D3DVBF_DISABLE;
						foundSkinning = FALSE;
						break;
					}
				}
				if (decinfo[i].Usage == D3DDECLUSAGE_TEXCOORD)
				{
					numTextures++;
				}
				if (decinfo[i].Usage == D3DDECLUSAGE_BINORMAL)
				{
					replace0w_idx = i;
				}
			}

			if (gstate.skin_replace0w && numWeights == D3DVBF_0WEIGHTS)
			{
				if (num < ARRAYSIZE(decinfo))
				{
					if (replace0w_idx >= 0)
					{
						const D3DVERTEXELEMENT9 decl_end = D3DDECL_END();
						memcpy(&decinfo[num], &decl_end, sizeof(decl_end));

						static IDirect3DVertexDeclaration9* local_pDecl = nullptr;
						if (local_pDecl != nullptr)
							local_pDecl->Release();

						decinfo[replace0w_idx].Usage = D3DDECLUSAGE_BLENDWEIGHT;

						dev->CreateVertexDeclaration(decinfo, &local_pDecl);
						if (local_pDecl)
						{
							//override the decl, so that this new one gets set
							pDecl = local_pDecl;

							replace0w = true;
							weights_offset = decinfo[replace0w_idx].Offset;
							streamNum = decinfo[replace0w_idx].Stream;
							//this should tell us how many skinWeights we have
							switch (decinfo[replace0w_idx].Type)
							{
							case D3DDECLTYPE_FLOAT4:
								numWeights = D3DVBF_DISABLE;
								foundSkinning = FALSE;
								break;
							case D3DDECLTYPE_FLOAT3:
								numWeights = D3DVBF_3WEIGHTS;
								break;
							case D3DDECLTYPE_FLOAT2:
								numWeights = D3DVBF_2WEIGHTS;
								break;
							case D3DDECLTYPE_FLOAT1:
								numWeights = D3DVBF_1WEIGHTS;
								break;
							default:
								numWeights = D3DVBF_DISABLE;
								foundSkinning = FALSE;
								break;
							}
						}
					}
					else
						shared::common::log("d3d9", std::format("VertexDeclaration 0WEIGHTS did not find replacement space"), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
				}
				else
					shared::common::log("d3d9", std::format("VertexDeclaration 0WEIGHTS needs more space {:d}", num), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);
			}
		}
		else
			shared::common::log("d3d9", std::format("VertexDeclaration needs more space {:d}", num), shared::common::LOG_TYPE::LOG_TYPE_ERROR, true);

		gstate.declHasSkinning = foundSkinning;
		gstate.skinWeights = numWeights;
		gstate.skinIndices_offset = indices_offset;
		gstate.skinWeights_offset = weights_offset;
		gstate.skinWeight_streamNum = streamNum;
		gstate.skinWeight_replace0w = replace0w;

		gstate.numTextures = numTextures;
	}

	void renderer::on_set_vertex_shader(IDirect3DDevice9* dev, IDirect3DVertexShader9* pShader)
	{
		using namespace shared::common;
		using namespace comp::game;

		ShaderCache::SShaderClasify info;
		if(!shaders.is_shader_info_cached(pShader, info))
		{
			bool dump_shader = shared::common::flags::has_flag("dump_shaders");;
			std::string decomp;
			uint32_t hash = shaders.get_shader_decomp(pShader, &decomp, dump_shader);

			if (decomp.contains("$mSkinToViewTransforms"))
			{
				//skinning c0, 75 x vec4
				info.type = ShaderCache::SHADER_SKINNING;
			}
			else if (decomp.contains("$mSkinToModelTransforms"))
			{
				//skinning c0, 75 x vec4
				info.type = ShaderCache::SHADER_SKINNING_2;
			}
			else if (decomp.contains("$mModelViewProjection"))
			{
				//geo c0, 4 x vec4
				info.type = ShaderCache::SHADER_GEO;
				if (shared::common::flags::is_shader_ignored(hash))
				{
					info.type = ShaderCache::SHADER_IGNORE;
				}
				else if (decomp.contains("$vSrcWidthHeight"))
				{
					info.type = ShaderCache::SHADER_IGNORE;
				}
				else if (decomp.contains("$fDepth "))
				{
					info.type = ShaderCache::SHADER_SKY;
				}
				else if (decomp.contains("$vWScale"))
				{
					//light shader, we need stuff from pixel shader
					info.type = ShaderCache::SHADER_LIGHT;
				}
				else if (decomp.contains("$mModelView"))
				{
					//geo c4, 3 x vec4
					//info.type = ShaderCache::SHADER_MODEL;
				}
				else if (decomp.contains("$fDepthBias") || decomp.contains("mad oPos.xyz, c6, r0.w, r0"))
				{
					info.type = ShaderCache::SHADER_IGNORE;
				}
			}
			else if (decomp.contains("$fDeformMagnitude") && decomp.contains("$vWScale"))
			{
				//blue fumes
				info.type = ShaderCache::SHADER_IGNORE;
				//info.albedoStage = 3;
			}
			else if (decomp.contains("vs_1_1") && decomp.contains("mov oPos.zw, v0.zwww"))
			{
				info.type = ShaderCache::SHADER_UI;
			}
			else
			{
				info.type = ShaderCache::SHADER_IGNORE;
			}

			shaders.add_to_cache(hash, info);
		}

		gstate.vs_type = info.type;
		gstate.albedoStage = info.albedoStage;
	}

	void renderer::on_set_pixel_shader(IDirect3DDevice9* dev, IDirect3DPixelShader9* pShader)
	{

		using namespace shared::common;
		using namespace comp::game;

		ShaderCache::SShaderClasify info;
		if(!shaders.is_shader_info_cached(pShader, info))
		{
			bool dump_shader = shared::common::flags::has_flag("dump_shaders");;
			std::string decomp;
			uint32_t hash = shaders.get_shader_decomp(pShader, &decomp, dump_shader);

			if (shared::common::flags::is_shader_ignored(hash))
			{
				info.type = ShaderCache::SHADER_IGNORE;
			}
			else if (decomp.contains("$fDesaturationAmount") || decomp.contains("$vKernelWeights") || decomp.contains("GBufferDiffuse") ||
					 decomp.contains("SourceTex") || decomp.contains("$vFresnel") || decomp.contains("$EnvironmentMap") ||
					 decomp.contains("mov_pp oC0, v0"))
			{
				info.type = ShaderCache::SHADER_IGNORE;
			}
			else if (decomp.contains("$vBoundingSphereCenterRadiusSqrd") || decomp.contains("$vViewLightPosition"))
			{
				info.type = ShaderCache::SHADER_LIGHT;
			}
			else if (decomp.contains("ps_1_1") && decomp.contains("mad r0, r0, c2, c3"))
			{
				info.type = ShaderCache::SHADER_UI;
			}
			else if (decomp.contains("$ScreenMap"))
			{
				info.type = ShaderCache::SHADER_UI_FF;
			}
			else
			{
				info.type = ShaderCache::SHADER_UNKNOWN;
			}

			shaders.add_to_cache(hash, info);
		}

		gstate.ps_type = info.type;
	}

	static int vertexCountFromPrimitiveType(const D3DPRIMITIVETYPE PrimitiveType, const UINT PrimitiveCount)
	{
		int VertexCount = 0;

		switch (PrimitiveType) {
		case D3DPT_POINTLIST:
			VertexCount = PrimitiveCount;
			break;
		case D3DPT_LINELIST:
			VertexCount = PrimitiveCount * 2;
			break;
		case D3DPT_LINESTRIP:
			VertexCount = PrimitiveCount + 1;
			break;
		case D3DPT_TRIANGLELIST:
			VertexCount = PrimitiveCount * 3;
			break;
		case D3DPT_TRIANGLESTRIP:
		case D3DPT_TRIANGLEFAN:
			VertexCount = PrimitiveCount + 2;
			break;
		}

		return VertexCount;
	}

	HRESULT renderer::on_draw_primitive(IDirect3DDevice9* dev, const D3DPRIMITIVETYPE& PrimitiveType, const UINT& StartVertex, const UINT& PrimitiveCount)
	{
		// Wait for the first rendered prim before further init of the comp mod 
		if (!g_rendered_first_primitive) {
			g_rendered_first_primitive = true;
		}

		if (!is_initialized() || shared::globals::imgui_is_rendering) {
			return dev->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
		}

		static auto im = imgui::get();
		im->m_stats._drawcall_prim_incl_ignored.track_single();

		auto& ctx = setup_context(dev);

		// use any logic to conditionally set this to disable the vertex shader and use fixed function fallback
		bool render_with_ff = prepare_drawcall(dev, ctx);

		gstate.drawcallCount++;

		/*if (g_is_rendering_something)
		{
			// do stuff here, eg:
			ctx.modifiers.do_not_render = true;
		}*/

		// use fixed function fallback if true
		if (render_with_ff)
		{
			ctx.save_vs(dev);
			dev->SetVertexShader(nullptr);
			ctx.save_ps(dev);
			dev->SetPixelShader(nullptr);

			prepare_ff_texture_stages(dev, ctx);

			if(ctx.modifiers.do_gpu_skinning)
				process_gpu_skinning(dev, ctx, StartVertex, 0, vertexCountFromPrimitiveType(PrimitiveType, PrimitiveCount));
		}


		// example code - HUD is mostly drawn with non-indexed prims - the first with non-perspective proj might be a hud element
			//if (const auto viewport = game::vp; viewport)
			//{
			//	if (viewport->proj.m[3][3] == 1.0f) {
			//		manually_trigger_remix_injection(dev);
			//	}
			//}


		// ---------
		// draw

		auto hr = S_OK;

		// do not render next surface if set
		if (!ctx.modifiers.do_not_render) 
		{
			hr = dev->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
			im->m_stats._drawcall_prim.track_single();

			if (!render_with_ff) {
				im->m_stats._drawcall_using_vs.track_single();
			}
		}


		// ---------
		// post draw
		if (render_with_ff && ctx.modifiers.do_gpu_skinning)
		{
			dev->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, 0);
			dev->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
		}
		ctx.restore_all(dev);
		ctx.reset_context();

		return hr;
	}


	// ----

	HRESULT renderer::on_draw_indexed_prim(IDirect3DDevice9* dev, const D3DPRIMITIVETYPE& PrimitiveType, const INT& BaseVertexIndex, const UINT& MinVertexIndex, const UINT& NumVertices, const UINT& startIndex, const UINT& primCount)
	{
		if (!is_initialized() || shared::globals::imgui_is_rendering) {
			return dev->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
		}

		auto& ctx = setup_context(dev);
		const auto im = imgui::get();

		// use any logic to conditionally set this to disable the vertex shader and use fixed function fallback
		bool render_with_ff = prepare_drawcall(dev, ctx);

		gstate.drawcallCount++;

		// any drawcall modifications in here
		im->m_stats._drawcall_indexed_prim_incl_ignored.track_single();

		// if we set do_not_render somewhere before the actual drawcall -> do not draw and reset context
		if (ctx.modifiers.do_not_render) 
		{
			ctx.restore_all(dev);
			ctx.reset_context();
			return S_OK;
		}


		// --- 
		// The following code is here for example purposes and does not function on its own.


		// uncomment and debug into this to see vertex format of current drawcall
			// shared::utils::lookat_vertex_decl(dev);


		// Eg: render with fixed function if 'g_is_rendering_something' is true 
			//render_with_ff = g_is_rendering_something;


		// Eg: some condition to not render the next drawcall
			//if (your_condition) {
			//	ctx.modifiers.do_not_render = true;
			//}


		// If going from shader to fixed function, you'll need at least the world transformation matrix of the current mesh (given that you set the view and projection matrices in comp::on_begin_scene_cb())
		// If you found or figured out a memory address of a structure or matrices and want to render something via FF:
			//dev->SetTransform(D3DTS_WORLD, game::current_mesh_world_transform_matrix);


		// Some games might also modify the view/projection for certain meshes (eg. first person objects), so its not a bad idea to reset the view and proj matrices every time
		// Eg: If you found a structure in memory and setup the offset and the struct:
			//if (const auto viewport = game::vp; viewport)
			//{
			//	dev->SetTransform(D3DTS_VIEW, &viewport->view);
			//	dev->SetTransform(D3DTS_PROJECTION, &viewport->proj);
			//}


		// Identified some problematic drawcall with eg. blending issues?
		// Modify Renderstates but make sure to save them before doing so to not affect later drawcalls
			/*if (your_condition) 
			{
				ctx.save_rs(dev, D3DRS_ALPHABLENDENABLE);
				dev->SetRenderState(D3DRS_ALPHABLENDENABLE, true);

				ctx.save_rs(dev, D3DRS_BLENDOP);
				dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

				ctx.save_rs(dev, D3DRS_SRCBLEND);
				dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);

				ctx.save_rs(dev, D3DRS_DESTBLEND);
				dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

				ctx.save_tss(dev, D3DTSS_COLOROP);
				dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);

				ctx.save_tss(dev, D3DTSS_COLORARG1);
				dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

				ctx.save_tss(dev, D3DTSS_ALPHAOP);
				dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

				ctx.save_tss(dev, D3DTSS_ALPHAARG1);
				dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

				ctx.save_tss(dev, D3DTSS_ALPHAARG2);
				dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);

				ctx.save_rs(dev, D3DRS_TEXTUREFACTOR);
				dev->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_COLORVALUE(0, 0, 0, 1.0f)); 
			}*/

			
		// use fixed function fallback if true
		if (render_with_ff)
		{
			ctx.save_vs(dev);
			//dev->SetVertexShader(nullptr);
			ctx.save_ps(dev);
			//dev->SetPixelShader(nullptr);

			prepare_ff_texture_stages(dev, ctx);

			if (ctx.modifiers.do_gpu_skinning)
				process_gpu_skinning(dev, ctx, BaseVertexIndex, MinVertexIndex, NumVertices);
		}


		// ---------
		// draw

		auto hr = S_OK;

		// do not render next surface if set
		if (!ctx.modifiers.do_not_render) 
		{
			hr = dev->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);

			if (!shared::globals::imgui_is_rendering) {
				im->m_stats._drawcall_indexed_prim.track_single();
			}

			if (!render_with_ff) {
				im->m_stats._drawcall_indexed_prim_using_vs.track_single();
			}
		}


		// ---------
		// post draw
		if (render_with_ff && ctx.modifiers.do_gpu_skinning)
		{
			dev->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, 0);
			dev->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
		}
		ctx.restore_all(dev);
		ctx.reset_context();
		
		return hr;
	}

	void renderer::on_present(IDirect3DDevice9* dev)
	{
		gstate.drawcallCount = 0;
		gstate.skyHasRendered = false;
	}
	// ---

	// This can be used to manually trigger remix injection without ever needing to manually tag HUD textures
	// Can help if its hard to tag UI because it might be constantly changing - or if there is no UI
	// Call this on the first UI drawcall (you obv. need to detect that on your own via a hook or something)

	void renderer::manually_trigger_remix_injection(IDirect3DDevice9* dev)
	{
		if (!m_triggered_remix_injection)
		{
			auto& ctx = dc_ctx;

			dev->SetRenderState(D3DRS_FOGENABLE, FALSE);

			ctx.save_vs(dev);
			dev->SetVertexShader(nullptr);
			ctx.save_ps(dev);
			dev->SetPixelShader(nullptr); // needed

			ctx.save_rs(dev, D3DRS_ZWRITEENABLE);
			dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE); // required - const bool zWriteEnabled = d3d9State().renderStates[D3DRS_ZWRITEENABLE]; -> if (isOrthographic && !zWriteEnabled)

			struct CUSTOMVERTEX
			{
				float x, y, z, rhw;
				D3DCOLOR color;
			};

			const auto color = D3DCOLOR_COLORVALUE(0, 0, 0, 0);
			const auto w = -0.49f;
			const auto h = -0.495f;

			CUSTOMVERTEX vertices[] =
			{
				{ -0.5f, -0.5f, 0.0f, 1.0f, color }, // tl
				{     w, -0.5f, 0.0f, 1.0f, color }, // tr
				{ -0.5f,     h, 0.0f, 1.0f, color }, // bl
				{     w,     h, 0.0f, 1.0f, color }  // br
			};

			dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(CUSTOMVERTEX));

			ctx.restore_vs(dev);
			ctx.restore_ps(dev);
			ctx.restore_render_state(dev, D3DRS_ZWRITEENABLE);
			m_triggered_remix_injection = true;
		}
	}


	// ---

	// assembly stub at the start of a function that is eg. about to issue a bunch of drawcalls for a certain type of mesh
	/*__declspec (naked) void pre_render_something_stub()
	{
		__asm
		{
			mov     ebx, ecx;			// example; this might be an instruction that was overwritten by the hook
			cmp     eax, 0xFFFFFFFF;	// ^; remember that placing a hook takes 5 bytes so make sure to restore what was overwritten

			mov		g_is_rendering_something, 1;	// global to let us know that every drawcall afterwards is 'something' special we want to handle
			jmp		game::retn_addr__pre_draw_something;
		}
	}*/

	// assembly stub at the end of the same function to reset the global var
	/*__declspec (naked) void post_render__something_stub()
	{
		__asm
		{
			mov		g_is_rendering_something, 0;
			retn    0x10;	// eg: this hook was placed on the return instruction, replicate it here
		}
	}*/

	// ---

	renderer::renderer()
	{
		p_this = this;

		// #Step 5: Create hooks as required

		// Eg: detect rendering of some special kind of mesh because the function we hook is issuing a bunch of drawcalls for a certain type of mesh we want modify
		// START OF FUNC: set global helper bool
		// - Every drawcall from here on will be our special type of mesh
		// END OF FUNC: reset global helper bool

		// - retn_addr__pre_draw_something contains the offset we want to return to after our assembly stub, which is mostly the instruction after our hook.
		//   If the instruction at the hook spot is exactly 5 bytes, we can place the hook there and use the return addr minus 5 bytes to retrive the addr of the hook.

		// - hk_addr__post_draw_something contains the direct addr that we want to place the hook at
		//   This example will place a stub on the retn instruction which has additional padding bytes until the next function starts (so more than 5 bytes of space).
		//   That way we do not need an addr to return to and can just replicate the retn instruction in the stub

			//shared::utils::hook(game::retn_addr__pre_draw_something - 5u, pre_render_something_stub, HOOK_JUMP).install()->quick();
			//shared::utils::hook(game::hk_addr__post_draw_something, post_render__something_stub, HOOK_JUMP).install()->quick();


		// Eg: if you only want to modify things based on commandline flags
			//if (shared::common::flags::has_flag("your_flag"))
			//{
			//	// any hooks or mem edits here, eg:
			//
			//	// if you want to place a hook but cant find easy 5 bytes of space, you can nop eg. 7 bytes first, then place the hook
			//	// make sure to replicate the overwritten instructions in your stub if you do so
			//		//shared::utils::hook::nop(game::nop_addr__func2, 7); // nop 7 bytes at addr
			//		//shared::utils::hook(game::nop_addr__func2, your_stub, HOOK_JUMP).install()->quick(); // we can now safely place a hook here without messing up following instructions
			//}


		// -----
		m_initialized = true;
		shared::common::log("Renderer", "Module initialized.", shared::common::LOG_TYPE::LOG_TYPE_DEFAULT, false);
	}

	renderer::~renderer()
	{
		tex_addons::init_texture_addons(true);
	}

	void renderer::prepare_structs()
	{
		memset(&gstate, 0, sizeof(gstate));
		D3DXMatrixIdentity(&gstate.proj);
		D3DXMatrixIdentity(&gstate.view);
		D3DXMatrixIdentity(&gstate.proj_inv);
		D3DXMatrixIdentity(&gstate.view_inv);
		//gstate.albedoStage = shared::common::flags::get_config("AlbedoStage", 0);
		gstate.alphaRef = shared::common::flags::get_config("AlphaRef", 128);
		gstate.drawcallLimit = shared::common::flags::get_config("DrawcallLimit", 0);
		gstate.skin_replace0w = shared::common::flags::has_flag("skins_replace_0WEIGHT");
		gstate.dontDrawUntextured = shared::common::flags::has_flag("skip_untextured_draws");
	}

	void handle_mats_inversion()
	{
		if (gstate.proj_inv_dirty)
		{
			if (!D3DXMatrixInverse(&gstate.proj_inv, nullptr, &gstate.proj))
			{
				D3DXMatrixIdentity(&gstate.proj_inv);
			}
			gstate.proj_inv_dirty = 0;;
		}
		if (gstate.view_inv_dirty)
		{
			if (!D3DXMatrixInverse(&gstate.view_inv, nullptr, &gstate.view))
			{
				D3DXMatrixIdentity(&gstate.view_inv);
			}
			gstate.view_inv_dirty = 0;
		}
	}
}
