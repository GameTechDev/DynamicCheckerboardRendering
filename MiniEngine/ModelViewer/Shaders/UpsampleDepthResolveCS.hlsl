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

#include "SamplePositions.hlsli"

#define UpsampleResolve_RootSig \
    "RootFlags(0), " \
    "CBV(b0), " \
    "DescriptorTable(SRV(t0, numDescriptors = 1))," \
    "DescriptorTable(UAV(u0, numDescriptors = 1))"

Texture2DMS<float> DownSizedInDepth : register(t0);
RWTexture2D<float> OutDepth : register(u0);

cbuffer CB0 : register(b0)
{
	float ZMagic;				// (zFar - zNear) / zNear
	float2 UpsampleFactor;		
}

[RootSignature(UpsampleResolve_RootSig)]
[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint Width, Height, SampleCount;
	float Depth;
	float LinearZ;
	DownSizedInDepth.GetDimensions(Width, Height, SampleCount);

	uint2 ID;
	ID.x = DTid.x / UpsampleFactor.x;
	ID.y = DTid.y / UpsampleFactor.y;

	/*uint Offset;
	Offset = (DTid.x % (uint)UpsampleFactor.x) + (DTid.y % (uint)UpsampleFactor.y) * UpsampleFactor.x;
	*/
	int Temp = (DTid.x % (uint)UpsampleFactor.x) + (DTid.y % (uint)UpsampleFactor.y) * UpsampleFactor.x;
	int Offset;
	if (SampleCount == 2) Offset = offset2[Temp];
	else if (SampleCount == 4) Offset = offset4[Temp];
	else if (SampleCount == 8) Offset = offset8[Temp];
	else if (SampleCount == 1) Offset = 0;

	Depth = DownSizedInDepth.Load(ID.xy, Offset);

	LinearZ = 1.0 / (ZMagic * Depth + 1.0);

	OutDepth[DTid.xy] = LinearZ;
}