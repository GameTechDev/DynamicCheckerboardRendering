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
#include "../../Core/Shaders/ShaderUtility.hlsli"

#define CheckerboardResolve_RootSig \
    "RootFlags(0), " \
    "CBV(b0), " \
    "DescriptorTable(SRV(t0, numDescriptors = 4))," \
    "DescriptorTable(UAV(u0, numDescriptors = 1))"

Texture2DMS<float4> DownSizedInColor2x0 : register(t0);
Texture2DMS<float4> DownSizedInColor2x1 : register(t1);
Texture2DMS<unorm float> DownSizedInDepth2x0 : register(t2);
Texture2DMS<unorm float> DownSizedInDepth2x1 : register(t3);
RWTexture2D<float4> OutColor : register(u0);

#define Up		0
#define Down	1
#define Left	2
#define Right	3

//#define HALF_PRECISION;

#ifdef HALF_PRECISION
#define lpfloat min16float
#define lpfloat2 min16float2
#define lpfloat3 min16float3
#define lpfloat4 min16float4
#else
#define lpfloat float
#define lpfloat2 float2
#define lpfloat3 float3
#define lpfloat4 float4
#endif

cbuffer CB0 : register(b0)
{
    float FrameOffset;
    float DepthTolerance;
    uint Flags;
    float _Pad;
    float4 LinearZTransform;
    float4x4 CurrViewProj;
    float4x4 PrevInvViewProj;
}

// Simple tonemap to invtonemap color blend
// should be replaced by customized solution
float3 hdrColorBlend(float3 a, float3 b, float3 c, float3 d)
{
    // Reinhard 
    float3 t_a = a / (a + 1);
    float3 t_b = b / (b + 1);
    float3 t_c = c / (c + 1);
    float3 t_d = d / (d + 1);

    float3 color = (t_a + t_b + t_c + t_d) * .25f;

    // back to hdr
    return -color / (color - 1);
}

// convert projected depth into projected pixel position 
// for frame N-1
uint2 previousPixelPos(float2 pixel, float currDepth, float2 res)
{
    // no depth buffer information
    if (currDepth <= 0.0)
        return pixel;

    uint2 old_pixel = floor(pixel);

    // Projection is flipped from UV coords
    pixel.y = res.y - pixel.y - 1;

    float2 projected = pixel / res * 2.0 - 1;

    float4 re_projected_pre_w_divide = float4(projected.x, projected.y, currDepth, 1.0);
    float4 ws = mul(re_projected_pre_w_divide, PrevInvViewProj);
    ws /= ws.w;

    float4 ws_to_curr_projection = mul(ws, CurrViewProj);
    ws_to_curr_projection = ws_to_curr_projection / ws_to_curr_projection.w;

    float2 curr = ws_to_curr_projection.xy * (res / 2) + (res / 2);
    curr.y = res.y - curr.y - 1;

    uint2 new_pixel = floor(curr);
    int2 delta = new_pixel - old_pixel;

    return old_pixel - delta;
}

float4 readFromQuadrant(int2 pixel, int quadrant)
{
    if (0 == quadrant)
        return DownSizedInColor2x0.Load(pixel, 1);
    else if (1 == quadrant)
        return DownSizedInColor2x1.Load(pixel + int2(1, 0), 1);
    else if (2 == quadrant)
        return DownSizedInColor2x1.Load(pixel, 0);
    else //( 3 == quadrant )
        return DownSizedInColor2x0.Load(pixel, 0);
}

float readDepthFromQuadrant(int2 pixel, int quadrant)
{
    if (0 == quadrant)
        return DownSizedInDepth2x0.Load(pixel, 1);
    else if (1 == quadrant)
        return DownSizedInDepth2x1.Load(pixel + int2(1, 0), 1);
    else if (2 == quadrant)
        return DownSizedInDepth2x1.Load(pixel, 0);
    else //( 3 == quadrant )
        return DownSizedInDepth2x0.Load(pixel, 0);
}

float4 colorFromCardinalOffsets(uint2 qtr_res_pixel, int2 offsets[4], int quadrants[2])
{
    float4 color[4];

    float2 w;

    color[Up] = readFromQuadrant(qtr_res_pixel + offsets[Up], quadrants[0]);
    color[Down] = readFromQuadrant(qtr_res_pixel + offsets[Down], quadrants[0]);
    color[Left] = readFromQuadrant(qtr_res_pixel + offsets[Left], quadrants[1]);
    color[Right] = readFromQuadrant(qtr_res_pixel + offsets[Right], quadrants[1]);

    return float4(hdrColorBlend(color[Up].rgb, color[Down].rgb, color[Left].rgb, color[Right].rgb), 1);
}

void getCardinalOffsets(int quadrant, out int2 offsets[4], out int quadrants[2])
{
    if (quadrant == 0)
    {
        offsets[Up] = -int2(0, 1);
        offsets[Down] = 0;
        offsets[Left] = -int2(1, 0);
        offsets[Right] = 0;

        quadrants[0] = 2;
        quadrants[1] = 1;
    }
    else if (quadrant == 1)
    {
        offsets[Up] = -int2(0, 1);
        offsets[Down] = 0;
        offsets[Left] = 0;
        offsets[Right] = +int2(1, 0);

        quadrants[0] = 3;
        quadrants[1] = 0;
    }
    else if (quadrant == 2)
    {
        offsets[Up] = 0;
        offsets[Down] = +int2(0, 1);
        offsets[Left] = -int2(1, 0);
        offsets[Right] = 0;

        quadrants[0] = 0;
        quadrants[1] = 3;
    }
    else // ( quadrant == 3 )
    {
        offsets[Up] = 0;
        offsets[Down] = +int2(0, 1);
        offsets[Left] = 0;
        offsets[Right] = +int2(1, 0);

        quadrants[0] = 1;
        quadrants[1] = 2;
    }
}

float projectedDepthToLinear(float depth)
{
    return (depth * LinearZTransform.x + LinearZTransform.y) / (depth * LinearZTransform.z + LinearZTransform.w);
}

float4 Resolve2xSampleTemporal(float FrameOffset, uint2 dispatchThreadId)
{
    uint2 full_res;
    OutColor.GetDimensions(full_res.x, full_res.y);

#define DEBUG_RENDER

#ifdef DEBUG_RENDER
    const bool render_motion_vectors = (Flags & 0x01) != 0;
    const bool render_missing_pixels = (Flags & 0x02) != 0;
    const bool render_qtr_motion_pixels = (Flags & 0x04) != 0;
    const bool render_checker_pattern_odd = (Flags & 0x08) != 0;
    const bool render_checker_pattern_even = (Flags & 0x10) != 0;
    const bool render_obstructed_pixels = (Flags & 0x20) != 0;
#endif

    const bool check_shading_occlusion = (Flags & 0x40) != 0;
    const bool render_resolution_changed = (Flags & 0x80) != 0;
    const uint2 qtr_res = full_res * .5;
    const uint2 full_res_pixel = dispatchThreadId.xy;
    const uint2 qtr_res_pixel = floor(dispatchThreadId.xy * .5);
    const uint quadrant = (dispatchThreadId.x & 0x1) + (dispatchThreadId.y & 0x1) * 2;
    const float tolerance = DepthTolerance;

    const uint frame_lookup[2][2] =
    {
       { 0, 3 },
       { 1, 2 }
    };

    uint frame_quadrants[2];
    frame_quadrants[0] = frame_lookup[FrameOffset][0];
    frame_quadrants[1] = frame_lookup[FrameOffset][1];

#ifdef DEBUG_RENDER
    if (render_checker_pattern_odd + render_checker_pattern_even > 0)
    {
        if ((render_checker_pattern_even && (quadrant == 0 || quadrant == 3)) ||
            (render_checker_pattern_odd && (quadrant == 1 || quadrant == 2)))
            return readFromQuadrant(qtr_res_pixel, quadrant);
        else
            return float4(0, 0, 0, 1);
    }
#endif

    // if the pixel we are writing to is in a MSAA quadrant which matches our latest CB frame
    // then read it directly and we're done
    if (frame_quadrants[0] == quadrant || frame_quadrants[1] == quadrant)
        return readFromQuadrant(qtr_res_pixel, quadrant);
    else
    {
        // We need to read from Frame N-1

        int2 cardinal_offsets[4];
        int cardinal_quadrants[2];

        // Get the locations of the pixels in Frame N which surround
        // our current pixel location
        getCardinalOffsets(quadrant, cardinal_offsets, cardinal_quadrants);

        bool missing_pixel = false;

        // if the render resolution changed then last frame's data is invalid
        // so ignore it entirely and early out
        if (render_resolution_changed)
            return colorFromCardinalOffsets(qtr_res_pixel, cardinal_offsets, cardinal_quadrants);

        // What is the depth at this pixel which was written to by Frame N-1
        float depth = readDepthFromQuadrant(qtr_res_pixel, quadrant);

        // Project that through the matrices and get the screen space position
        // this pixel was rendered in Frame N-1
        uint2 prev_pixel_pos = previousPixelPos(full_res_pixel + .5f, depth, full_res);

        int2 pixel_delta = floor((full_res_pixel + .5f) - prev_pixel_pos);
        int2 qtr_res_pixel_delta = pixel_delta * .5f;

        int2 prev_qtr_res_pixel = floor(prev_pixel_pos * .5f);

        // Which MSAA quadrant was this pixel in when it was shaded in Frame N-1
        uint quadrant_needed = (prev_pixel_pos.x & 0x1) + (prev_pixel_pos.y & 0x1) * 2;

#ifdef DEBUG_RENDER
        if (render_motion_vectors && (pixel_delta.x || pixel_delta.y))
            return float4(1, 0, 0, 1);

        if (render_qtr_motion_pixels && (qtr_res_pixel_delta.x || qtr_res_pixel_delta.y))
            return float4(0, 1, 0, 1);
#endif

        // if it falls on this frame (Frame N)'s quadrant then the shading information is missing
        // so extrapolate the color from the texels around us
        if (frame_quadrants[0] == quadrant_needed || frame_quadrants[1] == quadrant_needed)
            missing_pixel = true;
        else if (qtr_res_pixel_delta.x || qtr_res_pixel_delta.y)
        {
            // Otherwise we might have the shading information,
            // Now we check to see if it's obstructed

            // If the user doesn't want to check for obstruction we just assume it's obstructed
            // and this pixel will be an extrapolation of the Frame N pixels around it
            // This generally saves on perf and isn't noticeable because the pixels are in motion anyway
            if (false == check_shading_occlusion)
                missing_pixel = true;
            else
            {
                float4 current_depth = 0;
                const int count = 4;

                // Fetch the interpolated depth at this location in Frame N
                current_depth.x = readDepthFromQuadrant(qtr_res_pixel + cardinal_offsets[Left], cardinal_quadrants[1]);
                current_depth.y = readDepthFromQuadrant(qtr_res_pixel + cardinal_offsets[Right], cardinal_quadrants[1]);

                current_depth.z = readDepthFromQuadrant(qtr_res_pixel + cardinal_offsets[Down], cardinal_quadrants[0]);
                current_depth.w = readDepthFromQuadrant(qtr_res_pixel + cardinal_offsets[Up], cardinal_quadrants[0]);

                float current_depth_avg = (projectedDepthToLinear(current_depth.x) +
                    projectedDepthToLinear(current_depth.y) +
                    projectedDepthToLinear(current_depth.z) +
                    projectedDepthToLinear(current_depth.w)) * .25f;

                // reach across the frame N-1 and grab the depth of the pixel we want
                // then compare it to Frame N's depth at this pixel to see if it's within range
                float prev_depth = readDepthFromQuadrant(prev_qtr_res_pixel, quadrant_needed);
                prev_depth = projectedDepthToLinear(prev_depth);

                // if the discrepancy is too large assume the pixel we need to 
                // fetch from the previous buffer is missing
                float diff = prev_depth - current_depth_avg;
                missing_pixel = abs(diff) >= tolerance;

#ifdef DEBUG_RENDER
                if (render_obstructed_pixels && missing_pixel)
                    return float4(1, 0, 1, 1);
#endif
            }
        }

#ifdef DEBUG_RENDER
        if (render_missing_pixels && missing_pixel)
            return float4(1, 0, 0, 1);
#endif

        // If we've determined the pixel (i.e. shading information) is missing,
        // then extrapolate the missing color by blending the 
        // current frame's up, down, left, right pixels
        if (missing_pixel == true)
            return colorFromCardinalOffsets(qtr_res_pixel, cardinal_offsets, cardinal_quadrants);
        else
            return readFromQuadrant(prev_qtr_res_pixel, quadrant_needed);
    }
}

[RootSignature(CheckerboardResolve_RootSig)]
[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float4 Color = Resolve2xSampleTemporal(FrameOffset, DTid.xy);
    OutColor[DTid.xy] = float4(Color.xyz, 1.0f);
}