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

#pragma once

#include "GameCore.h"

class CommandContext;

namespace GpuCounterManager
{
    void Initialize( uint32_t MaxNumTimers = 4096, uint32_t MaxNumPipelineQueries = 256 );
    void Shutdown();

    // Reserve a unique timer index
    uint32_t NewTimer(void);
	// Reserve a unique pipeline query index
	uint32_t NewPipelineQuery(void);

    // Write start and stop time stamps on the GPU timeline
    void StartTimer(CommandContext& Context, uint32_t TimerIdx);
    void StopTimer(CommandContext& Context, uint32_t TimerIdx);

	// Collect pipeline statistics on the GPU
	void BeginPipelineQuery(CommandContext& Context, uint32_t QueryIdx);
	void EndPipelineQuery(CommandContext& Context, uint32_t QueryIdx);

    // Bookend all calls to GetTime() with Begin/End which correspond to Map/Unmap.  This
    // needs to happen either at the very start or very end of a frame.
    void BeginReadBack(void);
    void EndReadBack(void);

    // Returns the time in milliseconds between start and stop queries
    float GetTime(uint32_t TimerIdx);
	void GetPipelineStatistics(uint32_t QueryIdx, D3D12_QUERY_DATA_PIPELINE_STATISTICS& stats);
}
