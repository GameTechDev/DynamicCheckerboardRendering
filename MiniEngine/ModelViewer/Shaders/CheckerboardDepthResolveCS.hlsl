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

#define CheckerboardResolve_RootSig \
    "RootFlags(0), " \
    "CBV(b0), " \
    "DescriptorTable(SRV(t0, numDescriptors = 2))," \
    "DescriptorTable(UAV(u0, numDescriptors = 1))"

Texture2DMS<float> DownSizedInDepth2x0 : register(t0);
Texture2DMS<float> DownSizedInDepth2x1 : register(t1);
RWTexture2D<float> OutDepth : register(u0);

cbuffer CB0 : register(b0)
{
	float ZMagic;				// (zFar - zNear) / zNear
	float FrameOffset;
}

[RootSignature(CheckerboardResolve_RootSig)]
[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	float Depth;
	float LinearZ;

	uint2 ID;
	uint SubSampleID;
	int Offset;
	
	ID = DTid.xy * 0.5;
	SubSampleID = DTid.x % 2 + (DTid.y % 2) * 2;

	if (FrameOffset > 0) {
		// sample #1 and #2 is rendered, #0 and #3 need to be filled
		if (SubSampleID == 1 || SubSampleID == 2) {
			if (SubSampleID == 1)	ID.x = ID.x + 1;
			Offset = offset2[SubSampleID - 1];
			Depth = DownSizedInDepth2x1.Load(ID.xy, Offset);
		}
		else {
			// simply copying the other (vertically) sample's depth 
			if (SubSampleID == 3) ID.x = ID.x + 1;
			Offset = offset2[SubSampleID % 2];
			Depth = DownSizedInDepth2x1.Load(ID.xy, Offset);
		}
	}
	else {
		// sample #0 and #3 is rendered, #1 and #2 need to be filled
		if (SubSampleID == 0 || SubSampleID == 3) {
			Offset = offset2[SubSampleID % 2];
			Depth = DownSizedInDepth2x0.Load(ID.xy, Offset);
		}
		else {
			// simply copying the other (vertically) sample's depth 
			Offset = offset2[(SubSampleID +2)%4];
			Depth = DownSizedInDepth2x0.Load(ID.xy, Offset);
		}
	}

	LinearZ = 1.0 / (ZMagic * Depth + 1.0);
	OutDepth[DTid.xy] = LinearZ;
}