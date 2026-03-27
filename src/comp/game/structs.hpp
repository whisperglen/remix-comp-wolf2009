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
		bool view_inv_dirty;
		D3DXMATRIX view_inv;
		bool proj_inv_dirty;
		D3DXMATRIX proj_inv;

		bool hasMV;
		int vs_type;
		int ps_type;
		int albedoStage;
		int alphaRef;

		bool declHasSkinning;
		D3DVERTEXBLENDFLAGS weights;

		void* renderTargets[10];
		float vs_contants[256][4];
		float ps_contants[256][4];
	};
}
