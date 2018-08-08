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
#include "PixelBuffer.h"
#include "EsramAllocator.h"
#include "GraphicsCore.h"
#include "BufferManager.h"
#include "CommandContext.h"
#include "ReadbackBuffer.h"
#include <fstream>

#include <DirectXPackedVector.h>

using namespace Graphics;
using namespace PackedVector;

DXGI_FORMAT PixelBuffer::GetBaseFormat( DXGI_FORMAT defaultFormat )
{
    switch (defaultFormat)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return DXGI_FORMAT_R8G8B8A8_TYPELESS;

    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8A8_TYPELESS;

    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8X8_TYPELESS;

    // 32-bit Z w/ Stencil
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return DXGI_FORMAT_R32G8X24_TYPELESS;

    // No Stencil
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
        return DXGI_FORMAT_R32_TYPELESS;

    // 24-bit Z
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return DXGI_FORMAT_R24G8_TYPELESS;

    // 16-bit Z w/o Stencil
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
        return DXGI_FORMAT_R16_TYPELESS;

    default:
        return defaultFormat;
    }
}

DXGI_FORMAT PixelBuffer::GetUAVFormat( DXGI_FORMAT defaultFormat )
{
    switch (defaultFormat)
    {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM;

    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8X8_UNORM;

    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_R32_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;

#ifdef _DEBUG
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_D16_UNORM:

    ASSERT(false, "Requested a UAV format for a depth stencil format.");
#endif

    default:
        return defaultFormat;
    }
}

DXGI_FORMAT PixelBuffer::GetDSVFormat( DXGI_FORMAT defaultFormat )
{
    switch (defaultFormat)
    {
    // 32-bit Z w/ Stencil
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

    // No Stencil
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
        return DXGI_FORMAT_D32_FLOAT;

    // 24-bit Z
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return DXGI_FORMAT_D24_UNORM_S8_UINT;

    // 16-bit Z w/o Stencil
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
        return DXGI_FORMAT_D16_UNORM;

    default:
        return defaultFormat;
    }
}

DXGI_FORMAT PixelBuffer::GetDepthFormat( DXGI_FORMAT defaultFormat )
{
    switch (defaultFormat)
    {
    // 32-bit Z w/ Stencil
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

    // No Stencil
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;

    // 24-bit Z
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

    // 16-bit Z w/o Stencil
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
        return DXGI_FORMAT_R16_UNORM;

    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT PixelBuffer::GetStencilFormat( DXGI_FORMAT defaultFormat )
{
    switch (defaultFormat)
    {
    // 32-bit Z w/ Stencil
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;

    // 24-bit Z
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return DXGI_FORMAT_X24_TYPELESS_G8_UINT;

    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

//--------------------------------------------------------------------------------------
// Return the BPP for a particular format
//--------------------------------------------------------------------------------------
size_t PixelBuffer::BytesPerPixel( DXGI_FORMAT Format )
{
    switch( Format )
    {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
        return 16;

    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
        return 12;

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return 8;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        return 4;

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_B4G4R4A4_UNORM:
        return 2;

    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM:
    case DXGI_FORMAT_P8:
        return 1;

    default:
        return 0;
    }
}
void PixelBuffer::AssociateWithResource( DX12_DEVICE* Device, const std::wstring& Name, ID3D12Resource* Resource, D3D12_RESOURCE_STATES CurrentState )
{
    (Device); // Unused until we support multiple adapters

    ASSERT(Resource != nullptr);
    D3D12_RESOURCE_DESC ResourceDesc = Resource->GetDesc();

    m_pResource.Attach(Resource);
    m_UsageState = CurrentState;

    m_Width = (uint32_t)ResourceDesc.Width;		// We don't care about large virtual textures yet
    m_Height = ResourceDesc.Height;
    m_ArraySize = ResourceDesc.DepthOrArraySize;
    m_Format = ResourceDesc.Format;

#ifndef RELEASE
    m_Name = Name;
    m_pResource->SetName(Name.c_str());
#else
    (Name);
#endif
}

D3D12_RESOURCE_DESC PixelBuffer::DescribeTex2D( uint32_t Width, uint32_t Height, uint32_t DepthOrArraySize,
    uint32_t NumMips, DXGI_FORMAT Format, UINT Flags)
{
    m_Width = Width;
    m_Height = Height;
    m_ArraySize = DepthOrArraySize;
    m_Format = Format;
    
    return PixelBuffer::CreateResourceDesc( Width, Height, DepthOrArraySize, NumMips, Format, Flags );
}

void PixelBuffer::CreateTextureResource( DX12_DEVICE* Device, const std::wstring& Name,
    const D3D12_RESOURCE_DESC& ResourceDesc, D3D12_CLEAR_VALUE ClearValue, D3D12_GPU_VIRTUAL_ADDRESS /*VidMemPtr*/ )
{
    GpuResource::Destroy();

    CD3DX12_HEAP_PROPERTIES HeapProps(D3D12_HEAP_TYPE_DEFAULT);
    ASSERT_SUCCEEDED( Device->CreateCommittedResource( &HeapProps, D3D12_HEAP_FLAG_NONE,
        &ResourceDesc, D3D12_RESOURCE_STATE_COMMON, &ClearValue, MY_IID_PPV_ARGS(&m_pResource) ));

    m_UsageState = D3D12_RESOURCE_STATE_COMMON;
    m_GpuVirtualAddress = D3D12_GPU_VIRTUAL_ADDRESS_NULL;

#ifndef RELEASE
    m_Name = Name;
    m_pResource->SetName(Name.c_str());
#else
    (Name);
#endif
}

void PixelBuffer::CreateTextureResource( DX12_DEVICE* Device, const std::wstring& Name,
    const D3D12_RESOURCE_DESC& ResourceDesc, D3D12_CLEAR_VALUE ClearValue, EsramAllocator& /*Allocator*/ )
{
    CreateTextureResource(Device, Name, ResourceDesc, ClearValue);
}

void PixelBuffer::CreatePlacedResource( DX12_DEVICE* Device, const std::wstring& Name,
    const D3D12_RESOURCE_DESC& ResourceDesc, D3D12_CLEAR_VALUE ClearValue, ID3D12Heap *Heap, UINT64 HeapOffset )
{
    GpuResource::Destroy();

    RecreatePlacedResource( Device, ResourceDesc, ClearValue, Heap, HeapOffset );

    m_UsageState = D3D12_RESOURCE_STATE_COMMON;
    m_GpuVirtualAddress = D3D12_GPU_VIRTUAL_ADDRESS_NULL;

#ifndef RELEASE
    m_Name = Name;
    m_pResource->SetName(Name.c_str());
#else
    (Name);
#endif
}

void PixelBuffer::RecreatePlacedResource( DX12_DEVICE* Device, const D3D12_RESOURCE_DESC& ResourceDesc, 
        D3D12_CLEAR_VALUE ClearValue, ID3D12Heap *Heap, UINT64 HeapOffset )
{
    ASSERT_SUCCEEDED( Device->CreatePlacedResource( Heap, HeapOffset,
        &ResourceDesc, m_UsageState, &ClearValue, MY_IID_PPV_ARGS(&m_pResource) ));

#ifndef RELEASE
    m_pResource->SetName(m_Name.c_str());
#endif
}

void PixelBuffer::ExportToFile( const std::wstring& FilePath )
{
    // Create the buffer.  We will release it after all is done.
    ReadbackBuffer TempBuffer;
    TempBuffer.Create(L"Temporary Readback Buffer", m_Width * m_Height, (uint32_t)BytesPerPixel(m_Format));

    CommandContext::ReadbackTexture2D(TempBuffer, *this);

    // Retrieve a CPU-visible pointer to the buffer memory.  Map the whole range for reading.
    void* Memory = TempBuffer.Map();

    // Open the file and write the header followed by the texel data.
    std::ofstream OutFile(FilePath, std::ios::out | std::ios::binary);
    OutFile.write((const char*)&m_Format, 4);
    OutFile.write((const char*)&m_Width, 4); // Pitch
    OutFile.write((const char*)&m_Width, 4);
    OutFile.write((const char*)&m_Height, 4);
    OutFile.write((const char*)Memory, TempBuffer.GetBufferSize());
    OutFile.close();

    // No values were written to the buffer, so use a null range when unmapping.
    TempBuffer.Unmap();
}

float applySRGB(float x) {
	return x < 0.0031308f ? 12.92f * x : 1.055f * pow(x, 1.0f / 2.4f) - 0.055f;
}

void PixelBuffer::ExportToTGA(const std::wstring& FilePath) {

	// check if pixel format is supported
	if (m_Format != DXGI_FORMAT_R11G11B10_FLOAT) {
		Utility::Print("Error: ExportToTGA() unsupported pixel format.");
		return;
	}

	// Unlike ExportToFile we need to interpret the RGB channels and store them in the simple TGA format.
	ReadbackBuffer TempBuffer;
	TempBuffer.Create(L"Temporary Readback Buffer", m_Width * m_Height, (uint32_t)BytesPerPixel(m_Format));

	CommandContext::ReadbackTexture2D(TempBuffer, *this);

	// Retrieve a CPU-visible pointer to the buffer memory.  Map the whole range for reading.
	void* Memory = TempBuffer.Map();

	// Open the file and write the header followed by the texel data.
	std::ofstream OutFile(FilePath, std::ios::out | std::ios::binary);

	/////////////////////////////////////////////////////
	// header

	char header[18];
	ZeroMemory(header, 18);
	header[2] = 2; // uncompressed RGB
	header[12] = (m_Width & 0x00FF);
	header[13] = (m_Width & 0xFF00) / 256;
	header[14] = (m_Height & 0x00FF);
	header[15] = (m_Height & 0xFF00) / 256;
	header[16] = 24; // bpp
	
	OutFile.write((const char*)header, 18);

	std::vector<uint8_t> body;
	body.resize(m_Width * m_Height * 3);

	// We also just hardcode a possible LDR conversion, for the minimal effort to get acceptable images


	// pixel data in BGR format, row-major order
	XMFLOAT3PK* pixelData = (XMFLOAT3PK*)Memory;
	for (uint32_t y = 0; y < m_Height; y++) {
		for (uint32_t x = 0; x < m_Width; x++) {

			// extract channels from packed bits
			XMVECTOR unpackedData = XMLoadFloat3PK(pixelData);
			pixelData++;

			uint32_t pixLinIdx = 3 * ((m_Height - y - 1) * m_Width + x);

			body[pixLinIdx]= (uint8_t)(std::min<float>(applySRGB(unpackedData.m128_f32[2]) * 255.0f, 255.0f));
			body[pixLinIdx + 1] = (uint8_t)(std::min<float>(applySRGB(unpackedData.m128_f32[1]) * 255.0f, 255.0f));
			body[pixLinIdx + 2] = (uint8_t)(std::min<float>(applySRGB(unpackedData.m128_f32[0]) * 255.0f, 255.0f));
		}
	}

	OutFile.write((const char*) &body[0], body.size());

	OutFile.close();

	// No values were written to the buffer, so use a null range when unmapping.
	TempBuffer.Unmap();
}

D3D12_RESOURCE_DESC PixelBuffer::CreateResourceDesc( uint32_t Width, uint32_t Height, uint32_t DepthOrArraySize,
    uint32_t NumMips, DXGI_FORMAT Format, UINT Flags)
{
    D3D12_RESOURCE_DESC Desc = {};
    Desc.Alignment = 0;
    Desc.DepthOrArraySize = (UINT16)DepthOrArraySize;
    Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    Desc.Flags = (D3D12_RESOURCE_FLAGS)Flags;
    Desc.Format = GetBaseFormat(Format);
    Desc.Height = (UINT)Height;
    Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    Desc.MipLevels = (UINT16)NumMips;
    Desc.SampleDesc.Count = 1;
    Desc.SampleDesc.Quality = 0;
    Desc.Width = (UINT64)Width;
    
    return Desc;
}
