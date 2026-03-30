#pragma once
#include <d3d9.h>
#include "d3dx9.h"

namespace comp::game
{
	// place any game structures here

	struct state_s
	{
		D3DXMATRIX view;
		D3DXMATRIX proj;
		D3DXMATRIX view_inv;
		D3DXMATRIX proj_inv;
		bool view_inv_dirty;
		bool proj_inv_dirty;

		unsigned int drawcallCount;
		unsigned int drawcallLimit;

		bool hasMV;
		int vs_type;
		int ps_type;
		int albedoStage;
		int alphaRef;

		bool declHasSkinning;
		bool skin_replace0w;
		D3DVERTEXBLENDFLAGS skinWeights;
		int skinIndices_offset;
		int skinWeights_offset;
		int skinWeight_streamNum;
		bool skinWeight_replace0w;

		bool dontDrawUntextured;
		int numTextures;

		bool skyHasRendered;

		void* renderTargets[10];
		float vs_constants[256][4];
		float ps_constants[256][4];
	};
}
