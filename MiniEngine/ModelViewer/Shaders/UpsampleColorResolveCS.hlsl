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

Texture2DMS<float4> DownSizedInColor : register(t0);
RWTexture2D<float4> OutColor : register(u0);

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
	float4 Color;
	DownSizedInColor.GetDimensions(Width, Height, SampleCount);
	uint2 ID;
	ID.x = DTid.x / UpsampleFactor.x;
	ID.y = DTid.y / UpsampleFactor.y;

	int Temp = (DTid.x % (uint)UpsampleFactor.x) + (DTid.y % (uint)UpsampleFactor.y) * UpsampleFactor.x;
	int Offset;
	if (SampleCount == 2) Offset = offset2[Temp];
	else if (SampleCount == 4) Offset = offset4[Temp];
	else if (SampleCount == 8) Offset = offset8[Temp];
	else if (SampleCount == 1) Offset = 0;

	Color = DownSizedInColor.Load(ID.xy, Offset);

	OutColor[DTid.xy] = Color;

	//--------
	// Offset and SamplePosition Validation

	/*float2 SamplePosition = DownSizedInColor.GetSamplePosition(Offset);
	Color = float4(0, 0, 0, 1);

	if (SampleCount == 2) {
		if ((DTid.x % 2) == 0)
			Color += float4(pos2[1], 0, 0);
		else
			Color += float4(pos2[0], 0, 0);
	}
	else if (SampleCount == 4) {
		if ((DTid.y % 2) == 0) {
			if ((DTid.x%2) == 0)
				Color += float4(pos4[0], 0, 0);
			else
				Color += float4(pos4[1], 0, 0);
		}
		else {
			if ((DTid.x%2) == 0)
				Color += float4(pos4[2], 0, 0);
			else
				Color += float4(pos4[3], 0, 0);
		}
	}
	else if (SampleCount == 8) {
			if ((DTid.y % 2) == 0) {
				if ((DTid.x % 4) == 0)
					Color += float4(pos8[5], 0, 0);
				else if ((DTid.x % 4) ==1)
					Color += float4(pos8[3], 0, 0);
				else if ((DTid.x %4) == 2)
					Color += float4(pos8[0], 0, 0);
				else
					Color += float4(pos8[7], 0, 0);
			}
			else {
				if ((DTid.x % 4) == 0)
					Color += float4(pos8[4], 0, 0);
				else if ((DTid.x % 4) == 1)
					Color += float4(pos8[1], 0, 0);
				else if ((DTid.x % 4) == 2)
					Color += float4(pos8[6], 0, 0);
				else
					Color += float4(pos8[2], 0, 0);
			}
	}

	Color -= float4(SamplePosition, 0, 0);

	OutColor[DTid.xy] = Color;
*/
	// Standard sample patterns

	//	+static const tSamplePos pos1[] = {
	//	+{ 0.0 / 16.0,  0.0 / 16.0 },
	//	+};
	//+
	//	+    // standard sample positions for 2, 4, 8, and 16 samples.
	//	+static const tSamplePos pos2[] = {
	//	+{ 4.0 / 16.0,  4.0 / 16.0 },{ -4.0 / 16.0, -4.0 / 16.0 },
	//	+};
	//+
	//	+static const tSamplePos pos4[] = {
	//	+{-2.0 / 16.0, -6.0 / 16.0 },{ 6.0 / 16.0, -2.0 / 16.0 },{ -6.0 / 16.0,  2.0 / 16.0 },{ 2.0 / 16.0,  6.0 / 16.0 },
	//	+};
	//+
	//	+static const tSamplePos pos8[] = {
	//	+{ 1.0 / 16.0, -3.0 / 16.0 },{ -1.0 / 16.0,  3.0 / 16.0 },{ 5.0 / 16.0,  1.0 / 16.0 },{ -3.0 / 16.0, -5.0 / 16.0 },
	//	+{-5.0 / 16.0,  5.0 / 16.0 },{ -7.0 / 16.0, -1.0 / 16.0 },{ 3.0 / 16.0,  7.0 / 16.0 },{ 7.0 / 16.0, -7.0 / 16.0 },
	//	+};
	//+
	//	+static const tSamplePos pos16[] = {
	//	+{ 1.0 / 16.0,  1.0 / 16.0 },{ -1.0 / 16.0, -3.0 / 16.0 },{ -3.0 / 16.0,  2.0 / 16.0 },{ 4.0 / 16.0, -1.0 / 16.0 },
	//	+{-5.0 / 16.0, -2.0 / 16.0 },{ 2.0 / 16.0,  5.0 / 16.0 },{ 5.0 / 16.0,  3.0 / 16.0 },{ 3.0 / 16.0, -5.0 / 16.0 },
	//	+{-2.0 / 16.0,  6.0 / 16.0 },{ 0.0 / 16.0, -7.0 / 16.0 },{ -4.0 / 16.0, -6.0 / 16.0 },{ -6.0 / 16.0,  4.0 / 16.0 },
	//	+{-8.0 / 16.0,  0.0 / 16.0 },{ 7.0 / 16.0, -4.0 / 16.0 },{ 6.0 / 16.0,  7.0 / 16.0 },{ -7.0 / 16.0, -8.0 / 16.0 },
	//	+};

}