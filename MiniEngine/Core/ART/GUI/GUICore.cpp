
#include "pch.h"

#include "GUICore.h"

#ifdef ART_ENABLE_GUI

#include "imgui\imgui.h"
#include "imgui\imgui_impl_dx11.h"

#include "..\..\CommandContext.h"
#include "..\..\GraphicsCore.h"
#include "..\..\CommandListManager.h"
#include "..\..\ColorBuffer.h"
#include "..\..\Utility.h"
#include "..\..\GameInput.h"

#define IM_ARRAYSIZE(_ARR)  ((int)(sizeof(_ARR)/sizeof(*_ARR)))
#define IM_MAX(_A,_B)       (((_A) >= (_B)) ? (_A) : (_B))

#include <vector>
#include <string>

using namespace Graphics;
using namespace std;

namespace ARTGUI {

	ID3D11DeviceContext*		g_DeviceContext11 = nullptr;
	ComPtr<ID3D11Device>		g_Device11;
	ComPtr<ID3D11On12Device>	g_Device11on12;
	ComPtr<ID2D1Factory3>		g_D2DFactory;
	ComPtr<ID2D1Device2>		g_D2DDevice;
	ComPtr<ID2D1DeviceContext2>	g_D2DDeviceContext;
	ComPtr<IDWriteFactory>		g_DWriteFactory;

	vector<ComPtr<ID3D11Texture2D>>			g_WrappedBackbuffers;
	vector<ComPtr<ID2D1Bitmap1>>			g_D2DRenderTargets;
	vector<ComPtr<ID3D11RenderTargetView>>	g_D3D11RTV;

	// temporary GUI objects for testing
	ComPtr<ID2D1SolidColorBrush>	g_TestBrush;
	ComPtr<IDWriteTextFormat>		g_TestText;

	vector<IGUIWidget_ptr>			g_Widgets;

	HWND							g_hWnd = NULL;

	GameInput::DigitalInput			g_KeyCodeLUT[512];

	bool							g_IsVisible = false;
	bool							g_IsFocused = true;

	std::wstring					g_Text;

	void initKeyCodes() {
		using namespace GameInput;

		for (uint32_t i = 0; i < 512; i++) {
			g_KeyCodeLUT[i] = kNumKeys;
		}
		
		g_KeyCodeLUT['a'] = kKey_a;
		g_KeyCodeLUT['b'] = kKey_b;
		g_KeyCodeLUT['c'] = kKey_c;
		g_KeyCodeLUT['d'] = kKey_d;
		g_KeyCodeLUT['e'] = kKey_e;
		g_KeyCodeLUT['f'] = kKey_f;
		g_KeyCodeLUT['g'] = kKey_g;
		g_KeyCodeLUT['h'] = kKey_h;
		g_KeyCodeLUT['i'] = kKey_i;
		g_KeyCodeLUT['j'] = kKey_j;
		g_KeyCodeLUT['k'] = kKey_k;
		g_KeyCodeLUT['l'] = kKey_l;
		g_KeyCodeLUT['m'] = kKey_m;
		g_KeyCodeLUT['n'] = kKey_n;
		g_KeyCodeLUT['o'] = kKey_o;
		g_KeyCodeLUT['p'] = kKey_p;
		g_KeyCodeLUT['q'] = kKey_q;
		g_KeyCodeLUT['r'] = kKey_r;
		g_KeyCodeLUT['s'] = kKey_s;
		g_KeyCodeLUT['t'] = kKey_t;
		g_KeyCodeLUT['u'] = kKey_u;
		g_KeyCodeLUT['v'] = kKey_v;
		g_KeyCodeLUT['w'] = kKey_w;
		g_KeyCodeLUT['x'] = kKey_x;
		g_KeyCodeLUT['y'] = kKey_y;
		g_KeyCodeLUT['z'] = kKey_z;

		g_KeyCodeLUT['0'] = kKey_0;
		g_KeyCodeLUT['1'] = kKey_1;
		g_KeyCodeLUT['2'] = kKey_2;
		g_KeyCodeLUT['3'] = kKey_3;
		g_KeyCodeLUT['4'] = kKey_4;
		g_KeyCodeLUT['5'] = kKey_5;
		g_KeyCodeLUT['6'] = kKey_6;
		g_KeyCodeLUT['7'] = kKey_7;
		g_KeyCodeLUT['8'] = kKey_8;
		g_KeyCodeLUT['9'] = kKey_9;

	}

	void SetHwnd(HWND hWnd) {
		g_hWnd = hWnd;
	}
		
	void Initialize() {
		// Must be called after the DX12 device and the swapchain already exist

		initKeyCodes();

		const uint32_t numBackbuffers = Graphics::GetNumBackbuffers();

		Finalize();

		ART_ASSERT(g_Device);
		ART_ASSERT(g_hWnd);

		UINT d3d11DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#ifdef _DEBUG
		d3d11DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		ID3D12CommandQueue* graphicsQueue = g_CommandManager.GetGraphicsQueue().GetCommandQueue();

		ASSERT_SUCCEEDED(D3D11On12CreateDevice(
			g_Device,
			d3d11DeviceFlags,
			nullptr,
			0,
			reinterpret_cast<IUnknown**>(&graphicsQueue),
			1,
			0,
			&g_Device11,
			&g_DeviceContext11,
			nullptr
		));

		ASSERT_SUCCEEDED(g_Device11.As(&g_Device11on12));

		// D2D / DWrite resources
		D2D1_FACTORY_OPTIONS d2dFactoryOptions = {};
		D2D1_DEVICE_CONTEXT_OPTIONS deviceOptions = D2D1_DEVICE_CONTEXT_OPTIONS_NONE;
		ASSERT_SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory3),
			&d2dFactoryOptions, &g_D2DFactory));

		ComPtr<IDXGIDevice> dxgiDevice;
		ASSERT_SUCCEEDED(g_Device11on12.As(&dxgiDevice));
		ASSERT_SUCCEEDED(g_D2DFactory->CreateDevice(dxgiDevice.Get(), &g_D2DDevice));
		ASSERT_SUCCEEDED(g_D2DDevice->CreateDeviceContext(deviceOptions, &g_D2DDeviceContext));
		ASSERT_SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &g_DWriteFactory));

		g_WrappedBackbuffers.resize(numBackbuffers);
		g_D2DRenderTargets.resize(numBackbuffers);
		g_D3D11RTV.resize(numBackbuffers);

			// initialize test GUI objects
			ASSERT_SUCCEEDED(g_D2DDeviceContext->CreateSolidColorBrush(
				D2D1::ColorF(D2D1::ColorF::AntiqueWhite), &g_TestBrush
				 ));
		
			ASSERT_SUCCEEDED(g_DWriteFactory->CreateTextFormat(
				L"Verdana",
				NULL,
				DWRITE_FONT_WEIGHT_NORMAL,
				DWRITE_FONT_STYLE_NORMAL,
				DWRITE_FONT_STRETCH_NORMAL,
				36,
				L"en-us",
				&g_TestText
				));
		ASSERT_SUCCEEDED(g_TestText->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));
		ASSERT_SUCCEEDED(g_TestText->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));

		ImGui_ImplDX11_Init(g_hWnd, g_Device11.Get(), g_DeviceContext11);

		ImGuiIO& io = ImGui::GetIO();
		io.MousePos.x = 100;
		io.MousePos.y = 100;

		Utility::Print("Basic GUI controls: [F1] toggle visibility, [TAB] toggle focus");

	}

	void Finalize() {

		ImGui_ImplDX11_Shutdown();

		ReleaseSwapchainResources();
		
		if (g_DeviceContext11) {
			g_DeviceContext11->Release();
			g_DeviceContext11 = nullptr;
		}

		g_D2DDeviceContext.Reset();
		g_DWriteFactory.Reset();
		g_D2DFactory.Reset();
		g_D2DDevice.Reset();

		g_Device11on12.Reset();
		g_Device11.Reset();
	}

	void ReleaseSwapchainResources() {

		for (auto& rtv : g_D3D11RTV)
			rtv.Reset();
		for (auto& d2dRT : g_D2DRenderTargets)
			d2dRT.Reset();
		for (auto& wrappedBackbuffer : g_WrappedBackbuffers)
			wrappedBackbuffer.Reset();
	
		ImGui_ImplDX11_InvalidateDeviceObjects();
	}

	void Resize(uint32_t width, uint32_t height) {

		// get render targets as DX11 wrapped resources
		size_t numRTs = g_WrappedBackbuffers.size();
		for (size_t iRT = 0; iRT < numRTs; iRT++) {

			auto rtv = Graphics::GetFramebufferByIdx((uint32_t) iRT).GetResource();

			D3D11_RESOURCE_FLAGS d3d11Flags = { D3D11_BIND_RENDER_TARGET };
			ASSERT_SUCCEEDED(g_Device11on12->CreateWrappedResource(
				rtv,
				&d3d11Flags,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				IID_PPV_ARGS(&g_WrappedBackbuffers[iRT])
			));

			// Query the desktop's dpi settings, which will be used to create
			// D2D's render targets.
			float dpiX;
			float dpiY;
			g_D2DFactory->GetDesktopDpi(&dpiX, &dpiY);
			D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
				D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
				D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
				dpiX,
				dpiY
			);

			// create a render target for D3D11 to draw into
			// (we currently use this for ImGUI)
			D3D11_TEXTURE2D_DESC rtTexDesc;
			g_WrappedBackbuffers[iRT]->GetDesc(&rtTexDesc);

			D3D11_RENDER_TARGET_VIEW_DESC render_target_view_desc;
			ZeroMemory(&render_target_view_desc, sizeof(render_target_view_desc));
			render_target_view_desc.Format = rtTexDesc.Format;
			render_target_view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			render_target_view_desc.Texture2D.MipSlice = 0;

			ASSERT_SUCCEEDED(g_Device11->CreateRenderTargetView(g_WrappedBackbuffers[iRT].Get(), &render_target_view_desc, g_D3D11RTV[iRT].ReleaseAndGetAddressOf()));

			ASSERT(ImGui_ImplDX11_CreateDeviceObjects());

			// create a render target for D2D to draw into this back buffer
			ComPtr<IDXGISurface> surface;
			ASSERT_SUCCEEDED(g_WrappedBackbuffers[iRT].As(&surface));
			ASSERT_SUCCEEDED(g_D2DDeviceContext->CreateBitmapFromDxgiSurface(
				surface.Get(),
				&bitmapProperties,
				&g_D2DRenderTargets[iRT]
			));
		}

	}

	void Update(float elapsedTime) {

		GameInput::AllowReadInputs(true);

		// to detect first key stroke.
		// we cannot rely on GameInput::FirstKeyPressed, since we clear the key states if the GUI is in focus
		// to prevent the game from intercepting the keystrokes
		static bool toggleFocused[2] = { false, false };

		toggleFocused[0] = GameInput::IsPressed(GameInput::kKey_tab);

		// toggle visible / focused status
		if (GameInput::IsFirstPressed(GameInput::kKey_f1)) {
			if (!g_IsVisible) {
				g_IsVisible = true;
				g_IsFocused = true;
			}
			else
				g_IsVisible = false;
		}

		if (GameInput::IsFirstPressed(GameInput::kKey_escape))
			g_IsVisible = false;

		if (!IsVisible())
			return;

		if (toggleFocused[0] && !toggleFocused[1]) {
			g_IsFocused = !g_IsFocused;
			GameInput::ClearDigitalInput(GameInput::kKey_tab);
		}
		toggleFocused[1] = toggleFocused[0];

		if (!IsFocused())
			return;

		ImGuiIO& io = ImGui::GetIO();

		// mouse deltas
		// For some reason MiniEngine scales the analog values quite aggressively
		const float sensitivity = 1200;
		float dMouseX = GameInput::GetAnalogInput(GameInput::kAnalogMouseX) * sensitivity;
		float dMouseY = GameInput::GetAnalogInput(GameInput::kAnalogMouseY) * -sensitivity;

		io.MousePos.x += dMouseX;
		io.MousePos.y += dMouseY;

		io.MouseDown[0] = GameInput::IsPressed(GameInput::kMouse0);
		io.MouseDown[1] = GameInput::IsPressed(GameInput::kMouse1);
		io.MouseDown[2] = GameInput::IsPressed(GameInput::kMouse2);

		for (unsigned short i = 0; i < 512; i++) {
			if (g_KeyCodeLUT[i] != GameInput::kNumKeys) {
				io.KeysDown[i] = GameInput::IsPressed(g_KeyCodeLUT[i]) ? 1 : 0;
			}
		}

		for (auto& widget : g_Widgets) {
			widget->Update(elapsedTime);
		}

		// block all keyboard and mouse events from game
		GameInput::AllowReadInputs(false);
	}

	void Display(GraphicsContext& Context) {
		if (!IsVisible() && g_Text.size() == 0) return;

		const uint32_t fbIndex = Graphics::GetActiveFramebufferIdx();

		Context.TransitionResource(Graphics::GetFramebufferByIdx(fbIndex), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		Context.Flush();

		ID3D11Resource* backBufferResource = g_WrappedBackbuffers[fbIndex].Get();
		g_Device11on12->AcquireWrappedResources(&backBufferResource, 1);

		// Text display
		if (g_Text.size() > 0) {
			D2D1_SIZE_F rtSize = g_D2DRenderTargets[fbIndex]->GetSize();
			D2D1_RECT_F rect = D2D1::RectF(0, 50, rtSize.width - 100, rtSize.height - 50);

			g_D2DDeviceContext->SetTarget(g_D2DRenderTargets[fbIndex].Get());
			g_D2DDeviceContext->BeginDraw();

			g_D2DDeviceContext->SetTransform(D2D1::Matrix3x2F::Translation(0, -0.5f * rtSize.height + 50));
			g_TestText->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
			g_D2DDeviceContext->DrawTextW(
				g_Text.c_str(),
				(UINT32) g_Text.size(),
				g_TestText.Get(),
				&rect,
				g_TestBrush.Get()
			);

			g_D2DDeviceContext->EndDraw();
		}

		if (g_IsVisible) {
			ImGui_ImplDX11_NewFrame();

			for (auto& widget : g_Widgets)
				widget->Render();

			auto* rtv = g_D3D11RTV[fbIndex].Get();
			g_DeviceContext11->OMSetRenderTargets(1, &rtv, NULL);

			ImGui::Render();

		}

		ID3D11RenderTargetView* pNullRTV = nullptr;
		g_DeviceContext11->OMSetRenderTargets(1, &pNullRTV, NULL);
		g_DeviceContext11->Flush();

		g_Device11on12->ReleaseWrappedResources(&backBufferResource, 1);

		Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	bool IsFocused() {
		return g_IsVisible && g_IsFocused; // tmp
	}

	bool IsVisible() {
		return g_IsVisible;
	}

	void AddWidgetBase(const IGUIWidget_ptr& widget) {
		g_Widgets.push_back(widget);
	}

	LRESULT WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		if (!IsFocused())
			return 0;

		return ImGui_ImplDX11_WndProcHandler(hWnd, msg, wParam, lParam);
	}

	void SetText(const std::string& text) {
		g_Text = std::wstring(text.begin(), text.end());
	}

}

#endif // ART_ENABLE_GUI
