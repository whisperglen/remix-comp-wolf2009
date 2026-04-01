// SPDX-License-Identifier: MIT
// Original author: xoxor4d (https://github.com/xoxor4d/remix-comp-base)
// Incorporated via Ekozmaster's MIT-licensed project and Kim2091's fork
// See THIRD-PARTY-NOTICES.md for the full license texts

#pragma once

namespace comp
{
	/*
	 * Optional skinning module for FFP conversion.
	 *
	 * When enabled via [Skinning] Enabled=1 in remix-comp.ini, this module
	 * handles skinned mesh rendering by expanding compressed vertex data,
	 * uploading bone matrices via SetTransform(WORLDMATRIX(i)), and drawing
	 * with D3D FFP indexed vertex blending.
	 *
	 * Only enable after rigid FFP geometry works correctly.
	 */
	class skinning final : public shared::common::loader::component_module
	{
	public:
		skinning();
		~skinning();

		static inline skinning* p_this = nullptr;
		static skinning* get() { return p_this; }

		static bool is_available()
		{
			return p_this != nullptr;
		}

		// Called from renderer when cur_decl_is_skinned
		HRESULT draw_skinned_dip(IDirect3DDevice9* dev,
			D3DPRIMITIVETYPE pt, INT base_vtx, UINT min_vtx, UINT num_verts,
			UINT start_idx, UINT prim_count);
		void disable_skinning(IDirect3DDevice9* dev);

		// Called on device reset
		void on_reset();

	private:
		static constexpr int SKIN_VTX_SIZE = 40;
		static constexpr UINT STREAMING_CAPACITY = 131072; // 128K verts = 4MB

		IDirect3DVertexDeclaration9* skin_exp_decl_ = nullptr;

		// Single streaming dynamic VB (DISCARD/NOOVERWRITE pattern)
		IDirect3DVertexBuffer9* streaming_vb_ = nullptr;
		UINT streaming_cursor_ = 0;
		UINT last_discard_frame_ = UINT_MAX;

		bool initialized_ = false;

		void create_expanded_decl(IDirect3DDevice9* dev);
		void ensure_streaming_vb(IDirect3DDevice9* dev);

		// Expand vertices into streaming VB, returns start vertex offset or UINT_MAX on failure
		UINT expand_vertices(IDirect3DDevice9* dev,
			IDirect3DVertexBuffer9* src_vb, UINT stream_offset, INT base_vtx, UINT num_verts, UINT stride);

		// Bone upload
		void upload_bones(IDirect3DDevice9* dev);

		// Format decoders
		static float half_to_float(unsigned short h);
		static void decode_normal(const unsigned char* src, int type, float* out);
	};
}
