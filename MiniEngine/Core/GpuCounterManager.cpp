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
#include "GpuCounterManager.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "CommandListManager.h"

namespace
{
    ID3D12QueryHeap* sm_TimerQueryHeap = nullptr;
    ID3D12Resource* sm_TimerReadBackBuffer = nullptr;
    uint64_t* sm_TimeStampBuffer = nullptr;
    uint64_t sm_Fence = 0;
    uint32_t sm_MaxNumTimers = 0;
    uint32_t sm_NumTimers = 1;
    uint64_t sm_ValidTimeStart = 0;
    uint64_t sm_ValidTimeEnd = 0;
    double sm_GpuTickDelta = 0.0;

	// pipeline statistics
	ID3D12QueryHeap* sm_PipelineQueryHeap = nullptr;
	ID3D12Resource* sm_PipelineQueryReadbackBuffer = nullptr;
	D3D12_QUERY_DATA_PIPELINE_STATISTICS* sm_PipelineQueryDataBuffer = nullptr;
	uint32_t sm_MaxNumPipelineQueries = 0;
	uint32_t sm_NumPipelineQueries = 0;

}

void GpuCounterManager::Initialize(uint32_t MaxNumTimers, uint32_t MaxNumPipelineQueries)
{
    uint64_t GpuFrequency;
    Graphics::g_CommandManager.GetCommandQueue()->GetTimestampFrequency(&GpuFrequency);
    sm_GpuTickDelta = 1.0 / static_cast<double>(GpuFrequency);

    D3D12_HEAP_PROPERTIES HeapProps;
    HeapProps.Type = D3D12_HEAP_TYPE_READBACK;
    HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    HeapProps.CreationNodeMask = 1;
    HeapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC TimerQueryBufferDesc;
    TimerQueryBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    TimerQueryBufferDesc.Alignment = 0;
    TimerQueryBufferDesc.Width = sizeof(uint64_t) * MaxNumTimers * 2;
    TimerQueryBufferDesc.Height = 1;
    TimerQueryBufferDesc.DepthOrArraySize = 1;
    TimerQueryBufferDesc.MipLevels = 1;
    TimerQueryBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    TimerQueryBufferDesc.SampleDesc.Count = 1;
    TimerQueryBufferDesc.SampleDesc.Quality = 0;
    TimerQueryBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    TimerQueryBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_RESOURCE_DESC PipelineQueryBufferDesc = TimerQueryBufferDesc;
	PipelineQueryBufferDesc.Width = sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS) * MaxNumPipelineQueries;

    ASSERT_SUCCEEDED(Graphics::g_Device->CreateCommittedResource( &HeapProps, D3D12_HEAP_FLAG_NONE, &TimerQueryBufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, MY_IID_PPV_ARGS(&sm_TimerReadBackBuffer) ));
    sm_TimerReadBackBuffer->SetName(L"GpuTimeStamp Buffer");

    D3D12_QUERY_HEAP_DESC QueryHeapDesc;
    QueryHeapDesc.Count = MaxNumTimers * 2;
    QueryHeapDesc.NodeMask = 1;
    QueryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    ASSERT_SUCCEEDED(Graphics::g_Device->CreateQueryHeap(&QueryHeapDesc, MY_IID_PPV_ARGS(&sm_TimerQueryHeap)));
    sm_TimerQueryHeap->SetName(L"GpuTimeStamp QueryHeap");

	ASSERT_SUCCEEDED(Graphics::g_Device->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE, &PipelineQueryBufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, MY_IID_PPV_ARGS(&sm_PipelineQueryReadbackBuffer)));
	sm_PipelineQueryReadbackBuffer->SetName(L"Pipeline Query Buffer");

	D3D12_QUERY_HEAP_DESC PipelineQueryHeapDesc = {};
	PipelineQueryHeapDesc.Count = MaxNumPipelineQueries;
	//QueryHeapDesc.NodeMask = 0;
	PipelineQueryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
	ASSERT_SUCCEEDED(Graphics::g_Device->CreateQueryHeap(&PipelineQueryHeapDesc, MY_IID_PPV_ARGS(&sm_PipelineQueryHeap)));
	sm_PipelineQueryHeap->SetName(L"Pipeline QueryHeap");
	
	sm_MaxNumTimers = (uint32_t)MaxNumTimers;
	sm_MaxNumPipelineQueries = (uint32_t)MaxNumPipelineQueries;
}

void GpuCounterManager::Shutdown()
{
    if (sm_TimerReadBackBuffer != nullptr)
        sm_TimerReadBackBuffer->Release();

    if (sm_TimerQueryHeap != nullptr)
        sm_TimerQueryHeap->Release();

	if (sm_PipelineQueryReadbackBuffer != nullptr)
		sm_PipelineQueryReadbackBuffer->Release();

	if (sm_PipelineQueryHeap != nullptr)
		sm_PipelineQueryHeap->Release();
}

uint32_t GpuCounterManager::NewTimer(void)
{
    return sm_NumTimers++;
}

uint32_t GpuCounterManager::NewPipelineQuery() {
	return sm_NumPipelineQueries++;
}

void GpuCounterManager::StartTimer(CommandContext& Context, uint32_t TimerIdx)
{
    Context.InsertTimeStamp(sm_TimerQueryHeap, TimerIdx * 2);
}

void GpuCounterManager::StopTimer(CommandContext& Context, uint32_t TimerIdx)
{
    Context.InsertTimeStamp(sm_TimerQueryHeap, TimerIdx * 2 + 1);
}

void GpuCounterManager::BeginPipelineQuery(CommandContext& Context, uint32_t QueryIdx) {
	Context.BeginPipelineQuery(sm_PipelineQueryHeap, QueryIdx);
}

void GpuCounterManager::EndPipelineQuery(CommandContext& Context, uint32_t QueryIdx) {
	Context.EndPipelineQuery(sm_PipelineQueryHeap, QueryIdx);
}

void GpuCounterManager::BeginReadBack(void)
{
    Graphics::g_CommandManager.WaitForFence(sm_Fence);

    D3D12_RANGE Range;
    Range.Begin = 0;
    Range.End = (sm_NumTimers * 2) * sizeof(uint64_t);
    ASSERT_SUCCEEDED(sm_TimerReadBackBuffer->Map(0, &Range, reinterpret_cast<void**>(&sm_TimeStampBuffer)));

	D3D12_RANGE PipelineRange;
	PipelineRange.Begin = 0;
	PipelineRange.End = sm_NumPipelineQueries * sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS);
	ASSERT_SUCCEEDED(sm_PipelineQueryReadbackBuffer->Map(0, &Range, reinterpret_cast<void**>(&sm_PipelineQueryDataBuffer)));

    sm_ValidTimeStart = sm_TimeStampBuffer[0];
    sm_ValidTimeEnd = sm_TimeStampBuffer[1];

    // On the first frame, with random values in the timestamp query heap, we can avoid a misstart.
    if (sm_ValidTimeEnd < sm_ValidTimeStart)
    {
        sm_ValidTimeStart = 0ull;
        sm_ValidTimeEnd = 0ull;
    }
}

void GpuCounterManager::EndReadBack(void)
{
    // Unmap with an empty range to indicate nothing was written by the CPU
    D3D12_RANGE EmptyRange = {};
    sm_TimerReadBackBuffer->Unmap(0, &EmptyRange);
	sm_PipelineQueryReadbackBuffer->Unmap(0, &EmptyRange);
    sm_TimeStampBuffer = nullptr;
	sm_PipelineQueryDataBuffer = nullptr;

    CommandContext& Context = CommandContext::Begin();
    Context.InsertTimeStamp(sm_TimerQueryHeap, 1);
    Context.ResolveTimeStamps(sm_TimerReadBackBuffer, sm_TimerQueryHeap, sm_NumTimers * 2);
	Context.ResolvePipelineQueries(sm_PipelineQueryReadbackBuffer, sm_PipelineQueryHeap, sm_NumPipelineQueries);
    Context.InsertTimeStamp(sm_TimerQueryHeap, 0);
    sm_Fence = Context.Finish();
}

float GpuCounterManager::GetTime(uint32_t TimerIdx)
{
    ASSERT(sm_TimeStampBuffer != nullptr, "Time stamp readback buffer is not mapped");
    ASSERT(TimerIdx < sm_NumTimers, "Invalid GPU timer index");

    uint64_t TimeStamp1 = sm_TimeStampBuffer[TimerIdx * 2];
    uint64_t TimeStamp2 = sm_TimeStampBuffer[TimerIdx * 2 + 1];

    if (TimeStamp1 < sm_ValidTimeStart || TimeStamp2 > sm_ValidTimeEnd || TimeStamp2 <= TimeStamp1 )
        return 0.0f;

    return static_cast<float>(sm_GpuTickDelta * (TimeStamp2 - TimeStamp1));
}

void GpuCounterManager::GetPipelineStatistics(uint32_t QueryIdx, D3D12_QUERY_DATA_PIPELINE_STATISTICS& stats) {
	ASSERT(sm_PipelineQueryDataBuffer != nullptr, "Pipeline query readback buffer is not mapped");
	ASSERT(QueryIdx < sm_NumPipelineQueries, "Invalid pipeline query index");

	stats = sm_PipelineQueryDataBuffer[QueryIdx];
}
