
#include "ShaderUtility.hlsli"
#include "TemporalRS.hlsli"

RWTexture2D<float4> OutVariance0 : register(u0);
RWTexture2D<float2> OutVariance1 : register(u1);
Texture2D<float3> InColor : register(t0);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);

cbuffer InlineConstants : register(b0)
{
	float2 RcpBufferDim;	// 1 / width, 1 / height
}

// For each 8x8 tile being processed, we need to fetch 10x10 window with 1 pixel border on each side
// in order to compute 3x3 moments of variance per pixel
#define BORDER_SIZE 1
#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 8
#define GROUP_SIZE (GROUP_SIZE_X * GROUP_SIZE_Y)
#define TILE_SIZE_X (GROUP_SIZE_X + 2 * BORDER_SIZE)
#define TILE_SIZE_Y (GROUP_SIZE_Y + 2 * BORDER_SIZE)
#define TILE_PIXEL_COUNT (TILE_SIZE_X * TILE_SIZE_Y)

groupshared float ldsR[TILE_PIXEL_COUNT];
groupshared float ldsG[TILE_PIXEL_COUNT];
groupshared float ldsB[TILE_PIXEL_COUNT];

void StoreRGB(uint ldsIdx, float3 RGB)
{
	ldsR[ldsIdx] = RGB.r;
	ldsG[ldsIdx] = RGB.g;
	ldsB[ldsIdx] = RGB.b;
}

float3 LoadRGB(uint ldsIdx)
{
	return float3(ldsR[ldsIdx], ldsG[ldsIdx], ldsB[ldsIdx]);
}

[RootSignature(Temporal_RootSig)]
[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void main(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex)
{
	const uint ldsHalfPitch = TILE_SIZE_X / 2;

	for (uint i = GI; i < (TILE_PIXEL_COUNT/4); i += GROUP_SIZE)
	{
		uint X = (i % ldsHalfPitch) * 2;
		uint Y = (i / ldsHalfPitch) * 2;
		uint TopLeftIdx = X + Y * TILE_SIZE_X;
		int2 TopLeftST = Gid.xy * uint2(GROUP_SIZE_X, GROUP_SIZE_Y) - 1 + uint2(X, Y);
		float2 UV = RcpBufferDim * (TopLeftST + float2(1, 1));

		float4 R4 = InColor.GatherRed(LinearSampler, UV);
		float4 G4 = InColor.GatherGreen(LinearSampler, UV);
		float4 B4 = InColor.GatherBlue(LinearSampler, UV);
		StoreRGB(TopLeftIdx, float3(R4.w, G4.w, B4.w));
		StoreRGB(TopLeftIdx + 1, float3(R4.z, G4.z, B4.z));
		StoreRGB(TopLeftIdx + TILE_SIZE_X, float3(R4.x, G4.x, B4.x));
		StoreRGB(TopLeftIdx + 1 + TILE_SIZE_X, float3(R4.y, G4.y, B4.y));
	}

	GroupMemoryBarrierWithGroupSync();
	uint Idx = GTid.x + GTid.y * TILE_SIZE_X + (TILE_SIZE_X + 1);
	uint2 ST = DTid.xy;

	float3 m1, m2, temp;
	float n = 9.f;
	m1 = m2 = temp = float3(0, 0, 0);

	for (int k = -1; k <= 1; k++) {
		for (int j = -1; j <= 1; j++) {
			temp = LoadRGB(Idx + k + j*TILE_SIZE_X);
			temp = ColorToLum(temp);
			m1 += temp;
			m2 += temp*temp;
		}
	}
	m1 /= n;
	m2 /= n;

	OutVariance0[ST] = float4(m1, m2.x);
	OutVariance1[ST] = m2.yz;

}