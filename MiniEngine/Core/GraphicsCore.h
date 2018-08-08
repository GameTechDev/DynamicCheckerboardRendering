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

// Modified 2018, Intel Corporation
// Added checking for Intel GPU

#pragma once

#include "PipelineState.h"
#include "DescriptorHeap.h"
#include "RootSignature.h"
#include "SamplerManager.h"
#include "GraphicsCommon.h"

class ColorBuffer;
class DepthBuffer;
class GraphicsPSO;
class CommandContext;
class CommandListManager;
class CommandSignature;
class ContextManager;

namespace Graphics
{
#ifndef RELEASE
    extern const GUID WKPDID_D3DDebugObjectName;
#endif

    using namespace Microsoft::WRL;

    void Initialize(void);
    void Resize(uint32_t width, uint32_t height);
    void Terminate(void);
    void Shutdown(void);
    void Present(void);

    extern uint32_t g_DisplayWidth;
    extern uint32_t g_DisplayHeight;
    extern float g_ResolutionScale;

    // Returns the number of elapsed frames since application start
    uint64_t GetFrameCount(void);

	uint32_t GetNumBackbuffers(void);
	
	// for multi-buffered rendering, returns the index of the currently active backbuffer
	uint32_t GetActiveFramebufferIdx(void);

	ColorBuffer& GetFramebufferByIdx(uint32_t idx);

	ColorBuffer& GetOverlayBuffer();

    // The amount of time elapsed during the last completed frame.  The CPU and/or
    // GPU may be idle during parts of the frame.  The frame time measures the time
    // between calls to present each frame.
    float GetFrameTime(void);

    // The total number of frames per second
    float GetFrameRate(void);

	// if greater  than zero, the app will update the time with a constant rate
	// if zero, it will render interactively, based on the system time
	// The meaning of the time is "seconds per frame"
	void SetOfflineTimestep(float spf);
	
	extern DX12_DEVICE* g_Device;
    extern CommandListManager g_CommandManager;
    extern ContextManager g_ContextManager;

    extern D3D_FEATURE_LEVEL g_D3DFeatureLevel;
    extern bool g_bTypedUAVLoadSupport_R11G11B10_FLOAT;
    extern bool g_bEnableHDROutput;

    extern DescriptorAllocator g_DescriptorAllocator[];
    inline D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor( D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1 )
    {
        return g_DescriptorAllocator[Type].Allocate(Count);
    }

    extern bool g_IntelIsGPU;
    extern RootSignature g_GenerateMipsRS;
    extern ComputePSO g_GenerateMipsLinearPSO[4];
    extern ComputePSO g_GenerateMipsGammaPSO[4];
}
