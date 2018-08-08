// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author(s):	James Stanard
//

// Modified 2018, Intel Corporation
// Added derivatives for customizable mip levels

#include "ModelViewerRS.hlsli"

struct VSOutput
{
	sample float4 pos : SV_Position;
	sample float2 uv : TexCoord0;
};

Texture2D<float4>	texDiffuse		: register(t0);
Texture2D<float>	texOpacity		: register(t6);
SamplerState		sampler0		: register(s0);

[RootSignature(ModelViewer_RootSig)]
void main(VSOutput vsOutput)
{
#ifdef DDXY_BIAS
	float2 tdx = ddx_fine(vsOutput.uv);
	float2 tdy = ddy_fine(vsOutput.uv);

	if (texOpacity.SampleGrad(sampler0, vsOutput.uv, tdx * DDXY_BIAS, tdy * DDXY_BIAS).x < 0.5)
		discard;
#else
	if (texOpacity.Sample(sampler0, vsOutput.uv).x < 0.5)
		discard;
#endif

}
