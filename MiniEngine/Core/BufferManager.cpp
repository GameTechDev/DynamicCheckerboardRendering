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
#include "BufferManager.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "EsramAllocator.h"
#include "TemporalEffects.h"

namespace Graphics
{
    DepthBuffer g_SceneDepthBuffers[NUM_SCENE_BUFFERS];
    ColorBuffer g_SceneColorBuffers[NUM_SCENE_BUFFERS];
    ColorBuffer g_SceneColorBuffers2x[NUM_SCENE_BUFFERS];
    DepthBuffer g_SceneDepthBuffers2x[NUM_SCENE_BUFFERS];
    ColorBuffer g_SceneColorBuffers4x[NUM_SCENE_BUFFERS];
    DepthBuffer g_SceneDepthBuffers4x[NUM_SCENE_BUFFERS];
    ColorBuffer g_SceneColorBuffers8x[NUM_SCENE_BUFFERS];
    DepthBuffer g_SceneDepthBuffers8x[NUM_SCENE_BUFFERS];
    ColorBuffer g_SceneColorBuffersCB2x[NUM_CHECKER_BUFFERS];
    DepthBuffer g_SceneDepthBuffersCB2x[NUM_CHECKER_BUFFERS];
    
    ColorBuffer *g_pCheckerboardColors[2];
    DepthBuffer *g_pCheckerboardDepths[2];

    ColorBuffer *g_pSceneColorBuffer;
    DepthBuffer *g_pSceneDepthBuffer;

    ColorBuffer *g_pMsaaColor;
    DepthBuffer *g_pMsaaDepth;

    ColorBuffer g_PostEffectsBuffer;
    ColorBuffer g_VelocityBuffer;
    ColorBuffer g_OverlayBuffer;
    ColorBuffer g_HorizontalBuffer;

    ShadowBuffer g_ShadowBuffer;

    ColorBuffer g_SSAOFullScreen(Color(1.0f, 1.0f, 1.0f));
    ColorBuffer g_LinearDepth[2];
    ColorBuffer g_MinMaxDepth8;
    ColorBuffer g_MinMaxDepth16;
    ColorBuffer g_MinMaxDepth32;
    ColorBuffer g_DepthDownsize1;
    ColorBuffer g_DepthDownsize2;
    ColorBuffer g_DepthDownsize3;
    ColorBuffer g_DepthDownsize4;
    ColorBuffer g_DepthTiled1;
    ColorBuffer g_DepthTiled2;
    ColorBuffer g_DepthTiled3;
    ColorBuffer g_DepthTiled4;
    ColorBuffer g_AOMerged1;
    ColorBuffer g_AOMerged2;
    ColorBuffer g_AOMerged3;
    ColorBuffer g_AOMerged4;
    ColorBuffer g_AOSmooth1;
    ColorBuffer g_AOSmooth2;
    ColorBuffer g_AOSmooth3;
    ColorBuffer g_AOHighQuality1;
    ColorBuffer g_AOHighQuality2;
    ColorBuffer g_AOHighQuality3;
    ColorBuffer g_AOHighQuality4;

    ColorBuffer g_DoFTileClass[2];
    ColorBuffer g_DoFPresortBuffer;
    ColorBuffer g_DoFPrefilter;
    ColorBuffer g_DoFBlurColor[2];
    ColorBuffer g_DoFBlurAlpha[2];
    StructuredBuffer g_DoFWorkQueue;
    StructuredBuffer g_DoFFastQueue;
    StructuredBuffer g_DoFFixupQueue;

    ColorBuffer g_MotionPrepBuffer;
    ColorBuffer g_LumaBuffer;
    ColorBuffer g_TemporalColor[2];
    ColorBuffer g_aBloomUAV1[2];	// 640x384 (1/3)
    ColorBuffer g_aBloomUAV2[2];	// 320x192 (1/6)  
    ColorBuffer g_aBloomUAV3[2];	// 160x96  (1/12)
    ColorBuffer g_aBloomUAV4[2];	// 80x48   (1/24)
    ColorBuffer g_aBloomUAV5[2];	// 40x24   (1/48)
    ColorBuffer g_LumaLR;
    ByteAddressBuffer g_Histogram;
    ByteAddressBuffer g_FXAAWorkCounters;
    ByteAddressBuffer g_FXAAWorkQueue;
    TypedBuffer g_FXAAColorQueue(DXGI_FORMAT_R11G11B10_FLOAT);

    // For testing GenerateMipMaps()
    ColorBuffer g_GenMipsBuffer;

    // For variance map mipmapping
    ColorBuffer g_VarianceMap[2];

    // TODO: DRR - We should wrap D3D12 
    // specific functionaltiy in a class
    ID3D12Heap *g_pHeap;
    
    UINT64 SceneBufferOffsets[NUM_SCENE_BUFFERS];
    UINT64 ColorBufferOffsets[NUM_SCENE_BUFFERS];
    UINT64 DepthBufferOffsets[NUM_SCENE_BUFFERS];
    UINT64 CbrColorBufferOffsets[NUM_CHECKER_BUFFERS];
    UINT64 CbrDepthBufferOffsets[NUM_CHECKER_BUFFERS];
    
    DXGI_FORMAT DefaultHdrColorFormat = DXGI_FORMAT_R11G11B10_FLOAT;
}

void Graphics::InitializeRenderingBuffers(uint32_t bufferWidth, uint32_t bufferHeight)
{
    GraphicsContext& InitContext = GraphicsContext::Begin();

    const uint32_t bufferWidth1 = (bufferWidth + 1) / 2;
    const uint32_t bufferWidth2 = (bufferWidth + 3) / 4;
    const uint32_t bufferWidth3 = (bufferWidth + 7) / 8;
    const uint32_t bufferWidth4 = (bufferWidth + 15) / 16;
    const uint32_t bufferWidth5 = (bufferWidth + 31) / 32;
    const uint32_t bufferWidth6 = (bufferWidth + 63) / 64;
    const uint32_t bufferHeight1 = (bufferHeight + 1) / 2;
    const uint32_t bufferHeight2 = (bufferHeight + 3) / 4;
    const uint32_t bufferHeight3 = (bufferHeight + 7) / 8;
    const uint32_t bufferHeight4 = (bufferHeight + 15) / 16;
    const uint32_t bufferHeight5 = (bufferHeight + 31) / 32;
    const uint32_t bufferHeight6 = (bufferHeight + 63) / 64;

    EsramAllocator esram;

    esram.PushStack(); // Top

    // DRR - We could use separate heaps so the generic color one doesn't need 4MB MSAA alignment
    esram.PushStack(); // Heap
    {
        D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        UINT64 MaxSceneBufferSize;
        {
            D3D12_RESOURCE_DESC desc = PixelBuffer::CreateResourceDesc(bufferWidth, bufferHeight, 1, 1, DefaultHdrColorFormat, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            D3D12_RESOURCE_ALLOCATION_INFO allocInfo = g_Device->GetResourceAllocationInfo( 0, 1, &desc );
            // these all share the same heap so we can alias them - because of that they all must have the worst alignment case
            MaxSceneBufferSize = ALIGN(allocInfo.SizeInBytes, D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT);
        }

        UINT64 MaxDepthSize;
        {
            D3D12_RESOURCE_DESC desc = PixelBuffer::CreateResourceDesc(bufferWidth, bufferHeight, 1, 1, DSV_FORMAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
            desc.SampleDesc.Count = 8;

            D3D12_RESOURCE_ALLOCATION_INFO allocInfo = g_Device->GetResourceAllocationInfo( 0, 1, &desc );
            // these all share the same heap so we can alias them - because of that they all must have the worst alignment case
            MaxDepthSize = ALIGN(allocInfo.SizeInBytes, D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT);
        }

        UINT64 MaxColorSize;
        {
            D3D12_RESOURCE_DESC desc = PixelBuffer::CreateResourceDesc(bufferWidth, bufferHeight, 1, 1, DefaultHdrColorFormat, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
            desc.SampleDesc.Count = 8;

            D3D12_RESOURCE_ALLOCATION_INFO allocInfo = g_Device->GetResourceAllocationInfo( 0, 1, &desc );
            // these all share the same heap so we can alias them - because of that they all must have the worst alignment case
            MaxColorSize = ALIGN(allocInfo.SizeInBytes, D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT);
        }

        UINT64 CbColorAlignedOffset;
        {
            D3D12_RESOURCE_DESC desc = PixelBuffer::CreateResourceDesc(bufferWidth1, bufferHeight1, 1, 1, DefaultHdrColorFormat, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
            desc.SampleDesc.Count = 2;

            D3D12_RESOURCE_ALLOCATION_INFO allocInfo = g_Device->GetResourceAllocationInfo( 0, 1, &desc );
            // these all share the same heap so we can alias them - because of that they all must have the worst alignment case
            CbColorAlignedOffset = ALIGN(allocInfo.SizeInBytes, D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT);
        }

        UINT64 CbDepthAlignedOffset;
        {
            D3D12_RESOURCE_DESC desc = PixelBuffer::CreateResourceDesc(bufferWidth1, bufferHeight1, 1, 1, DSV_FORMAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
            desc.SampleDesc.Count = 2;

            D3D12_RESOURCE_ALLOCATION_INFO allocInfo = g_Device->GetResourceAllocationInfo( 0, 1, &desc );
            // these all share the same heap so we can alias them - because of that they all must have the worst alignment case
            CbDepthAlignedOffset = ALIGN(allocInfo.SizeInBytes, D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT);
        }


        UINT64 CbColorOffset = 0;
        UINT64 CbDepthOffset = 0;
        UINT64 HeapSize = 0;
        UINT CbIndex = 0;
        int i;

        for ( i = 0; i < NUM_SCENE_BUFFERS; i++ )
        {
            SceneBufferOffsets[i] = HeapSize;    HeapSize += MaxSceneBufferSize;
            ColorBufferOffsets[i] = HeapSize;    HeapSize += MaxColorSize;
            DepthBufferOffsets[i] = HeapSize;    HeapSize += MaxDepthSize;

            CbrColorBufferOffsets[i] = ColorBufferOffsets[CbIndex] + CbColorOffset; //this will fix a scene buffer overwrite, but i'd rather figure it out
            CbrDepthBufferOffsets[i] = DepthBufferOffsets[CbIndex] + CbDepthOffset;

            // cbr frames are only half the resolution
            // so we can pack 2 cbr buffers into the space normally consumed by a single color buffer
            CbColorOffset ^= CbColorAlignedOffset;
            CbDepthOffset ^= CbDepthAlignedOffset;

            CbIndex += (i & 0x1);
        }

        // cbr has two extra frames for the temporal element
        for (; i < NUM_CHECKER_BUFFERS; i++ )
        {
            CbrColorBufferOffsets[i] = ColorBufferOffsets[CbIndex] + CbColorOffset;
            CbrDepthBufferOffsets[i] = DepthBufferOffsets[CbIndex] + CbDepthOffset;

            CbColorOffset ^= CbColorAlignedOffset;
            CbDepthOffset ^= CbDepthAlignedOffset;

            CbIndex += (i & 0x1);
        }

        D3D12_HEAP_DESC desc = {};
        {
            desc.SizeInBytes = HeapSize;
            desc.Properties = heapProps;
            desc.Alignment = D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;
            desc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;

            ASSERT_SUCCEEDED(g_Device->CreateHeap(&desc, MY_IID_PPV_ARGS(&g_pHeap)));
        }

        esram.PopStack(); // Heap
    }

    esram.PushStack(); // HDR
    {
        int i;

        for ( i = 0; i < NUM_SCENE_BUFFERS; i++ )
        {
            g_SceneColorBuffers[i].CreatePlaced(L"Scene Color Buffer " + std::to_wstring(i), bufferWidth, bufferHeight, 1, DefaultHdrColorFormat, g_pHeap, SceneBufferOffsets[i]);
            g_SceneDepthBuffers[i].CreatePlaced(L"Scene Depth Buffer " + std::to_wstring(i), bufferWidth, bufferHeight, 1, DSV_FORMAT, g_pHeap, DepthBufferOffsets[i]);

            // Setup the buffers for MSAA
            g_SceneColorBuffers2x[i].SetMsaaMode(2, 2);
            g_SceneColorBuffers2x[i].CreatePlaced(L"Offscreen Color Buffer 2xMSAA " + std::to_wstring(i), bufferWidth, bufferHeight, 1, DefaultHdrColorFormat, g_pHeap, ColorBufferOffsets[i]);
            g_SceneDepthBuffers2x[i].CreatePlaced(L"Offscreen Depth Buffer 2xMSAA " + std::to_wstring(i), bufferWidth, bufferHeight, 2, DSV_FORMAT, g_pHeap, DepthBufferOffsets[i]);

            g_SceneColorBuffers4x[i].SetMsaaMode(4, 4);
            g_SceneColorBuffers4x[i].CreatePlaced(L"Offscreen Color Buffer 4xMSAA " + std::to_wstring(i), bufferWidth, bufferHeight, 1, DefaultHdrColorFormat, g_pHeap, ColorBufferOffsets[i]);
            g_SceneDepthBuffers4x[i].CreatePlaced(L"Offscreen Depth Buffer 4xMSAA " + std::to_wstring(i), bufferWidth, bufferHeight, 4, DSV_FORMAT, g_pHeap, DepthBufferOffsets[i]);

            g_SceneColorBuffers8x[i].SetMsaaMode(8, 8);
            g_SceneColorBuffers8x[i].CreatePlaced(L"Offscreen Color Buffer 8xMSAA " + std::to_wstring(i), bufferWidth, bufferHeight, 1, DefaultHdrColorFormat, g_pHeap, ColorBufferOffsets[i]);
            g_SceneDepthBuffers8x[i].CreatePlaced(L"Offscreen Depth Buffer 8xMSAA " + std::to_wstring(i), bufferWidth, bufferHeight, 8, DSV_FORMAT, g_pHeap, DepthBufferOffsets[i]);

            g_SceneColorBuffersCB2x[i].SetMsaaMode(2, 2);
            g_SceneColorBuffersCB2x[i].CreatePlaced(L"Offscreen Color Buffer 2xCheckerboard " + std::to_wstring(i), bufferWidth1, bufferHeight1, 1, DefaultHdrColorFormat, g_pHeap, CbrColorBufferOffsets[i]);
            g_SceneDepthBuffersCB2x[i].CreatePlaced(L"Offscreen Depth Buffer 2xCheckerboard " + std::to_wstring(i), bufferWidth1, bufferHeight1, 2, DSV_FORMAT, g_pHeap, CbrDepthBufferOffsets[i]);
        }

        // cbr has two extra frames for the temporal element
        for (; i < NUM_CHECKER_BUFFERS; i++ )
        {
            g_SceneColorBuffersCB2x[i].SetMsaaMode(2, 2);
            g_SceneColorBuffersCB2x[i].CreatePlaced(L"Offscreen Color Buffer 2xCheckerboard " + std::to_wstring(i), bufferWidth1, bufferHeight1, 1, DefaultHdrColorFormat, g_pHeap, CbrColorBufferOffsets[i]);
            g_SceneDepthBuffersCB2x[i].CreatePlaced(L"Offscreen Depth Buffer 2xCheckerboard " + std::to_wstring(i), bufferWidth1, bufferHeight1, 2, DSV_FORMAT, g_pHeap, CbrDepthBufferOffsets[i]);
        }

        // DRR - can stay full res (see notes.md)
        g_VelocityBuffer.Create(L"Motion Vectors", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);

        // DRR - can stay full res (see notes.md)
        g_PostEffectsBuffer.Create(L"Post Effects Buffer", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R32_UINT);

        esram.PopStack();
    }

    g_pSceneColorBuffer = &g_SceneColorBuffers[0];
    g_pSceneDepthBuffer = &g_SceneDepthBuffers[0];

    g_pCheckerboardColors[0] = NULL;
    g_pCheckerboardColors[1] = NULL;
    g_pCheckerboardDepths[0] = NULL;
    g_pCheckerboardDepths[1] = NULL;
    g_pMsaaColor = NULL;
    g_pMsaaDepth = NULL;


    esram.PushStack(); // Linear Depth
    {
        g_LinearDepth[0].Create(L"Linear Depth 0", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R16_UNORM);
        g_LinearDepth[1].Create(L"Linear Depth 1", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R16_UNORM);
        g_MinMaxDepth8.Create(L"MinMaxDepth 8x8", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R32_UINT, esram);
        g_MinMaxDepth16.Create(L"MinMaxDepth 16x16", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R32_UINT, esram);
        g_MinMaxDepth32.Create(L"MinMaxDepth 32x32", bufferWidth5, bufferHeight5, 1, DXGI_FORMAT_R32_UINT, esram);
        esram.PopStack(); // Linear Depth
    }


    esram.PushStack();	// Begin generating SSAO
    {
        g_SSAOFullScreen.Create(L"SSAO Full Res", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R8_UNORM);
        g_DepthDownsize1.Create(L"Depth Down-Sized 1", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R32_FLOAT, esram);
        g_DepthDownsize2.Create(L"Depth Down-Sized 2", bufferWidth2, bufferHeight2, 1, DXGI_FORMAT_R32_FLOAT, esram);
        g_DepthDownsize3.Create(L"Depth Down-Sized 3", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R32_FLOAT, esram);
        g_DepthDownsize4.Create(L"Depth Down-Sized 4", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R32_FLOAT, esram);
        g_DepthTiled1.CreateArray(L"Depth De-Interleaved 1", bufferWidth3, bufferHeight3, 16, DXGI_FORMAT_R16_FLOAT, esram);
        g_DepthTiled2.CreateArray(L"Depth De-Interleaved 2", bufferWidth4, bufferHeight4, 16, DXGI_FORMAT_R16_FLOAT, esram);
        g_DepthTiled3.CreateArray(L"Depth De-Interleaved 3", bufferWidth5, bufferHeight5, 16, DXGI_FORMAT_R16_FLOAT, esram);
        g_DepthTiled4.CreateArray(L"Depth De-Interleaved 4", bufferWidth6, bufferHeight6, 16, DXGI_FORMAT_R16_FLOAT, esram);
        g_AOMerged1.Create(L"AO Re-Interleaved 1", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R8_UNORM, esram);
        g_AOMerged2.Create(L"AO Re-Interleaved 2", bufferWidth2, bufferHeight2, 1, DXGI_FORMAT_R8_UNORM, esram);
        g_AOMerged3.Create(L"AO Re-Interleaved 3", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R8_UNORM, esram);
        g_AOMerged4.Create(L"AO Re-Interleaved 4", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R8_UNORM, esram);
        g_AOSmooth1.Create(L"AO Smoothed 1", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R8_UNORM, esram);
        g_AOSmooth2.Create(L"AO Smoothed 2", bufferWidth2, bufferHeight2, 1, DXGI_FORMAT_R8_UNORM, esram);
        g_AOSmooth3.Create(L"AO Smoothed 3", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R8_UNORM, esram);
        g_AOHighQuality1.Create(L"AO High Quality 1", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R8_UNORM, esram);
        g_AOHighQuality2.Create(L"AO High Quality 2", bufferWidth2, bufferHeight2, 1, DXGI_FORMAT_R8_UNORM, esram);
        g_AOHighQuality3.Create(L"AO High Quality 3", bufferWidth3, bufferHeight3, 1, DXGI_FORMAT_R8_UNORM, esram);
        g_AOHighQuality4.Create(L"AO High Quality 4", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R8_UNORM, esram);
        esram.PopStack();	// End generating SSAO
    }

    esram.PushStack();	// shadow
        g_ShadowBuffer.Create(L"Shadow Map", 8192, 8192, esram);
    esram.PopStack();	// shadow

    esram.PushStack();	// Begin depth of field
    {
        g_DoFTileClass[0].Create(L"DoF Tile Classification Buffer 0", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R11G11B10_FLOAT);
        g_DoFTileClass[1].Create(L"DoF Tile Classification Buffer 1", bufferWidth4, bufferHeight4, 1, DXGI_FORMAT_R11G11B10_FLOAT);

        g_DoFPresortBuffer.Create(L"DoF Presort Buffer", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R11G11B10_FLOAT, esram);
        g_DoFPrefilter.Create(L"DoF PreFilter Buffer", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R11G11B10_FLOAT, esram);
        g_DoFBlurColor[0].Create(L"DoF Blur Color", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R11G11B10_FLOAT, esram);
        g_DoFBlurColor[1].Create(L"DoF Blur Color", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R11G11B10_FLOAT, esram);
        g_DoFBlurAlpha[0].Create(L"DoF FG Alpha", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R8_UNORM, esram);
        g_DoFBlurAlpha[1].Create(L"DoF FG Alpha", bufferWidth1, bufferHeight1, 1, DXGI_FORMAT_R8_UNORM, esram);
        g_DoFWorkQueue.Create(L"DoF Work Queue", bufferWidth4 * bufferHeight4, 4, esram);
        g_DoFFastQueue.Create(L"DoF Fast Queue", bufferWidth4 * bufferHeight4, 4, esram);
        g_DoFFixupQueue.Create(L"DoF Fixup Queue", bufferWidth4 * bufferHeight4, 4, esram);
        esram.PopStack();	// End depth of field
    }

    
    esram.PushStack();	// Variance map creation
    {
        g_VarianceMap[0].Create(L"Variance Map 0", bufferWidth, bufferHeight, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
        g_VarianceMap[1].Create(L"Variance Map 1", bufferWidth, bufferHeight, 0, DXGI_FORMAT_R16G16_FLOAT);

        g_TemporalColor[0].Create(L"Temporal Color 0", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
        g_TemporalColor[1].Create(L"Temporal Color 1", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
        TemporalEffects::ClearHistory(InitContext);
        esram.PopStack();	// variance
    }

    esram.PushStack();	// Begin motion blur
        g_MotionPrepBuffer.Create(L"Motion Blur Prep", bufferWidth1, bufferHeight1, 1, HDR_MOTION_FORMAT, esram);
    esram.PopStack();	// End motion blur

    esram.PushStack();	// Begin post processing
    {
        // This is useful for storing per-pixel weights such as motion strength or pixel luminance
        g_LumaBuffer.Create(L"Luminance", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R8_UNORM, esram);
        g_Histogram.Create(L"Histogram", 256, 4, esram);

        // Divisible by 128 so that after dividing by 16, we still have multiples of 8x8 tiles.  The bloom
        // dimensions must be at least 1/4 native resolution to avoid undersampling.
        //uint32_t kBloomWidth = bufferWidth > 2560 ? Math::AlignUp(bufferWidth / 4, 128) : 640;
        //uint32_t kBloomHeight = bufferHeight > 1440 ? Math::AlignUp(bufferHeight / 4, 128) : 384;
        uint32_t kBloomWidth = bufferWidth > 2560 ? 1280 : 640;
        uint32_t kBloomHeight = bufferHeight > 1440 ? 768 : 384;

        esram.PushStack();	// Begin bloom and tone mapping
            g_LumaLR.Create(L"Luma Buffer", kBloomWidth, kBloomHeight, 1, DXGI_FORMAT_R8_UINT, esram);
            g_aBloomUAV1[0].Create(L"Bloom Buffer 1a", kBloomWidth, kBloomHeight, 1, DefaultHdrColorFormat, esram);
            g_aBloomUAV1[1].Create(L"Bloom Buffer 1b", kBloomWidth, kBloomHeight, 1, DefaultHdrColorFormat, esram);
            g_aBloomUAV2[0].Create(L"Bloom Buffer 2a", kBloomWidth / 2, kBloomHeight / 2, 1, DefaultHdrColorFormat, esram);
            g_aBloomUAV2[1].Create(L"Bloom Buffer 2b", kBloomWidth / 2, kBloomHeight / 2, 1, DefaultHdrColorFormat, esram);
            g_aBloomUAV3[0].Create(L"Bloom Buffer 3a", kBloomWidth / 4, kBloomHeight / 4, 1, DefaultHdrColorFormat, esram);
            g_aBloomUAV3[1].Create(L"Bloom Buffer 3b", kBloomWidth / 4, kBloomHeight / 4, 1, DefaultHdrColorFormat, esram);
            g_aBloomUAV4[0].Create(L"Bloom Buffer 4a", kBloomWidth / 8, kBloomHeight / 8, 1, DefaultHdrColorFormat, esram);
            g_aBloomUAV4[1].Create(L"Bloom Buffer 4b", kBloomWidth / 8, kBloomHeight / 8, 1, DefaultHdrColorFormat, esram);
            g_aBloomUAV5[0].Create(L"Bloom Buffer 5a", kBloomWidth / 16, kBloomHeight / 16, 1, DefaultHdrColorFormat, esram);
            g_aBloomUAV5[1].Create(L"Bloom Buffer 5b", kBloomWidth / 16, kBloomHeight / 16, 1, DefaultHdrColorFormat, esram);
        esram.PopStack();	// End tone mapping

        esram.PushStack();	// Begin antialiasing
            const uint32_t kFXAAWorkSize = bufferWidth * bufferHeight / 4 + 128;
            g_FXAAWorkQueue.Create(L"FXAA Work Queue", kFXAAWorkSize, sizeof(uint32_t), esram);
            g_FXAAColorQueue.Create(L"FXAA Color Queue", kFXAAWorkSize, sizeof(uint32_t), esram);
            g_FXAAWorkCounters.Create(L"FXAA Work Counters", 2, sizeof(uint32_t));
            InitContext.ClearUAV(g_FXAAWorkCounters);
        esram.PopStack();	// End antialiasing

        esram.PushStack(); // GenerateMipMaps() test
            g_GenMipsBuffer.Create(L"GenMips", bufferWidth, bufferHeight, 0, DXGI_FORMAT_R11G11B10_FLOAT, esram);
        esram.PopStack();

        g_OverlayBuffer.Create(L"UI Overlay", g_DisplayWidth, g_DisplayHeight, 1, DXGI_FORMAT_R8G8B8A8_UNORM, esram);
        g_HorizontalBuffer.Create(L"Bicubic Intermediate", g_DisplayWidth, bufferHeight, 1, DefaultHdrColorFormat, esram);

        esram.PopStack(); // end post processing
    }

    esram.PopStack(); // End final image

    InitContext.Finish();
}

void Graphics::ResizeDisplayDependentBuffers(uint32_t NativeWidth, uint32_t NativeHeight)
{
    g_OverlayBuffer.Create(L"UI Overlay", g_DisplayWidth, g_DisplayHeight, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
    g_HorizontalBuffer.Create(L"Bicubic Intermediate", g_DisplayWidth, NativeHeight, 1, DefaultHdrColorFormat);
}

void Graphics::DestroyRenderingBuffers()
{
    g_pHeap->Release();

    int i = 0;

    for (i = 0; i < NUM_SCENE_BUFFERS; i++)
    {
        g_SceneDepthBuffers[i].Destroy();
        g_SceneColorBuffers[i].Destroy();

        g_SceneColorBuffers2x[i].Destroy();
        g_SceneDepthBuffers2x[i].Destroy();
        g_SceneColorBuffers4x[i].Destroy();
        g_SceneDepthBuffers4x[i].Destroy();
        g_SceneColorBuffers8x[i].Destroy();
        g_SceneDepthBuffers8x[i].Destroy();

        g_SceneColorBuffersCB2x[i].Destroy();
        g_SceneDepthBuffersCB2x[i].Destroy();
    }

    for (; i < NUM_CHECKER_BUFFERS; i++)
    {
        g_SceneColorBuffersCB2x[i].Destroy();
        g_SceneDepthBuffersCB2x[i].Destroy();
    }

    g_VelocityBuffer.Destroy();
    g_OverlayBuffer.Destroy();
    g_HorizontalBuffer.Destroy();
    g_PostEffectsBuffer.Destroy();

    g_ShadowBuffer.Destroy();

    g_SSAOFullScreen.Destroy();
    g_LinearDepth[0].Destroy();
    g_LinearDepth[1].Destroy();
    g_MinMaxDepth8.Destroy();
    g_MinMaxDepth16.Destroy();
    g_MinMaxDepth32.Destroy();
    g_DepthDownsize1.Destroy();
    g_DepthDownsize2.Destroy();
    g_DepthDownsize3.Destroy();
    g_DepthDownsize4.Destroy();
    g_DepthTiled1.Destroy();
    g_DepthTiled2.Destroy();
    g_DepthTiled3.Destroy();
    g_DepthTiled4.Destroy();
    g_AOMerged1.Destroy();
    g_AOMerged2.Destroy();
    g_AOMerged3.Destroy();
    g_AOMerged4.Destroy();
    g_AOSmooth1.Destroy();
    g_AOSmooth2.Destroy();
    g_AOSmooth3.Destroy();
    g_AOHighQuality1.Destroy();
    g_AOHighQuality2.Destroy();
    g_AOHighQuality3.Destroy();
    g_AOHighQuality4.Destroy();

    g_DoFTileClass[0].Destroy();
    g_DoFTileClass[1].Destroy();
    g_DoFPresortBuffer.Destroy();
    g_DoFPrefilter.Destroy();
    g_DoFBlurColor[0].Destroy();
    g_DoFBlurColor[1].Destroy();
    g_DoFBlurAlpha[0].Destroy();
    g_DoFBlurAlpha[1].Destroy();
    g_DoFWorkQueue.Destroy();
    g_DoFFastQueue.Destroy();
    g_DoFFixupQueue.Destroy();

    g_MotionPrepBuffer.Destroy();
    g_LumaBuffer.Destroy();
    g_VarianceMap[0].Destroy();
    g_VarianceMap[1].Destroy();
    g_TemporalColor[0].Destroy();
    g_TemporalColor[1].Destroy();
    g_aBloomUAV1[0].Destroy();
    g_aBloomUAV1[1].Destroy();
    g_aBloomUAV2[0].Destroy();
    g_aBloomUAV2[1].Destroy();
    g_aBloomUAV3[0].Destroy();
    g_aBloomUAV3[1].Destroy();
    g_aBloomUAV4[0].Destroy();
    g_aBloomUAV4[1].Destroy();
    g_aBloomUAV5[0].Destroy();
    g_aBloomUAV5[1].Destroy();
    g_LumaLR.Destroy();
    g_Histogram.Destroy();
    g_FXAAWorkCounters.Destroy();
    g_FXAAWorkQueue.Destroy();
    g_FXAAColorQueue.Destroy();

    g_GenMipsBuffer.Destroy();
}

void Graphics::RecreateCBRBuffers(int indexA, int indexB, int width, int height)
{
    g_SceneColorBuffersCB2x[indexA].RecreatePlaced( width, height, 1, DefaultHdrColorFormat, g_pHeap, CbrColorBufferOffsets[indexA] );
    g_SceneDepthBuffersCB2x[indexA].RecreatePlaced( width, height, 2, DSV_FORMAT, g_pHeap, CbrDepthBufferOffsets[indexA] );
    
    g_SceneColorBuffersCB2x[indexB].RecreatePlaced( width, height, 1, DefaultHdrColorFormat, g_pHeap, CbrColorBufferOffsets[indexB] );
    g_SceneDepthBuffersCB2x[indexB].RecreatePlaced( width, height, 2, DSV_FORMAT, g_pHeap, CbrDepthBufferOffsets[indexB] );
}

void Graphics::RecreateMsaaBuffers(int index, int msaaMode, int width, int height )
{
    int SampleCount = 0;

    ColorBuffer *pColor;
    DepthBuffer *pDepth;

    switch ( msaaMode )
    {
    case 1 : 
        pColor = &g_SceneColorBuffers2x[index];
        pDepth = &g_SceneDepthBuffers2x[index];
        SampleCount = 2;
        break;
    case 2:
        pColor = &g_SceneColorBuffers4x[index];
        pDepth = &g_SceneDepthBuffers4x[index];
        SampleCount = 4;
        break;
    case 3:
        pColor = &g_SceneColorBuffers8x[index];
        pDepth = &g_SceneDepthBuffers8x[index];
        SampleCount = 8;
        break;

    default:
        break;
    }

    pColor->RecreatePlaced( width, height, 1, DefaultHdrColorFormat, g_pHeap, ColorBufferOffsets[index] );
    pDepth->RecreatePlaced( width, height, SampleCount, DSV_FORMAT, g_pHeap, DepthBufferOffsets[index] );
}

void Graphics::RecreateSceneDepthBuffer(int index, int width, int height)
{
    g_SceneDepthBuffers[index].RecreatePlaced( width, height, 1, DSV_FORMAT, g_pHeap, DepthBufferOffsets[index] );
}

void Graphics::RecreateSceneColorBuffer(int index, int width, int height)
{
    g_SceneColorBuffers[index].RecreatePlaced( width, height, 1, DefaultHdrColorFormat, g_pHeap, SceneBufferOffsets[index] );
}
