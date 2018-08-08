#pragma once

#include "..\CommonDefs.h"

#ifdef ART_ENABLE_GUI

#include "AnimationWidget.h"

// to avoid re-including these to all GUI classes
#include <d3d11.h>
#include <d3d11on12.h>
#include <dwrite.h>
#include <d2d1_3.h>
#include <wrl.h>

#include "..\..\GraphicsCommon.h"

class GraphicsContext;

namespace ARTGUI {

	using namespace Microsoft::WRL;

	// GUI has three states:
	// 1 - hidden
	// 2 - visible
	// 3 - focused (visible and capturing input)

	void SetHwnd(HWND hWnd);
	void Initialize();
	void Finalize();
	void ReleaseSwapchainResources();
	void Resize(uint32_t width, uint32_t height);
	void Update(float frameTime);
	void Display(GraphicsContext& Context);
	bool IsVisible();
	bool IsFocused();

	void SetText(const std::string& text);

	void AddWidgetBase(const IGUIWidget_ptr& widget);

	// convenience method to avoid shared ptr conversions
	template<class T>
	void AddWidget(const std::shared_ptr<T>& widget) {
		IGUIWidget_ptr iWidget = std::dynamic_pointer_cast<IGUIWidget, T>(widget);
		AddWidgetBase(widget);
	}

	LRESULT WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	extern ID3D11DeviceContext*			g_DeviceContext11;
	extern ComPtr<ID3D11On12Device>		g_Device11on12;
	extern ComPtr<ID2D1Factory3>		g_D2DFactory;
	extern ComPtr<ID2D1Device2>			g_D2DDevice;
	extern ComPtr<ID2D1DeviceContext2>	g_D2DDeviceContext;
	extern ComPtr<IDWriteFactory>		g_DWriteFactory;
}

#endif // ART_ENABLE_GUI
