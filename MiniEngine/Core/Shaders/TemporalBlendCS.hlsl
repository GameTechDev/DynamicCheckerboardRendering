//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

#include "TemporalRS.hlsli"
#include "ShaderUtility.hlsli"
#include "PixelPacking_Velocity.hlsli"

static const uint kLdsPitch = 16;
static const uint kLdsRows = 16;

RWTexture2D<float4> OutTemporal : register(u0);

Texture2D<packed_velocity_t> VelocityBuffer : register(t0);
Texture2D<float3> InColor : register(t1);
Texture2D<float4> InTemporal : register(t2);
Texture2D<float> CurDepth : register(t3);
Texture2D<float> PreDepth : register(t4);
Texture2D<float4> VarianceMap0 : register(t5);
Texture2D<float2> VarianceMap1 : register(t6);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);

groupshared float ldsDepth[kLdsPitch * kLdsRows];
groupshared float ldsR[kLdsPitch * kLdsRows];
groupshared float ldsG[kLdsPitch * kLdsRows];
groupshared float ldsB[kLdsPitch * kLdsRows];

static const float PI = 3.1416;


cbuffer CB1 : register(b1)
{
    float2 RcpBufferDim;    // 1 / width, 1 / height
    float2 RcpColorBufferDim; // DRR - incolor might have different dimensions
    float2 _Pad;
    float TemporalBlendControl;
    float RcpSpeedLimiter;
    float2 JitterDelta;
	float2 JitterVector;
	uint  BicubicFiltering;
	uint  VisualSilhouette;
	uint  FrameOffset;
	uint  VisualDisocclusion;
	float VarianceExtension;
	float AccModifier;
}

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

float2 STtoUV(float2 ST)
{
	return (ST + 0.5) * RcpBufferDim;
}

float3 ClipColor(float3 Color, float3 BoxMin, float3 BoxMax, float Dilation = 1.0)
{
    float3 BoxCenter = (BoxMax + BoxMin) * 0.5;
    float3 HalfDim = (BoxMax - BoxMin) * 0.5 * Dilation + 0.001;
    float3 Displacement = Color - BoxCenter;
    float3 Units = abs(Displacement / HalfDim);
    float MaxUnit = max(max(Units.x, Units.y), max(Units.z, 1.0));
    return BoxCenter + Displacement / MaxUnit;
}

bool ColorBoxTest(float3 Color, float3 BoxMin, float3 BoxMax) {
	return (
		(Color.x >= BoxMin.x)
		&& (Color.y >= BoxMin.y)
		&& (Color.z >= BoxMin.z)
		&& (Color.x <= BoxMax.x)
		&& (Color.y <= BoxMax.y)
		&& (Color.z <= BoxMax.z)
		);
}

float MaxOf(float4 Depths) { return max(max(Depths.x, Depths.y), max(Depths.z, Depths.w)); }
float MinOf(float4 Depths) { return min(min(Depths.x, Depths.y), min(Depths.z, Depths.w)); }

// Find closest pixel in 3x3 window with plus pattern
int2 GetClosestPixel(uint Idx, out float ClosestDepth)
{
    float DepthO = ldsDepth[Idx];
    float DepthW = ldsDepth[Idx - 1];
    float DepthE = ldsDepth[Idx + 1];
    float DepthN = ldsDepth[Idx - kLdsPitch];
    float DepthS = ldsDepth[Idx + kLdsPitch];

    ClosestDepth = min(DepthO, min(min(DepthW, DepthE), min(DepthN, DepthS)));

    if (DepthN == ClosestDepth)
        return int2(0, -1);
    else if (DepthS == ClosestDepth)
        return int2(0, +1);
    else if (DepthW == ClosestDepth)
        return int2(-1, 0);
    else if (DepthE == ClosestDepth)
        return int2(+1, 0);

    return int2(0, 0);
}

// Find closest pixel in 3x3 window with cross pattern
int2 GetClosestPixelX(uint Idx, out float ClosestDepth)
{
	float DepthO = ldsDepth[Idx];
	float DepthNW = ldsDepth[Idx - 1 - kLdsPitch];
	float DepthNE = ldsDepth[Idx + 1 - kLdsPitch];
	float DepthSW = ldsDepth[Idx - 1 + kLdsPitch];
	float DepthSE = ldsDepth[Idx + 1 + kLdsPitch];

	ClosestDepth = min(DepthO, min(min(DepthNW, DepthNE), min(DepthSW, DepthSE)));

	if (DepthNW == ClosestDepth)
		return int2(-1, -1);
	else if (DepthNE == ClosestDepth)
		return int2(+1, -1);
	else if (DepthSW == ClosestDepth)
		return int2(-1, +1);
	else if (DepthSE == ClosestDepth)
		return int2(+1, +1);

	return int2(0, 0);
}


// Find closest pixel in 3x3 window with both plus and cross pattern
int2 GetClosestPixel3x3(uint Idx, out float ClosestDepth)
{
	int2 closestPixX, closestPixP, closestPix;
	float closestDepthX, closestDepthP;

	closestPixP = GetClosestPixel(Idx, closestDepthP);
	closestPixX = GetClosestPixelX(Idx, closestDepthX);

	ClosestDepth = closestDepthP;
	closestPix = closestPixP;

	if (closestDepthX < closestDepthP) {
		closestPix = closestPixX;
		ClosestDepth = closestDepthX;
	}
	return closestPix;
}


// Find closest pixel in 7x7 window but only sample at the end of corner and plus
int2 GetClosestPixelExtend(uint Idx, out float ClosestDepth, float VarExtensionRequest)
{
	int offset = 1 + floor(VarExtensionRequest);
	float DepthO = ldsDepth[Idx];
	float DepthW = ldsDepth[Idx - offset];
	float DepthE = ldsDepth[Idx + offset];
	float DepthN = ldsDepth[Idx - offset *kLdsPitch];
	float DepthS = ldsDepth[Idx + offset *kLdsPitch];
	float DepthNW = ldsDepth[Idx - offset * kLdsPitch - offset];
	float DepthNE = ldsDepth[Idx - offset * kLdsPitch + offset];
	float DepthSW = ldsDepth[Idx + offset * kLdsPitch - offset];
	float DepthSE = ldsDepth[Idx + offset * kLdsPitch + offset];

	float ClosestPlus = min(min(DepthW, DepthE), min(DepthN, DepthS));
	float ClosestCross =min(min(DepthSE, DepthSW), min(DepthNE, DepthNW));
	ClosestDepth = min(DepthO, min(ClosestPlus, ClosestCross));
	if (DepthN == ClosestDepth)
		return int2(0, -offset);
	else if (DepthS == ClosestDepth)
		return int2(0, +offset);
	else if (DepthW == ClosestDepth)
		return int2(-offset, 0);
	else if (DepthE == ClosestDepth)
		return int2(+offset, 0);
	else if (DepthSE == ClosestDepth)
		return int2(+offset, +offset);
	else if (DepthSW == ClosestDepth)
		return int2(-offset, +offset);
	else if (DepthNE == ClosestDepth)
		return int2(+offset, -offset);
	else if (DepthNW == ClosestDepth)
		return int2(-offset, -offset);
	return int2(0, 0);
}

// Find closest pixel in 7x7 window but only sample at the end of corner and plus, with offset
int2 GetClosestPixelExtendAdditional(uint Idx, out float ClosestDepth)
{
	int offset = 3;
	int offset1 = 1;
	float DepthW = ldsDepth[Idx - offset + offset1 * kLdsPitch];
	float DepthE = ldsDepth[Idx + offset - offset1 * kLdsPitch];
	float DepthN = ldsDepth[Idx - offset * kLdsPitch - offset1];
	float DepthS = ldsDepth[Idx + offset * kLdsPitch + offset1];
	float DepthNW = ldsDepth[Idx - offset * kLdsPitch - offset + offset1 * kLdsPitch];
	float DepthNE = ldsDepth[Idx - offset * kLdsPitch + offset - offset1];
	float DepthSW = ldsDepth[Idx + offset * kLdsPitch - offset + offset1];
	float DepthSE = ldsDepth[Idx + offset * kLdsPitch + offset - offset1 * kLdsPitch];

	float ClosestPlus = min(min(DepthW, DepthE), min(DepthN, DepthS));
	float ClosestCross = min(min(DepthSE, DepthSW), min(DepthNE, DepthNW));
	ClosestDepth = min(ClosestPlus, ClosestCross);
	if (DepthN == ClosestDepth)
		return int2(-offset1, -offset);
	else if (DepthS == ClosestDepth)
		return int2(+offset1, +offset);
	else if (DepthW == ClosestDepth)
		return int2(-offset, +offset1);
	else if (DepthE == ClosestDepth)
		return int2(+offset, -offset1);
	else if (DepthSE == ClosestDepth)
		return int2(+offset, +offset - offset1);
	else if (DepthSW == ClosestDepth)
		return int2(-offset + offset1, +offset);
	else if (DepthNE == ClosestDepth)
		return int2(+offset - offset1, -offset);
	else if (DepthNW == ClosestDepth)
		return int2(-offset, -offset + offset1);
	return int2(0, 0);
}

//=======================================================================================
float4 CubicHermite(float4 A, float4 B, float4 C, float4 D, float t)
{
	float t2 = t*t;
	float t3 = t*t*t;
	float4 a = -A / 2.0 + (3.0*B) / 2.0 - (3.0*C) / 2.0 + D / 2.0;
	float4 b = A - (5.0*B) / 2.0 + 2.0*C - D / 2.0;
	float4 c = -A / 2.0 + C / 2.0;
	float4 d = B;

	return a*t3 + b*t2 + c*t + d;
}
//=======================================================================================
float4 CubicLagrange(float4 A, float4 B, float4 C, float4 D, float t)
{

	float c_x0 = -1.0f;
	float c_x1 = 0.0f;
	float c_x2 = 1.0f;
	float c_x3 = 2.0f;

	return
		A *
		(
		(t - c_x1) / (c_x0 - c_x1) *
			(t - c_x2) / (c_x0 - c_x2) *
			(t - c_x3) / (c_x0 - c_x3)
			) +
		B *
		(
		(t - c_x0) / (c_x1 - c_x0) *
			(t - c_x2) / (c_x1 - c_x2) *
			(t - c_x3) / (c_x1 - c_x3)
			) +
		C *
		(
		(t - c_x0) / (c_x2 - c_x0) *
			(t - c_x1) / (c_x2 - c_x1) *
			(t - c_x3) / (c_x2 - c_x3)
			) +
		D *
		(
		(t - c_x0) / (c_x3 - c_x0) *
			(t - c_x1) / (c_x3 - c_x1) *
			(t - c_x2) / (c_x3 - c_x2)
			);
}
// 16-tap bicubic sampling
float4 BicubicSampling16(Texture2D<float4> Texture, float2 STV) {
	float2 pixel = float2(STV.x / RcpBufferDim.x, STV.y / RcpBufferDim.y) + float2(0.5f, 0.5f);
	float2 c_onePixel = 1.f *RcpBufferDim;
	float2 c_twoPixels = 2.f *RcpBufferDim;
	float2 Frac;
	Frac.x = frac(pixel.x);
	Frac.y = frac(pixel.y);

	float2 P = floor(pixel) -float2(0.5f, 0.5f);
	P = P*RcpBufferDim;
	//
	float4 C00 = InTemporal.SampleLevel(PointSampler, P + float2(-c_onePixel.x, -c_onePixel.y), 0);
	float4 C10 = InTemporal.SampleLevel(PointSampler, P + float2(0.f, -c_onePixel.y), 0);
	float4 C20 = InTemporal.SampleLevel(PointSampler, P + float2(c_onePixel.x, -c_onePixel.y), 0);
	float4 C30 = InTemporal.SampleLevel(PointSampler, P + float2(c_twoPixels.x, -c_onePixel.y), 0);
	//
	float4 C01 = InTemporal.SampleLevel(PointSampler, P + float2(-c_onePixel.x, 0.f), 0);
	float4 C11 = InTemporal.SampleLevel(PointSampler, P + float2(0.f, 0.f), 0);
	float4 C21 = InTemporal.SampleLevel(PointSampler, P + float2(c_onePixel.x, 0.f), 0);
	float4 C31 = InTemporal.SampleLevel(PointSampler, P + float2(c_twoPixels.x, 0.f), 0);
	//
	float4 C02 = InTemporal.SampleLevel(PointSampler, P + float2(-c_onePixel.x, c_onePixel.y), 0);
	float4 C12 = InTemporal.SampleLevel(PointSampler, P + float2(0.f, c_onePixel.y), 0);
	float4 C22 = InTemporal.SampleLevel(PointSampler, P + float2(c_onePixel.x, c_onePixel.y), 0);
	float4 C32 = InTemporal.SampleLevel(PointSampler, P + float2(c_twoPixels.x, c_onePixel.y), 0);
	//
	float4 C03 = InTemporal.SampleLevel(PointSampler, P + float2(-c_onePixel.x, c_twoPixels.y), 0);
	float4 C13 = InTemporal.SampleLevel(PointSampler, P + float2(0.f, c_twoPixels.y), 0);
	float4 C23 = InTemporal.SampleLevel(PointSampler, P + float2(c_onePixel.x, c_twoPixels.y), 0);
	float4 C33 = InTemporal.SampleLevel(PointSampler, P + float2(c_twoPixels.x, c_twoPixels.y), 0);

	float4 color;

	float4 CP0X = CubicHermite(C00, C10, C20, C30, Frac.x);
	float4 CP1X = CubicHermite(C01, C11, C21, C31, Frac.x);
	float4 CP2X = CubicHermite(C02, C12, C22, C32, Frac.x);
	float4 CP3X = CubicHermite(C03, C13, C23, C33, Frac.x);

	color = CubicHermite(CP0X, CP1X, CP2X, CP3X, Frac.y);
	return color;
}
// optimized 5-tap bicubic sampling
float4 BicubicSampling5(Texture2D<float4> Texture, float2 STV) {
	float2 pixel = float2(STV.x / RcpBufferDim.x, STV.y / RcpBufferDim.y) + float2(0.5f, 0.5f);
	float2 c_onePixel = 1.f *RcpBufferDim;
	float2 c_twoPixels = 2.f *RcpBufferDim;
	float2 Frac;
	Frac.x = frac(pixel.x);
	Frac.y = frac(pixel.y);

	float2 P = floor(pixel) - float2(0.5f, 0.5f);
	P = P*RcpBufferDim;
	// 5-tap bicubic sampling (for Hermite/Carmull-Rom filter) -- (approximate from original 16->9-tap bilinear fetching) 
	float2 t = Frac;
	float2 t2 = t*t;
	float2 t3 = t2*t;
	float s = 0.5;	// s is potentially adjustable
	float2 w0 = -s*t3 + 2 * s*t2 - s*t;
	float2 w1 = (2 - s) * t3 + (s - 3)*t2 + 1;
	float2 w2 = (s - 2)*t3 + (3 - 2 * s)*t2 + s*t;
	float2 w3 = s*t3 - s*t2;
	float2 s0 = w1 + w2;
	float2 f0 = w2 / (w1 + w2);
	float2 m0 = P + f0 * RcpBufferDim;
	float2 tc0 = P - c_onePixel;
	float2 tc3 = P + c_twoPixels;
	
	float4 A = Texture.SampleLevel(LinearSampler, float2(m0.x, tc0.y), 0);
	float4 B = Texture.SampleLevel(LinearSampler, float2(tc0.x, m0.y), 0);
	float4 C = Texture.SampleLevel(LinearSampler, float2(m0.x, m0.y), 0);
	float4 D = Texture.SampleLevel(LinearSampler, float2(tc3.x, m0.y), 0);
	float4 E = Texture.SampleLevel(LinearSampler, float2(m0.x, tc3.y), 0);
	float4 color =	(0.5 * (A + B) * w0.x + A * s0.x + 0.5 * (A + B) * w3.x) * w0.y +
					(B * w0.x + C * s0.x + D * w3.x) * s0.y +
					(0.5 * (B + E) * w0.x + E * s0.x + 0.5 * (D + E) * w3.x) * w3.y;
	return color;
}


bool CheckSilhouette(uint LdsIdx, int2 ST, float3 Velocity, float CompareDepth, float VarExtensionRequest) {
	bool NoSilhouetteDetected = false;
	float CompareDepthExt;
	int2 DepthOffset = GetClosestPixelExtend(LdsIdx, CompareDepthExt, VarExtensionRequest);
	float3 VelocityExt = UnpackVelocity(VelocityBuffer[ST + DepthOffset]);
	float Threshold = 0.5*TemporalBlendControl;
	NoSilhouetteDetected = abs(length(Velocity.xy) - length(VelocityExt.xy)) < Threshold;
	// always treat boundary between geometry and empty space as silhouette
	NoSilhouetteDetected = NoSilhouetteDetected && (CompareDepth < 1.f);
	return NoSilhouetteDetected;
}

void GetVarianceBox(uint2 ST, out float3 BoxMin, out float3 BoxMax, float VarExtensionFactor) {
	//float LOD = (VarianceExtension - 1)*VarExtensionFactor;
	float LOD = VarExtensionFactor;
	float4 TempVariance0 = VarianceMap0.SampleLevel(LinearSampler, STtoUV(ST), LOD);
	float2 TempVariance1 = VarianceMap1.SampleLevel(LinearSampler, STtoUV(ST), LOD);
	float3 mu = TempVariance0.xyz;
	float3 m2 = float3(TempVariance0.w, TempVariance1.xy);
	float3 var = max(0.f, m2 - mu * mu);
	float3 sigma = sqrt(var) * 1.2f;
	BoxMin = (mu - sigma);
	BoxMax = (mu + sigma);
}

float GetShadingWeight1Tap(uint2 ST, int2 pixelJitter) {
	float2 fractJitter = JitterVector - float2(pixelJitter);
	int2 pixOffset = ST % 2;
	float2 delta = pixOffset + fractJitter - float2(1, 1);
	float dist = delta.x*delta.x + delta.y*delta.y;
	float sigma2 = 2.f*0.7f*0.7f;
	float w = exp(-dist / sigma2);
	return w;
}


float GetShadingWeightLanczos(uint2 ST, int2 pixelJitter) {
	float2 fractJitter = JitterVector - float2(pixelJitter);
	int2 pixOffset = ST % 2;
	float2 delta = pixOffset + fractJitter - float2(1, 1);
	float x = min(1.f, length(delta));
	float w = (x == 0) ? 1 : 2 * sin(PI * x)*sin(PI * x / 2) / (PI * PI * x * x);
	return w;
}

void ApplyTemporalBlend(uint2 ST, uint LdsIdx)
{
	// If jitter exceeds a pixel cancel pixel offset
	int2 pixelJitter = floor(JitterVector);
	int2 STP = max(0, ST - pixelJitter);
	float3 CurrentColor = LoadRGB(LdsIdx);
	float3 Velocity;
	float CompareDepth;
	float3 BoxMin, BoxMax;

    Velocity = UnpackVelocity(VelocityBuffer[ST + GetClosestPixel3x3(LdsIdx, CompareDepth)]);

    CompareDepth += Velocity.z;

    // The temporal depth is the actual depth of the pixel found at the same reprojected location.
	float TemporalDepthMax = MaxOf(PreDepth.Gather(LinearSampler, STtoUV(STP + Velocity.xy))) + 1e-3;

	// Fast-moving pixels cause motion blur and probably don't need TAA
	float SpeedFactor = saturate(1.0 - length(Velocity.xy) * RcpSpeedLimiter);

    // Fetch temporal color.  Its "confidence" weight is stored in alpha.
	float4 Temp;
	if (BicubicFiltering == 1) {
		Temp = BicubicSampling5(InTemporal, STtoUV(STP + Velocity.xy));
	}
	else {
		Temp = InTemporal.SampleLevel(LinearSampler, STtoUV(STP + Velocity.xy), 0);
	}

    float3 TemporalColor = Temp.rgb;
	float TemporalWeight = Temp.w;

	// Pixel colors are pre-multiplied by their weight to enable bilinear filtering.  Divide by weight to recover color.
	TemporalColor /= max(TemporalWeight, 1e-6);

	// Update the confidence term based on speed and disocclusion
	uint NoDisocclusion = step(CompareDepth, TemporalDepthMax);
	TemporalWeight *= SpeedFactor * NoDisocclusion;

	// No Variance Extension needed off from a fresh disocclusion
	// Slowly increase the variance extension (between 0 to VarianceExtension-1)
	float VarExtensionFactorRequest = TemporalWeight * TemporalWeight * (VarianceExtension - 1);

	// Check velocity difference in box covered the requested Variance Extension window
	bool NoSilhouetteInBox = CheckSilhouette(LdsIdx, ST, Velocity, CompareDepth, VarExtensionFactorRequest);

	// Nullify the requested variance extension if there is silhouette in the requested window
	float VarExtension = VarExtensionFactorRequest *NoSilhouetteInBox;
	GetVarianceBox(ST, BoxMin, BoxMax, VarExtension);
	
	TemporalColor = ClipColor(ColorToLum(TemporalColor), BoxMin, BoxMax, 1.f);
	TemporalColor = LumToColor(TemporalColor);

	// Blend previous color with new color based on confidence.  Confidence steadily grows with each iteration
    // until it is broken by movement such as through disocclusion, color changes, or moving beyond the resolution
    // of the velocity buffer.
	TemporalColor = ITM(lerp(TM(CurrentColor), TM(TemporalColor), (TemporalWeight)));

	// Update weight
	TemporalWeight = saturate(rcp(2.0 - TemporalWeight + AccModifier*0.1f));

    // Quantize weight to what is representable
    TemporalWeight = f16tof32(f32tof16(TemporalWeight));

    // Breaking this up into two buffers means it can be 40 bits instead of 64.
	OutTemporal[STP] = float4(TemporalColor, 1) * TemporalWeight;
	if (VisualSilhouette) {
		// visualize silhouette detection
		OutTemporal[STP] = float4((1-NoSilhouetteInBox) || (1 - NoDisocclusion), 10*(CompareDepth - Velocity.z), 100*(CompareDepth - Velocity.z), 1) * TemporalWeight;
	}
	if (VisualDisocclusion) {
		// visualize disocclusoin detection
		OutTemporal[STP] = float4(1 - NoDisocclusion, 10 * (CompareDepth - Velocity.z), 100 * (CompareDepth - Velocity.z), 1) * TemporalWeight;
	}
}

[RootSignature(Temporal_RootSig)]
[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    const uint ldsHalfPitch = kLdsPitch/2;

    // Prefetch an 16x16 tile of pixels (8x8 colors) including a 4 pixel border
	// 16x16 IDs with 4 IDs per thread = 64 threads

	for (uint i = GI; i < (kLdsPitch * kLdsRows / 4); i += 64)
    {
        uint X = (i % ldsHalfPitch)*2;
        uint Y = (i / ldsHalfPitch)*2;
        uint TopLeftIdx = X + Y * kLdsPitch;
        int2 TopLeftST = Gid.xy * uint2(8, 8) - 4 + uint2(X, Y);
        float2 ColorUV = RcpColorBufferDim * (TopLeftST + float2(1, 1));
        float2 UV = RcpBufferDim * (TopLeftST + float2(1, 1));

        float4 Depths = CurDepth.Gather(LinearSampler, UV);
        ldsDepth[TopLeftIdx + 0] = Depths.w;
        ldsDepth[TopLeftIdx + 1] = Depths.z;
        ldsDepth[TopLeftIdx + kLdsPitch] = Depths.x;
        ldsDepth[TopLeftIdx + 1 + kLdsPitch] = Depths.y;

        float4 R4 = InColor.GatherRed(LinearSampler, ColorUV);
        float4 G4 = InColor.GatherGreen(LinearSampler, ColorUV);
        float4 B4 = InColor.GatherBlue(LinearSampler, ColorUV);
        StoreRGB(TopLeftIdx, float3(R4.w, G4.w, B4.w));
        StoreRGB(TopLeftIdx + 1, float3(R4.z, G4.z, B4.z));
        StoreRGB(TopLeftIdx + kLdsPitch, float3(R4.x, G4.x, B4.x));
        StoreRGB(TopLeftIdx + 1 + kLdsPitch, float3(R4.y, G4.y, B4.y));
    }

    GroupMemoryBarrierWithGroupSync();

    uint Idx = GTid.x + GTid.y * kLdsPitch + (kLdsPitch + 1)*4;

    uint2 ST = DTid.xy;
	ApplyTemporalBlend(ST, Idx);
}
