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

#include "pch.h"
#include "TemporalEffects.h"
#include "BufferManager.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "SystemTime.h"
#include "PostEffects.h"

#include "CompiledShaders/TemporalBlendCS.h"
#include "CompiledShaders/BoundNeighborhoodCS.h"
#include "CompiledShaders/ResolveTAACS.h"
#include "CompiledShaders/SharpenTAACS.h"
#include "CompiledShaders/VarianceMapCS.h"

using namespace Graphics;
using namespace Math;
using namespace TemporalEffects;

namespace TemporalEffects
{
    BoolVar SilhouetteVis("Graphics/AA/TAA/Silhouette Visual in Motion", false);
    BoolVar VelocityVis("Graphics/AA/TAA/Disocclusion Visual", false);
    BoolVar EnableTAA("Graphics/AA/TAA/Enable", false);
    ExpVar NumSamples("Graphics/AA/TAA/NumSamples", 3.f, 3.f, 4.f, 1.f);
    NumVar Sharpness("Graphics/AA/TAA/Sharpness", 0.0f, 0.0f, 1.0f, 0.25f);
    NumVar SilhouetteThreshold("Graphics/AA/TAA/Silhouette Threshold Factor", 1.0f, 0.0f, 3.0f, 0.1f);
    NumVar VarianceExtension("Graphics/AA/TAA/Variance Extension", 3.0f, 1.0f, 40.0f, 0.5f);
    ExpVar TemporalSpeedLimit("Graphics/AA/TAA/Speed Limit", 32.0f, 1.0f, 1024.0f, 1.0f);
    BoolVar TriggerReset("Graphics/AA/TAA/Reset", false);
    BoolVar TriggerBicubicFiltering("Graphics/AA/TAA/Bicubic Filtering", true);
    BoolVar TriggerBlurNoise("Graphics/AA/TAA/Blue Noise", false);
    NumVar TemporalAccModifier("Graphics/AA/TAA/Temporal Acc Modifier", 0.0f, 0.0f, 1.0f, 0.05f);

    RootSignature s_RootSignature;

    ComputePSO s_TemporalBlendCS;
    ComputePSO s_BoundNeighborhoodCS;
    ComputePSO s_SharpenTAACS;
    ComputePSO s_ResolveTAACS;
    ComputePSO s_VarianceMapCS;

    uint32_t s_FrameIndex = 0;
    uint32_t s_FrameIndexMod2 = 0;
    float s_JitterX = 0.5f;
    float s_JitterY = 0.5f;
    float s_JitterDeltaX = 0.0f;
    float s_JitterDeltaY = 0.0f;

    void ComputeVarianceMap(CommandContext& BaseContext);
    void ApplyTemporalAA(ComputeContext& Context);
    void SharpenImage(ComputeContext& Context, ColorBuffer& TemporalColor);
}

void TemporalEffects::Initialize(void)
{
    s_RootSignature.Reset(4, 2);
    s_RootSignature[0].InitAsConstants(0, 4);
    s_RootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
    s_RootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 10);
    s_RootSignature[3].InitAsConstantBuffer(1);
    s_RootSignature.InitStaticSampler(0, SamplerLinearBorderDesc);
    s_RootSignature.InitStaticSampler(1, SamplerPointBorderDesc);
    s_RootSignature.Finalize(L"Temporal RS");

#define CreatePSO( ObjName, ShaderByteCode ) \
    ObjName.SetRootSignature(s_RootSignature); \
    ObjName.SetComputeShader(ShaderByteCode, sizeof(ShaderByteCode) ); \
    ObjName.Finalize();

    CreatePSO(s_TemporalBlendCS, g_pTemporalBlendCS);
    CreatePSO(s_BoundNeighborhoodCS, g_pBoundNeighborhoodCS);
    CreatePSO(s_SharpenTAACS, g_pSharpenTAACS);
    CreatePSO(s_ResolveTAACS, g_pResolveTAACS);
    CreatePSO(s_VarianceMapCS, g_pVarianceMapCS);

#undef CreatePSO
}

void TemporalEffects::Shutdown(void)
{
}

void TemporalEffects::Update(uint64_t FrameIndex)
{
    s_FrameIndex = (uint32_t) FrameIndex;
    s_FrameIndexMod2 = s_FrameIndex % 2;

    if (EnableTAA)
    {
        const float* OffsetHalton;
        const float* OffsetBlueNoise;

        //const float* OffsetHalton = Halton23_64[s_FrameIndex % 64];
        //const float* OffsetBlueNoise = BlueNoise_64[s_FrameIndex % 64];


        if (NumSamples == 64) {
            static const float BlueNoise_64[64][2] = {
                { 0.593750, 0.718750 },
            { 0.000000,1.343750 },
            { 1.281250,0.968750 },
            { 1.281250,1.500000 },
            { 0.531250,1.968750 },
            { 0.812500,1.468750 },
            { 1.062500,0.562500 },
            { 0.937500,1.875000 },
            { 1.593750,1.281250 },
            { 0.906250,1.093750 },
            { 1.781250,1.656250 },
            { 0.218750,1.812500 },
            { 1.093750,0.156250 },
            { 1.406250,0.406250 },
            { 1.562500,0.125000 },
            { 0.312500,0.656250 },
            { 1.750000,1.062500 },
            { 1.656250,0.593750 },
            { 0.250000,0.093750 },
            { 1.812500,0.343750 },
            { 1.843750,1.968750 },
            { 0.406250,1.000000 },
            { 0.843750,0.781250 },
            { 1.343750,1.250000 },
            { 1.531250,1.593750 },
            { 0.218750,1.218750 },
            { 0.656250,0.968750 },
            { 0.718750,1.250000 },
            { 0.500000,0.468750 },
            { 0.187500,1.562500 },
            { 0.718750,1.750000 },
            { 1.968750,1.000000 },
            { 1.593750,1.875000 },
            { 1.125000,1.281250 },
            { 1.875000,0.593750 },
            { 1.468750,0.875000 },
            { 0.906250,0.406250 },
            { 0.000000,1.656250 },
            { 1.156250,1.968750 },
            { 0.593750,1.406250 },
            { 0.125000,0.843750 },
            { 0.656250,0.156250 },
            { 1.343750,0.125000 },
            { 0.250000,0.312500 },
            { 1.437500,0.625000 },
            { 0.500000,1.750000 },
            { 0.875000,0.187500 },
            { 1.156250,1.656250 },
            { 0.031250,0.468750 },
            { 1.718750,1.437500 },
            { 0.468750,0.281250 },
            { 1.781250,0.875000 },
            { 1.468750,1.093750 },
            { 0.031250,0.250000 },
            { 0.750000,0.562500 },
            { 1.375000,1.906250 },
            { 1.031250,0.781250 },
            { 0.031250,1.906250 },
            { 1.125000,0.375000 },
            { 0.437500,1.187500 },
            { 0.687500,0.343750 },
            { 0.312500,1.406250 },
            { 1.093750,1.062500 },
            { 1.781250,0.156250 }
            };

            OffsetHalton = BlueNoise_64[s_FrameIndex % 64];
            OffsetBlueNoise = BlueNoise_64[s_FrameIndex % 64];
        }
        else if (NumSamples == 16) {
            static const float Halton23_16[16][2] = {
                { 0.000000, 0.000000 },
            { 0.500000,0.333333 },
            { 0.250000,0.666667 },
            { 0.750000,0.111111 },
            { 0.125000,0.444444 },
            { 0.625000,0.777778 },
            { 0.375000,0.222222 },
            { 0.875000,0.555556 },
            { 0.062500,0.888889 },
            { 0.562500,0.037037 },
            { 0.312500,0.370370 },
            { 0.812500,0.703704 },
            { 0.187500,0.148148 },
            { 0.687500,0.481481 },
            { 0.437500,0.814815 },
            { 0.937500,0.259259 },
            };

            static const float BlueNoise_16[16][2] = {
            { 1.500000,0.593750 },
            { 1.218750,1.375000 },
            { 1.687500,1.906250 },
            { 0.375000,0.843750 },
            { 1.125000,1.875000 },
            { 0.718750,1.656250 },
            { 1.937500,0.718750 },
            { 0.656250,0.125000 },
            { 0.906250,0.937500 },
            { 1.656250,1.437500 },
            { 0.500000,1.281250 },
            { 0.218750,0.062500 },
            { 1.843750,0.312500 },
            { 1.093750,0.562500 },
            { 0.062500,1.218750 },
            { 0.281250,1.656250 },
            };

            //static const float BlueNoise_16[16][2] = {
            //{ 1.718750,1.031250 },
            //{ 0.531250,0.187500 },
            //{ 0.750000,1.281250 },
            //{ 1.468750,1.968750 },
            //{ 1.593750,0.406250 },
            //{ 0.625000,0.718750 },
            //{ 1.812500,1.531250 },
            //{ 0.437500,1.531250 },
            //{ 1.375000,0.750000 },
            //{ 0.937500,1.687500 },
            //{ 0.218750,0.500000 },
            //{ 1.312500,1.281250 },
            //{ 0.156250,1.843750 },
            //{ 0.968750,0.406250 },
            //{ 0.187500,1.062500 },
            //{ 0.968750,0.968750 }
            //};


            OffsetHalton = Halton23_16[s_FrameIndex % 16];
            OffsetBlueNoise = BlueNoise_16[s_FrameIndex % 16];
        }
        else {
            static const float Halton23_8[8][2] = {
            { 0.0f / 8.0f, 0.0f / 9.0f },{ 4.0f / 8.0f, 3.0f / 9.0f },
            { 2.0f / 8.0f, 6.0f / 9.0f },{ 6.0f / 8.0f, 1.0f / 9.0f },
            { 1.0f / 8.0f, 4.0f / 9.0f },{ 5.0f / 8.0f, 7.0f / 9.0f },
            { 3.0f / 8.0f, 2.0f / 9.0f },{ 7.0f / 8.0f, 5.0f / 9.0f }
            };

            static const float BlueNoise_8[8][2] = {
            { 1.906250,1.062500 },
            { 0.968750,1.656250 },
            { 1.218750,0.906250 },
            { 1.437500,0.281250 },
            { 0.562500,0.968750 },
            { 0.343750,1.593750 },
            { 0.093750,0.343750 },
            { 1.656250,1.656250 }
            };

            //static const float BlueNoise_8[8][2] = {
            //{ 1.031250,1.625000 },
            //{ 1.687500,0.625000 },
            //{ 0.375000,0.500000 },
            //{ 0.281250,1.843750 },
            //{ 1.656250,1.281250 },
            //{ 1.031250,0.281250 },
            //{ 0.343750,1.156250 },
            //{ 0.968750,0.968750 }
            //};

            OffsetHalton = Halton23_8[s_FrameIndex % 8];
            OffsetBlueNoise = BlueNoise_8[s_FrameIndex % 8];
        }


        if (TriggerBlurNoise) {
            s_JitterDeltaX = s_JitterX - OffsetBlueNoise[0] * 0.5f;
            s_JitterDeltaY = s_JitterY - OffsetBlueNoise[1] * 0.5f;
            s_JitterX = OffsetBlueNoise[0] * 0.5f;
            s_JitterY = OffsetBlueNoise[1] * 0.5f;
        }
        else {
            s_JitterDeltaX = s_JitterX - OffsetHalton[0];
            s_JitterDeltaY = s_JitterY - OffsetHalton[1];
            s_JitterX = OffsetHalton[0];
            s_JitterY = OffsetHalton[1];
        }
    }
    else
    {
        s_JitterDeltaX = s_JitterX - 0.5f;
        s_JitterDeltaY = s_JitterY - 0.5f;
        s_JitterX = 0.5f;
        s_JitterY = 0.5f;
    }
}

uint32_t TemporalEffects::GetFrameIndexMod2(void)
{
    return s_FrameIndexMod2;
}

void TemporalEffects::GetJitterOffset(float& JitterX, float& JitterY)
{
    JitterX = s_JitterX;
    JitterY = s_JitterY;
}

void TemporalEffects::ClearHistory(CommandContext& Context)
{
    GraphicsContext& gfxContext = Context.GetGraphicsContext();

    if (EnableTAA)
    {
        gfxContext.TransitionResource(g_TemporalColor[0], D3D12_RESOURCE_STATE_RENDER_TARGET);
        gfxContext.TransitionResource(g_TemporalColor[1], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
        gfxContext.ClearColor(g_TemporalColor[0]);
        gfxContext.ClearColor(g_TemporalColor[1]);
    }
}

void TemporalEffects::ResolveImage(CommandContext& BaseContext)
{
    ScopedTimer _prof(L"Temporal Resolve", BaseContext);

    ComputeContext& Context = BaseContext.GetComputeContext();

    static bool s_EnableTAA = false;

    if (EnableTAA != s_EnableTAA || TriggerReset)
    {
        ClearHistory(Context);
        s_EnableTAA = EnableTAA;
        TriggerReset = false;
    }

    uint32_t Src = s_FrameIndexMod2;
    uint32_t Dst = Src ^ 1;

    if (EnableTAA)
    {
        ComputeVarianceMap(BaseContext);
        ApplyTemporalAA(Context);
        SharpenImage(Context, g_TemporalColor[Dst]);
    }
}

void TemporalEffects::ComputeVarianceMap(CommandContext& BaseContext)
{
    ComputeContext& Context = BaseContext.GetComputeContext();
    Context.SetRootSignature(s_RootSignature);
    Context.SetPipelineState(s_VarianceMapCS);

    Context.SetConstants(0, 1.0f / g_pSceneColorBuffer->GetWidth(), 1.0f / g_pSceneColorBuffer->GetHeight());
    Context.TransitionResource(*g_pSceneColorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_VarianceMap[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.TransitionResource(g_VarianceMap[1], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.SetDynamicDescriptor(1, 0, g_pSceneColorBuffer->GetSRV());
    Context.SetDynamicDescriptor(2, 0, g_VarianceMap[0].GetUAV());
    Context.SetDynamicDescriptor(2, 1, g_VarianceMap[1].GetUAV());

    // generate variance map of 3x3 window
    Context.Dispatch2D(g_pSceneColorBuffer->GetWidth(), g_pSceneColorBuffer->GetHeight());

    // generate mipmap for the variance maps
    g_VarianceMap[0].GenerateMipMaps(BaseContext);
    g_VarianceMap[1].GenerateMipMaps(BaseContext);

}

void TemporalEffects::ApplyTemporalAA(ComputeContext& Context)
{
    ScopedTimer _prof(L"Resolve Image", Context);

    uint32_t Src = s_FrameIndexMod2;
    uint32_t Dst = Src ^ 1;

    Context.SetRootSignature(s_RootSignature);
    Context.SetPipelineState(s_TemporalBlendCS);

    __declspec(align(16)) struct ConstantBuffer
    {
        float RcpBufferDim[2];
        float RcpInColorBufferDim[2];
        float _Pad[2];
        float SilhouetteThresholdFactor;
        float RcpSeedLimiter;
        float CombinedJitterDelta[2];
        float CombinedJitterVector[2];
        uint32_t  BicubicFiltering;
        uint32_t  VisualSilhouette;
        uint32_t  FrameOffset;
        uint32_t  VisualDisocclusion;
        float VarianceExtensionFactor;
        float AccModifierFactor;
    };

    ConstantBuffer cbv = {
       1.0f / g_VelocityBuffer.GetWidth(), 1.0f / g_VelocityBuffer.GetHeight(),
       1.0f / g_pSceneColorBuffer->GetWidth(), 1.0f / g_pSceneColorBuffer->GetHeight(),
       0.0f, 0.0f, //pad
       (float) SilhouetteThreshold, 1.0f / TemporalSpeedLimit,
       s_JitterDeltaX, s_JitterDeltaY,
       s_JitterX, s_JitterY,
       (uint32_t) (TriggerBicubicFiltering ? 1 : 0),
       (uint32_t) (SilhouetteVis ? 1 : 0),
       (uint32_t) s_FrameIndexMod2,
       (uint32_t) (VelocityVis ? 1 : 0),
       (float) VarianceExtension,
       (float) TemporalAccModifier,
    };

    Context.SetDynamicConstantBufferView(3, sizeof(cbv), &cbv);

    Context.TransitionResource(g_VelocityBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(*g_pSceneColorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_TemporalColor[Src], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_TemporalColor[Dst], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.TransitionResource(g_LinearDepth[Src], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_LinearDepth[Dst], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_VarianceMap[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_VarianceMap[1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.SetDynamicDescriptor(1, 0, g_VelocityBuffer.GetSRV());
    Context.SetDynamicDescriptor(1, 1, g_pSceneColorBuffer->GetSRV());
    Context.SetDynamicDescriptor(1, 2, g_TemporalColor[Src].GetSRV());
    Context.SetDynamicDescriptor(1, 3, g_LinearDepth[Src].GetSRV());
    Context.SetDynamicDescriptor(1, 4, g_LinearDepth[Dst].GetSRV());
    Context.SetDynamicDescriptor(1, 5, g_VarianceMap[0].GetSRV());
    Context.SetDynamicDescriptor(1, 6, g_VarianceMap[1].GetSRV());
    Context.SetDynamicDescriptor(2, 0, g_TemporalColor[Dst].GetUAV());

    Context.Dispatch2D(g_pSceneColorBuffer->GetWidth(), g_pSceneColorBuffer->GetHeight(), 8, 8);
}

void TemporalEffects::SharpenImage(ComputeContext& Context, ColorBuffer& TemporalColor)
{
    ScopedTimer _prof(L"Sharpen or Copy Image", Context);

    Context.TransitionResource(*g_pSceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.TransitionResource(TemporalColor, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    Context.SetPipelineState(Sharpness >= 0.001f ? s_SharpenTAACS : s_ResolveTAACS);
    Context.SetConstants(0, 1.0f + Sharpness, 0.25f * Sharpness);
    Context.SetDynamicDescriptor(1, 0, TemporalColor.GetSRV());
    Context.SetDynamicDescriptor(2, 0, g_pSceneColorBuffer->GetUAV());
    Context.Dispatch2D(g_pSceneColorBuffer->GetWidth(), g_pSceneColorBuffer->GetHeight());
}
