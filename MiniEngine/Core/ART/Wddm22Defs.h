#pragma once
//#define WDDM22

// -- define different device and commandlist for systems with or without WDDM2.2 support
#ifdef WDDM22
#define DX12_DEVICE	ID3D12Device2
#define DX12_GRAPHICSCOMMANDLIST ID3D12GraphicsCommandList1
#else
#define DX12_DEVICE ID3D12Device
#define DX12_GRAPHICSCOMMANDLIST ID3D12GraphicsCommandList
#endif