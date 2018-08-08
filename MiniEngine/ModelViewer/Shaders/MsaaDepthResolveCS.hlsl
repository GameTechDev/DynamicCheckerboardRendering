///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018, Intel Corporation
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of 
// the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
// SOFTWARE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MsaaResolve_RootSig \
    "RootFlags(0), " \
    "CBV(b0), " \
    "DescriptorTable(SRV(t0, numDescriptors = 1))," \
    "DescriptorTable(UAV(u0, numDescriptors = 1))"

//"RootConstants(b0, num32BitConstants = 4), " \

Texture2DMS<float> MsaaInDepth : register(t0);
RWTexture2D<float> OutDepth : register(u0);

cbuffer CB0 : register(b0)
{
	float ZMagic;				// (zFar - zNear) / zNear
	float DepthMode;			// 0: average depth, 1: minDepth
}

[RootSignature(MsaaResolve_RootSig)]
[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	float Depth = 0.f;
	uint Width, Height, SampleCount;
	float LinearZ; 
	MsaaInDepth.GetDimensions(Width, Height, SampleCount);
	if (DepthMode > 0) {	// find the minDepth of those samples
		Depth = 100.f;
		for (uint i = 0; i < SampleCount; i++) {
			LinearZ = 1.0 / (ZMagic * MsaaInDepth.Load(DTid.xy, i) + 1.0);
			if (Depth > LinearZ) Depth = LinearZ;
		}
		OutDepth[DTid.xy] = Depth;
	}
	else {	// Compute an average depth of all samples
		for (uint i = 0; i < SampleCount; i++) {
			LinearZ = 1.0 / (ZMagic * MsaaInDepth.Load(DTid.xy, i) + 1.0);
			Depth += LinearZ;
		}
		OutDepth[DTid.xy] = Depth / (float)SampleCount;
	}
}