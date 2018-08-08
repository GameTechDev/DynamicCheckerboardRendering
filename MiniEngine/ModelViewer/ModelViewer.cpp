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
// Author(s):  Alex Nankervis
//             James Stanard
//

// Modified 2018, Intel Corporation
// Added Checkerboard rendering paths

#include "Scene.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "CameraController.h"
#include "BufferManager.h"
//#include "Camera.h"
//#include "Model.h"
#include "GpuBuffer.h"
#include "CommandContext.h"
#include "SamplerManager.h"
#include "TemporalEffects.h"
#include "MotionBlur.h"
#include "DepthOfField.h"
#include "PostEffects.h"
#include "SSAO.h"
#include "FXAA.h"
#include "SystemTime.h"
#include "TextRenderer.h"
#include "ShadowCamera.h"
#include "ParticleEffectManager.h"
#include "GameInput.h"
#include "./ForwardPlusLighting.h"

#include "ART/Animation/AnimatedValue.inl"
#include "ART/Animation/AnimationController.h"

#include "ART/GUI/GUICore.h"
#include "ART/GUI/AnimationWidget.h"
#include "ART/GUI/SequencerWidget.h"

#include "ART/Sequencer/FrameSequencer.h"

// To enable wave intrinsics, uncomment this macro and #define DXIL in Core/GraphcisCore.cpp.
// Run CompileSM6Test.bat to compile the relevant shaders with DXC.
//#define _WAVE_OP

#include "CompiledShaders/DepthViewerVS.h"
#include "CompiledShaders/DepthViewerPS.h"
#include "CompiledShaders/ModelViewerVS.h"
#include "CompiledShaders/ModelViewerPS.h"
#ifdef _WAVE_OP
#include "CompiledShaders/DepthViewerVS_SM6.h"
#include "CompiledShaders/ModelViewerVS_SM6.h"
#include "CompiledShaders/ModelViewerPS_SM6.h"
#endif
#include "CompiledShaders/WaveTileCountPS.h"
// shaders for MSAA
#include "CompiledShaders/MsaaColorResolveCS.h"
#include "CompiledShaders/MsaaDepthResolveCS.h"
// shaders for Upsampling
#include "CompiledShaders/UpsampleColorResolveCS.h"
#include "CompiledShaders/UpsampleDepthResolveCS.h"
// shaders for checkerboard rendering
#include "CompiledShaders/CheckerboardColorResolveCS.h"
#include "CompiledShaders/CheckerboardDepthResolveCS.h"
#include "CompiledShaders/ModelViewerPS_CBR.h"
#include "CompiledShaders/ModelViewerPS_CBR_PP.h"

#include "CompiledShaders/DepthViewerPS_CBR.h"

#include <shellapi.h>

using namespace GameCore;
using namespace Math;
using namespace Graphics;
using namespace ART;

class ModelViewer : public GameCore::IGameApp
{
public:

    ModelViewer(void) {}

    virtual void Startup(void) override;
    virtual void Cleanup(void) override;

    virtual void Update(float deltaT) override;
    virtual void RenderScene(void) override;

    virtual bool IsDone(void) override
    {
        if (false == m_Loaded)
            return true;

        return GameInput::IsFirstPressed(GameInput::kKey_escape);
    }

private:

    void RenderLightShadows(GraphicsContext& gfxContext);

    enum eObjectFilter { kOpaque = 0x1, kCutout = 0x2, kTransparent = 0x4, kAll = 0xF, kNone = 0x0 };
    void RenderObjects(GraphicsContext& Context, const Matrix4& ViewProjMat, eObjectFilter Filter = kAll);
    void CreateParticleEffects();
    void UpdatePlacedResources(GraphicsContext& context);
    float RecalculateResolutionScale();
    void SetFrameBuffers( GraphicsContext& context, int index, int alt_index, bool CbrWasEnabled, bool MsaaWasEnabled, int PrevMsaaMode );
        
    //Camera m_Camera;
    std::auto_ptr<CameraController> m_CameraController;
    Matrix4 m_ViewProjMatrix;
    D3D12_VIEWPORT m_MainViewport;
    D3D12_RECT m_MainScissor;

    D3D12_VIEWPORT m_DownSizedViewport;
    D3D12_RECT m_DownSizedScissor;

    RootSignature m_RootSig;
    GraphicsPSO m_DepthPSO;
    GraphicsPSO m_DepthMsaaPSO;
    GraphicsPSO m_CutoutDepthPSO;
    GraphicsPSO m_CutoutDepthMsaaPSO;
    GraphicsPSO m_ModelPSO;
    GraphicsPSO m_ModelMsaaPSO;
#ifdef _WAVE_OP
    GraphicsPSO m_DepthWaveOpsPSO;
    GraphicsPSO m_ModelWaveOpsPSO;
#endif
    GraphicsPSO m_CutoutModelPSO;
    GraphicsPSO m_CutoutModelMsaaPSO;
    GraphicsPSO m_ShadowPSO;
    GraphicsPSO m_CutoutShadowPSO;
    GraphicsPSO m_WaveTileCountPSO;

    RootSignature m_MsaaResolveRootSig;
    ComputePSO m_MsaaColorResolvePSO;
    ComputePSO m_MsaaDepthResolvePSO;

    RootSignature m_UpsampleResolveRootSig;
    ComputePSO m_UpsampleColorResolvePSO;
    ComputePSO m_UpsampleDepthResolvePSO;

    RootSignature m_CheckerboardResolveRootSig;
    ComputePSO m_CheckerboardColorResolvePSO;
    ComputePSO m_CheckerboardDepthResolvePSO;

    D3D12_CPU_DESCRIPTOR_HANDLE m_DefaultSampler;
    D3D12_CPU_DESCRIPTOR_HANDLE m_ShadowSampler;

    D3D12_CPU_DESCRIPTOR_HANDLE m_ExtraTextures[6];
    std::vector<bool> m_pMaterialIsCutout;

    Scene m_Scene;

    Vector3 m_SunDirection;
    ShadowCamera m_SunShadow;

    SceneAnimation_ptr		m_SceneAnimation;
    AnimationController_ptr	m_AnimationController;
    FrameSequencer			m_Sequencer;

    XMFLOAT2 m_DownSizedFactor;
    int m_FrameOffset;
    bool m_Loaded;
};

CREATE_APPLICATION(ModelViewer)

ExpVar m_SunLightIntensity("Application/Lighting/Sun Light Intensity", 4.0f, 0.0f, 16.0f, 0.1f);
ExpVar m_AmbientIntensity("Application/Lighting/Ambient Intensity", 0.1f, -16.0f, 16.0f, 0.1f);
NumVar m_SunOrientation("Application/Lighting/Sun Orientation", -0.5f, -100.0f, 100.0f, 0.1f);
NumVar m_SunInclination("Application/Lighting/Sun Inclination", 0.75f, 0.0f, 1.0f, 0.01f);
NumVar ShadowDimX("Application/Lighting/Shadow Dim X", 5000, 10, 15000, 50);
NumVar ShadowDimY("Application/Lighting/Shadow Dim Y", 5000, 10, 15000, 50);
NumVar ShadowDimZ("Application/Lighting/Shadow Dim Z", 3000, 10, 10000, 50);

BoolVar ShowWaveTileCounts("Application/Forward+/Show Wave Tile Counts", false);

// MSAA options
BoolVar MsaaEnabled("Application/MSAA/MSAA Enable", false);
const char* MsaaModeLabels[] = { "2x", "4x", "8x" };
EnumVar MsaaMode("Application/MSAA/MSAA Mode", 2, _countof(MsaaModeLabels), MsaaModeLabels);
const char* MsaaResolverLabels[] = { "Auto", "Custom" };
EnumVar MsaaResolver("Application/MSAA/MSAA Resolver", 1, _countof(MsaaResolverLabels), MsaaResolverLabels);

// Checkerboard rendering options (CBR)
BoolVar CbrEnabled("Checkerboard/Enable", true);
BoolVar CbrCheckShadingOcclusion("Checkerboard/Check Shading Occlusion", true);
BoolVar CbrRenderMotionVectors("Checkerboard/Show Derived Motion", false);
BoolVar CbrRenderMissingPixels("Checkerboard/Show Missing Pixels", false);
BoolVar CbrRenderObstructedPixels("Checkerboard/Show Occluded Pixels", false);
BoolVar CbrRenderPixelMotion("Checkerboard/Show Pixel Motion", false);
ExpVar CbrDepthTolerance("Checkerboard/Depth Tolerance", 0.1f, -4.0f, 512.0f, 0.1f);

const char* CbrRenderCheckerPatternLabels[] = { "Both", "Odd", "Even" };
EnumVar CbrRenderCheckerPattern("Checkerboard/Show Checker Pattern", 0, _countof(CbrRenderCheckerPatternLabels), CbrRenderCheckerPatternLabels);

//DRR
BoolVar DrrEnabled("DRR/Enable", true);
BoolVar DrrForceScale("DRR/Force Scale", false);
ExpVar DrrResolutionIncrements("DRR/Resolution Increments", .1f, log2(.01f), log2(1.0f));
IntVar DrrDesiredFrameRate("DRR/Desired Frame Rate", 60, 15, 360, 15 );

ExpVar DrrMinScale("DRR/Min Scale", 0.7f, -3.0f, 0.0f, 0.1f);
ExpVar DrrFrameRateDeltaResolution("DRR/_Advanced/Frame Rate Delta Resolution", .10f, log2(.001f), log2(1.0f) );
ExpVar DrrFrameRateLowPassFilter("DRR/_Advanced/Frame Rate Low Pass K", 0.005f, log2(0.001f), log2(1.0f));
ExpVar DrrRateOfChange("DRR/_Advanced/Rate of Change", .01f, log2(.01f), log2(1.0f) );
ExpVar DrrInternalScale("DRR/_Internal/Scale", 1.0f);
IntVar DrrInternalFrameRate("DRR/_Internal/Frame Rate", 0);

#ifdef _WAVE_OP
BoolVar EnableWaveOps("Application/Forward+/Enable Wave Ops", true);
#endif

bool FindScene(std::wstring *pScenePath, const wchar_t *pExecutableFolder, const wchar_t *pSceneName)
{
    // start at the executable folder and continue to move up folders
    // until we can find a relative path matching pSceneName

    wchar_t path[MAX_PATH];
    wcsncpy_s(path, pExecutableFolder, _countof(path));

    while (true)
    {
        wchar_t *pSlash = wcsrchr(path, '\\');
        if (pSlash == NULL)
            break;

        *pSlash = 0;

        wchar_t newPath[MAX_PATH];
        _snwprintf_s(newPath, _countof(newPath), _TRUNCATE, L"%s\\%s", path, pSceneName);

        DWORD result = GetFileAttributes(newPath);

        if (INVALID_FILE_ATTRIBUTES != result &&
            ERROR_FILE_NOT_FOUND != GetLastError())
        {
            *pScenePath = std::wstring(newPath);
            return true;
        }
    }

    return false;
}

void ModelViewer::Startup(void)
{
    SamplerDesc DefaultSamplerDesc;
    DefaultSamplerDesc.MaxAnisotropy = 8;

    m_RootSig.Reset(6, 2);
    m_RootSig.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
    m_RootSig.InitStaticSampler(1, SamplerShadowDesc, D3D12_SHADER_VISIBILITY_PIXEL);
    m_RootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
    m_RootSig[1].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_PIXEL);
    m_RootSig[2].InitAsConstantBuffer(1, D3D12_SHADER_VISIBILITY_PIXEL);
    m_RootSig[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, Model::kMaterialTexChannelCount(), D3D12_SHADER_VISIBILITY_PIXEL);
    m_RootSig[4].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 64, 6, D3D12_SHADER_VISIBILITY_PIXEL);
    m_RootSig[5].InitAsConstants(1, 2, D3D12_SHADER_VISIBILITY_VERTEX);
    m_RootSig.Finalize(L"ModelViewer", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    m_MsaaResolveRootSig.Reset(3, 0);
    m_MsaaResolveRootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL);
    m_MsaaResolveRootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
    m_MsaaResolveRootSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
    m_MsaaResolveRootSig.Finalize(L"MsaaResolveRS");

    m_UpsampleResolveRootSig.Reset(3, 0);
    m_UpsampleResolveRootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL);
    m_UpsampleResolveRootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
    m_UpsampleResolveRootSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
    m_UpsampleResolveRootSig.Finalize(L"UpsampleResolveRS");

    m_CheckerboardResolveRootSig.Reset(3, 0);
    m_CheckerboardResolveRootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL);
    m_CheckerboardResolveRootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4);
    m_CheckerboardResolveRootSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
    m_CheckerboardResolveRootSig.Finalize(L"CheckerboardResolveRS");

    DXGI_FORMAT ColorFormat = g_pSceneColorBuffer->GetFormat();
    DXGI_FORMAT DepthFormat = g_pSceneDepthBuffer->GetFormat();
    DXGI_FORMAT ShadowFormat = g_ShadowBuffer.GetFormat();

    D3D12_INPUT_ELEMENT_DESC vertElem[] =
    {
         { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Depth-only
    m_DepthPSO.SetRootSignature(m_RootSig);
    m_DepthPSO.SetRasterizerState(RasterizerDefault);
    m_DepthPSO.SetBlendState(BlendNoColorWrite);
    m_DepthPSO.SetDepthStencilState(DepthStateReadWrite);
    m_DepthPSO.SetInputLayout(_countof(vertElem), vertElem);
    m_DepthPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    m_DepthPSO.SetRenderTargetFormats(0, nullptr, DepthFormat);
    m_DepthPSO.SetVertexShader(g_pDepthViewerVS, sizeof(g_pDepthViewerVS));
    m_DepthPSO.Finalize();

    m_DepthMsaaPSO = m_DepthPSO;
    m_DepthMsaaPSO.SetRasterizerState(RasterizerDefaultMsaa);
    m_DepthMsaaPSO.SetPixelShader(g_pDepthViewerPS_CBR, sizeof(g_pDepthViewerPS_CBR));
    m_DepthMsaaPSO.Finalize();

    // Depth-only shading but with alpha testing
    m_CutoutDepthPSO = m_DepthPSO;
    m_CutoutDepthPSO.SetPixelShader(g_pDepthViewerPS, sizeof(g_pDepthViewerPS));
    m_CutoutDepthPSO.SetRasterizerState(RasterizerTwoSided);
    m_CutoutDepthPSO.Finalize();

    m_CutoutDepthMsaaPSO = m_CutoutDepthPSO;
    m_CutoutDepthMsaaPSO.SetRasterizerState(RasterizerTwoSidedMsaa);
    m_CutoutDepthMsaaPSO.SetPixelShader(g_pDepthViewerPS_CBR, sizeof(g_pDepthViewerPS_CBR));
    m_CutoutDepthMsaaPSO.Finalize();

    // Depth-only but with a depth bias and/or render only backfaces
    m_ShadowPSO = m_DepthPSO;
    m_ShadowPSO.SetRasterizerState(RasterizerShadow);
    m_ShadowPSO.SetRenderTargetFormats(0, nullptr, g_ShadowBuffer.GetFormat());
    m_ShadowPSO.Finalize();

    // Shadows with alpha testing
    m_CutoutShadowPSO = m_ShadowPSO;
    m_CutoutShadowPSO.SetPixelShader(g_pDepthViewerPS, sizeof(g_pDepthViewerPS));
    m_CutoutShadowPSO.SetRasterizerState(RasterizerShadowTwoSided);
    m_CutoutShadowPSO.Finalize();

    // Full color pass
    m_ModelPSO = m_DepthPSO;
    m_ModelPSO.SetBlendState(BlendDisable);
    m_ModelPSO.SetDepthStencilState(DepthStateTestEqual);
    m_ModelPSO.SetRenderTargetFormats(1, &ColorFormat, DepthFormat);
    m_ModelPSO.SetVertexShader(g_pModelViewerVS, sizeof(g_pModelViewerVS));
    m_ModelPSO.SetPixelShader(g_pModelViewerPS, sizeof(g_pModelViewerPS));
    m_ModelPSO.Finalize();

    m_ModelMsaaPSO = m_ModelPSO;
    m_ModelMsaaPSO.SetRasterizerState(RasterizerDefaultMsaa);
    m_ModelMsaaPSO.Finalize();

#ifdef _WAVE_OP
    m_DepthWaveOpsPSO = m_DepthPSO;
    m_DepthWaveOpsPSO.SetVertexShader(g_pDepthViewerVS_SM6, sizeof(g_pDepthViewerVS_SM6));
    m_DepthWaveOpsPSO.Finalize();

    m_ModelWaveOpsPSO = m_ModelPSO;
    m_ModelWaveOpsPSO.SetVertexShader(g_pModelViewerVS_SM6, sizeof(g_pModelViewerVS_SM6));
    m_ModelWaveOpsPSO.SetPixelShader(g_pModelViewerPS_SM6, sizeof(g_pModelViewerPS_SM6));
    m_ModelWaveOpsPSO.Finalize();
#endif

    m_CutoutModelPSO = m_ModelPSO;
    m_CutoutModelPSO.SetRasterizerState(RasterizerTwoSided);
    m_CutoutModelPSO.Finalize();

    m_CutoutModelMsaaPSO = m_CutoutModelPSO;
    m_CutoutModelMsaaPSO.SetRasterizerState(RasterizerTwoSidedMsaa);
    m_CutoutModelMsaaPSO.Finalize();

    // A debug shader for counting lights in a tile
    m_WaveTileCountPSO = m_ModelPSO;
    m_WaveTileCountPSO.SetPixelShader(g_pWaveTileCountPS, sizeof(g_pWaveTileCountPS));
    m_WaveTileCountPSO.Finalize();

    // Compute PSOs for MSAA depth and color resolve
    m_MsaaColorResolvePSO.SetRootSignature(m_MsaaResolveRootSig);
    m_MsaaColorResolvePSO.SetComputeShader(g_pMsaaColorResolveCS, sizeof(g_pMsaaColorResolveCS));
    m_MsaaColorResolvePSO.Finalize();

    m_MsaaDepthResolvePSO.SetRootSignature(m_MsaaResolveRootSig);
    m_MsaaDepthResolvePSO.SetComputeShader(g_pMsaaDepthResolveCS, sizeof(g_pMsaaDepthResolveCS));
    m_MsaaDepthResolvePSO.Finalize();

    // Compute PSOs for Upsample dpeth and color resolve
    m_UpsampleColorResolvePSO.SetRootSignature(m_UpsampleResolveRootSig);
    m_UpsampleColorResolvePSO.SetComputeShader(g_pUpsampleColorResolveCS, sizeof(g_pUpsampleColorResolveCS));
    m_UpsampleColorResolvePSO.Finalize();

    m_UpsampleDepthResolvePSO.SetRootSignature(m_UpsampleResolveRootSig);
    m_UpsampleDepthResolvePSO.SetComputeShader(g_pUpsampleDepthResolveCS, sizeof(g_pUpsampleDepthResolveCS));
    m_UpsampleDepthResolvePSO.Finalize();

    // Compute PSOs for Checkerboard depth and color resolve
    m_CheckerboardColorResolvePSO.SetRootSignature(m_CheckerboardResolveRootSig);
    m_CheckerboardColorResolvePSO.SetComputeShader(g_pCheckerboardColorResolveCS, sizeof(g_pCheckerboardColorResolveCS));
    m_CheckerboardColorResolvePSO.Finalize();

    m_CheckerboardDepthResolvePSO.SetRootSignature(m_CheckerboardResolveRootSig);
    m_CheckerboardDepthResolvePSO.SetComputeShader(g_pCheckerboardDepthResolveCS, sizeof(g_pCheckerboardDepthResolveCS));
    m_CheckerboardDepthResolvePSO.Finalize();

    Lighting::InitializeResources();

    m_ExtraTextures[0] = g_SSAOFullScreen.GetSRV();
    m_ExtraTextures[1] = g_ShadowBuffer.GetSRV();

    std::wstring wScenePath;

    int nArgs = 0;
    LPWSTR* argList = CommandLineToArgvW(GetCommandLineW(), &nArgs);

    m_Loaded = false;

    if (nArgs < 2)
    {
        bool result = FindScene(&wScenePath, argList[0], L"Sponza\\sponza.scn");

        if (false == result)
        {
            Utility::Print("Expected: a scene file path as input argument 1, or Sponza\\sponza.scn in the parent directory structure.\r\n");

            MessageBox(GetForegroundWindow(),
                L"Expected: a scene file path as input argument 1, "
                L"or Sponza\\sponza.scn in the parent directory structure.",
                L"Could not find Scene", MB_ICONERROR | MB_OK);

            return;
        }
    }
    else
        wScenePath = std::wstring(argList[1]);

    std::string scenepath(wScenePath.begin(), wScenePath.end());

    LocalFree(argList);

    ASSERT(m_Scene.LoadJson(scenepath.c_str()));
    auto& model = m_Scene.GetModel();

    // The caller of this function can override which materials are considered cutouts
    m_pMaterialIsCutout.resize(model.m_Header.materialCount);
    for (uint32_t i = 0; i < model.m_Header.materialCount; ++i)
    {
        const Model::Material& mat = model.m_pMaterial[i];
        if (mat.hasMask)
        {
            m_pMaterialIsCutout[i] = true;
        }
        else
        {
            m_pMaterialIsCutout[i] = false;
        }
    }

    //CreateParticleEffects();

    float modelRadius = m_Scene.GetModelRadius();
    m_CameraController.reset(new CameraController(m_Scene.GetCamera(), Vector3(kYUnitVector)));
    m_CameraController->SetSpeed(0.25f * modelRadius);

    const Vector3& sceneShadowDim = m_Scene.GetShadowDim();
    if (sceneShadowDim.GetX() > 0.0f) ShadowDimX = sceneShadowDim.GetX();
    if (sceneShadowDim.GetY() > 0.0f) ShadowDimY = sceneShadowDim.GetY();
    if (sceneShadowDim.GetZ() > 0.0f) ShadowDimZ = sceneShadowDim.GetZ();

    // KX: turn off motion blur, hdr, and ssao by default.
    // KX: also turn off TAA by default to immediately show CB effects (image quality)
    //MotionBlur::Enable = false;
    //TemporalEffects::EnableTAA = false;
    //FXAA::Enable = false;
    PostEffects::EnableHDR = false;
    //PostEffects::EnableAdaptation = false;
    //SSAO::Enable = false;

    // initialize DownSized Factors
    m_DownSizedFactor.x = 1.f; m_DownSizedFactor.y = 1.f;

    Lighting::CreateRandomLights(model.GetBoundingBox().min, model.GetBoundingBox().max);

    m_ExtraTextures[2] = Lighting::m_LightBuffer.GetSRV();
    m_ExtraTextures[3] = Lighting::m_LightShadowArray.GetSRV();
    m_ExtraTextures[4] = Lighting::m_LightGrid.GetSRV();
    m_ExtraTextures[5] = Lighting::m_LightGridBitMask.GetSRV();

    m_AnimationController = std::make_shared<AnimationController>();
    m_AnimationController->SetSceneAnimation(m_Scene.GetAnimation());
    m_AnimationController->SetTargetCamera(&m_Scene.GetCamera());

    m_AnimationController->AnimateFloatVar(&m_SunOrientation, "SunOrientation");
    m_AnimationController->AnimateBoolVar(&ShowWaveTileCounts, "ShowWaveTileCounts");

    // TODO: DRR - This might not work with our aliased placed resources
    m_Sequencer.SetSrcBuffer(Graphics::g_pSceneColorBuffer);
    m_Sequencer.SetDstRootFolder(m_Scene.GetRootFolder());

#ifdef ART_ENABLE_GUI
    auto animWidget = ARTGUI::AnimationWidget::Create(m_AnimationController);
    auto sequencerWidget = ARTGUI::SequencerWidget::Create(&m_Sequencer, m_AnimationController);
    ARTGUI::AddWidget(animWidget);
    ARTGUI::AddWidget(sequencerWidget);
#endif

    m_Loaded = true;

    return;
}

void ModelViewer::Cleanup(void)
{
    if (m_Loaded)
    {
        m_Scene.SaveAnimation();
        m_Scene.Cleanup();

        Lighting::Shutdown();
    }
}

namespace Graphics
{
    extern EnumVar DebugZoom;
}

void ModelViewer::Update(float deltaT)
{
    {	// setting downsize factors for jitter and viewport scaling
        if (CbrEnabled) {
            m_FrameOffset = Graphics::GetFrameCount() % 2;

            // downsize to quarter resolution in CBR 2x and 4x (2x MSAA and 1xMSAA, respectively)
            m_DownSizedFactor.x = 2.f; m_DownSizedFactor.y = 2.f;
        }
        else {
            // reset to default
            m_DownSizedFactor.x = 1.f; m_DownSizedFactor.y = 1.f;
        }
    }

    ScopedTimer _prof(L"Update State");

    if (GameInput::IsFirstPressed(GameInput::kLShoulder))
        DebugZoom.Decrement();
    else if (GameInput::IsFirstPressed(GameInput::kRShoulder))
        DebugZoom.Increment();

    // grab screenshot?
    if (GameInput::IsFirstPressed(GameInput::kKey_sysrq)) {
        Utility::Print("Capturing screenshot\n");
        m_Sequencer.CaptureOne();
    }

    m_AnimationController->Update(deltaT);
    if (m_AnimationController->IsDirty()) {
        m_CameraController->Reset();
        m_CameraController->Update(0);
        m_AnimationController->SetDirty(false);
    }
    else
        m_CameraController->Update(deltaT);

    auto& camera = m_Scene.GetCamera();

    m_ViewProjMatrix = camera.GetViewProjMatrix();

    float costheta = cosf(m_SunOrientation);
    float sintheta = sinf(m_SunOrientation);
    float cosphi = cosf(m_SunInclination * 3.14159f * 0.5f);
    float sinphi = sinf(m_SunInclination * 3.14159f * 0.5f);
    m_SunDirection = Normalize(Vector3(costheta * cosphi, sinphi, sintheta * cosphi));
}

void ModelViewer::RenderObjects(GraphicsContext& gfxContext, const Matrix4& ViewProjMat, eObjectFilter Filter)
{
    struct VSConstants
    {
        Matrix4 modelToProjection;
        Matrix4 modelToShadow;
        XMFLOAT3 viewerPos;
    } vsConstants;

    vsConstants.modelToProjection = ViewProjMat;
    vsConstants.modelToShadow = m_SunShadow.GetShadowMatrix();
    XMStoreFloat3(&vsConstants.viewerPos, m_Scene.GetCamera().GetPosition());
    gfxContext.SetDynamicConstantBufferView(0, sizeof(vsConstants), &vsConstants);

    uint32_t materialIdx = 0xFFFFFFFFul;

    auto& model = m_Scene.GetModel();

    uint32_t VertexStride = model.m_VertexStride;

    gfxContext.SetDynamicDescriptors(3, Model::kMaterialTexChannelCount() - 1, 1, &model.m_MaterialConstants.GetSRV());

    for (uint32_t meshIndex = 0; meshIndex < model.m_Header.meshCount; meshIndex++)
    {
        const Model::Mesh& mesh = model.m_pMesh[meshIndex];

        uint32_t indexCount = mesh.indexCount;
        uint32_t startIndex = mesh.indexDataByteOffset / sizeof(uint16_t);
        uint32_t baseVertex = mesh.vertexDataByteOffset / VertexStride;

        if (mesh.materialIndex != materialIdx)
        {
            if (m_pMaterialIsCutout[mesh.materialIndex] && !(Filter & kCutout) ||
                !m_pMaterialIsCutout[mesh.materialIndex] && !(Filter & kOpaque))
                continue;

            materialIdx = mesh.materialIndex;
            gfxContext.SetDynamicDescriptors(3, 0, Model::kMaterialTexChannelCount() - 1, model.GetSRVs(materialIdx));
            __declspec(align(16)) struct {
                UINT32 vmaterialIdx;
            } psMaterialConstants;
            psMaterialConstants.vmaterialIdx = materialIdx;
            gfxContext.SetDynamicConstantBufferView(2, sizeof(psMaterialConstants), &psMaterialConstants);
        }

        gfxContext.SetConstants(5, baseVertex, materialIdx);

        gfxContext.DrawIndexed(indexCount, startIndex, baseVertex);
    }
}

void ModelViewer::RenderLightShadows(GraphicsContext& gfxContext)
{
    using namespace Lighting;

    ScopedTimer _prof(L"RenderLightShadows", gfxContext);

    static uint32_t LightIndex = 0;
    if (LightIndex >= MaxLights)
        return;

    m_LightShadowTempBuffer.BeginRendering(gfxContext);
    {
        gfxContext.SetPipelineState(m_ShadowPSO);
        RenderObjects(gfxContext, m_LightShadowMatrix[LightIndex], kOpaque);
        gfxContext.SetPipelineState(m_CutoutShadowPSO);
        RenderObjects(gfxContext, m_LightShadowMatrix[LightIndex], kCutout);
    }
    m_LightShadowTempBuffer.EndRendering(gfxContext);

    gfxContext.TransitionResource(m_LightShadowTempBuffer, D3D12_RESOURCE_STATE_GENERIC_READ);
    gfxContext.TransitionResource(m_LightShadowArray, D3D12_RESOURCE_STATE_COPY_DEST);

    gfxContext.CopySubresource(m_LightShadowArray, LightIndex, m_LightShadowTempBuffer, 0);

    gfxContext.TransitionResource(m_LightShadowArray, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    ++LightIndex;
}

void ModelViewer::RenderScene(void)
{
    GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");

    UpdatePlacedResources( gfxContext );


    // We use viewport offsets to jitter sample positions from frame to frame (for TAA.)
    // D3D has a design quirk with fractional offsets such that the implicit scissor
    // region of a viewport is floor(TopLeftXY) and floor(TopLeftXY + WidthHeight), so
    // having a negative fractional top left, e.g. (-0.25, -0.25) would also shift the
    // BottomRight corner up by a whole integer.  One solution is to pad your viewport
    // dimensions with an extra pixel.  My solution is to only use positive fractional offsets,
    // but that means that the average sample position is +0.5, which I use when I disable
    // temporal AA.
    TemporalEffects::GetJitterOffset(m_MainViewport.TopLeftX, m_MainViewport.TopLeftY);

    m_MainViewport.Width = (float) g_pSceneColorBuffer->GetWidth();
    m_MainViewport.Height = (float) g_pSceneColorBuffer->GetHeight();
    m_MainViewport.MinDepth = 0.0f;
    m_MainViewport.MaxDepth = 1.0f;

    m_MainScissor.left = 0;
    m_MainScissor.top = 0;
    m_MainScissor.right = (LONG) m_MainViewport.Width;
    m_MainScissor.bottom = (LONG) m_MainViewport.Height;

    if (CbrEnabled) {
        // scale viewport and jitter vectors
        XMFLOAT2 Jitter;
        TemporalEffects::GetJitterOffset(Jitter.x, Jitter.y);

        m_DownSizedViewport.TopLeftX = Jitter.x / m_DownSizedFactor.x;
        m_DownSizedViewport.TopLeftY = Jitter.y / m_DownSizedFactor.y;

        m_DownSizedViewport.TopLeftX += .5f * m_FrameOffset;

        m_DownSizedViewport.Width = (float) g_pSceneColorBuffer->GetWidth() / m_DownSizedFactor.x;
        m_DownSizedViewport.Height = (float) g_pSceneColorBuffer->GetHeight() / m_DownSizedFactor.y;
        m_DownSizedViewport.MinDepth = 0.0f;
        m_DownSizedViewport.MaxDepth = 1.0f;

        m_DownSizedScissor.left = 0;
        m_DownSizedScissor.top = 0;
        m_DownSizedScissor.right = (LONG) (g_pSceneColorBuffer->GetWidth() / m_DownSizedFactor.x);
        m_DownSizedScissor.bottom = (LONG) (g_pSceneColorBuffer->GetHeight() / m_DownSizedFactor.y);
    }



    if (CbrEnabled) {
        m_FrameOffset = Graphics::GetFrameCount() % 2;
        m_DownSizedFactor.x = 2.f; m_DownSizedFactor.y = 2.f;
    }

    static bool s_ShowLightCounts = false;
    if (ShowWaveTileCounts != s_ShowLightCounts)
    {
        static bool EnableHDR;
        if (ShowWaveTileCounts)
        {
            EnableHDR = PostEffects::EnableHDR;
            PostEffects::EnableHDR = false;
        }
        else
        {
            PostEffects::EnableHDR = EnableHDR;
        }
        s_ShowLightCounts = ShowWaveTileCounts;
    }


    ParticleEffects::Update(gfxContext.GetComputeContext(), Graphics::GetFrameTime());

    uint32_t FrameIndex = TemporalEffects::GetFrameIndexMod2();

    __declspec(align(16)) struct
    {
        Vector3 sunDirection;
        Vector3 sunLight;
        Vector3 ambientLight;
        float ShadowTexelSize[4];

        float InvTileDim[4];
        uint32_t TileCount[4];
        uint32_t FirstLightIndex[4];
        float DownSizedFactors[4];

    } psConstants;

    unsigned int flags = 0;

    if (m_FrameOffset)
        flags |= 0x01;

    if (Graphics::g_IntelIsGPU)
        flags |= 0x02;

    psConstants.sunDirection = m_SunDirection;
    psConstants.sunLight = Vector3(1.0f, 1.0f, 1.0f) * m_SunLightIntensity;
    psConstants.ambientLight = Vector3(1.0f, 1.0f, 1.0f) * m_AmbientIntensity;
    psConstants.ShadowTexelSize[0] = 1.0f / g_ShadowBuffer.GetWidth();
    psConstants.InvTileDim[0] = 1.0f / Lighting::LightGridDim;
    psConstants.InvTileDim[1] = 1.0f / Lighting::LightGridDim;
    psConstants.TileCount[0] = Math::DivideByMultiple(g_pSceneColorBuffer->GetWidth(), Lighting::LightGridDim);
    psConstants.TileCount[1] = Math::DivideByMultiple(g_pSceneColorBuffer->GetHeight(), Lighting::LightGridDim);
    psConstants.FirstLightIndex[0] = Lighting::m_FirstConeLight;
    psConstants.FirstLightIndex[1] = Lighting::m_FirstConeShadowedLight;
    psConstants.DownSizedFactors[0] = m_DownSizedFactor.x;
    psConstants.DownSizedFactors[1] = m_DownSizedFactor.y;
    psConstants.DownSizedFactors[2] = *(float *) (int *) &flags;

    auto& model = m_Scene.GetModel();
    auto& camera = m_Scene.GetCamera();

    // Set the default state for command lists
    auto& pfnSetupGraphicsState = [&](void)
    {
        gfxContext.SetRootSignature(m_RootSig);
        gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        gfxContext.SetIndexBuffer(model.m_IndexBuffer.IndexBufferView());
        gfxContext.SetVertexBuffer(0, model.m_VertexBuffer.VertexBufferView());
    };

    pfnSetupGraphicsState();
    RenderLightShadows(gfxContext);

    // -- Depth prepass Normal,Upsampling,CBR
    {
        if (!CbrEnabled) {

            ScopedTimer _prof(L"Z PrePass", gfxContext);
            gfxContext.SetDynamicConstantBufferView(1, sizeof(psConstants), &psConstants);
            if (!MsaaEnabled)
            {
                {
                    ScopedTimer _prof(L"Opaque", gfxContext);
                    gfxContext.TransitionResource(*g_pSceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
                    gfxContext.ClearDepth(*g_pSceneDepthBuffer);
#ifdef _WAVE_OP
                    gfxContext.SetPipelineState(EnableWaveOps ? m_DepthWaveOpsPSO : m_DepthPSO);
#else
                    gfxContext.SetPipelineState(m_DepthPSO);
#endif
                    gfxContext.SetDepthStencilTarget(g_pSceneDepthBuffer->GetDSV());

                    gfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);
                    RenderObjects(gfxContext, m_ViewProjMatrix, kOpaque);
                }

                {
                    ScopedTimer _prof(L"Cutout", gfxContext);
                    gfxContext.SetPipelineState(m_CutoutDepthPSO);
                    RenderObjects(gfxContext, m_ViewProjMatrix, kCutout);
                }
            }

            if (MsaaEnabled) {
                ScopedTimer _prof(L"OpaqueMSAA", gfxContext);

                gfxContext.TransitionResource(*g_pMsaaDepth, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
                gfxContext.ClearDepth(*g_pMsaaDepth);
#ifdef _WAVE_OP
                gfxContext.SetPipelineState(EnableWaveOps ? m_DepthWaveOpsPSO : m_DepthMsaaPSO);
#else
                m_DepthMsaaPSO.SetMsaaCount((UINT) std::pow((UINT) 2, (UINT) MsaaMode));
                m_DepthMsaaPSO.Finalize();
                gfxContext.SetPipelineState(m_DepthMsaaPSO);
#endif
                gfxContext.SetDepthStencilTarget((*g_pMsaaDepth).GetDSV());

                gfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);
                RenderObjects(gfxContext, m_ViewProjMatrix, kOpaque);
                {
                    ScopedTimer _prof(L"CutoutMSAA", gfxContext);
                    m_CutoutDepthMsaaPSO.SetMsaaCount((UINT) std::pow((UINT) 2, (UINT) MsaaMode));
                    m_CutoutDepthMsaaPSO.Finalize();
                    gfxContext.SetPipelineState(m_CutoutDepthMsaaPSO);
                    RenderObjects(gfxContext, m_ViewProjMatrix, kCutout);
                }
            }

            if (MsaaEnabled) {
                // explicitly resolve MSAA depth
                const float zMagic = (camera.GetFarClip() - camera.GetNearClip()) / camera.GetNearClip();
                __declspec(align(16)) float cbData[] = {
                    zMagic, (float) 1
                };

                uint32_t FrameIndex = TemporalEffects::GetFrameIndexMod2();
                ColorBuffer *LinearDepth = &g_LinearDepth[Graphics::GetFrameCount() % 2];

                ComputeContext& Context = gfxContext.GetComputeContext();
                Context.SetRootSignature(m_MsaaResolveRootSig);
                Context.SetPipelineState(m_MsaaDepthResolvePSO);
                Context.TransitionResource(*g_pMsaaDepth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                Context.TransitionResource(*LinearDepth, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                Context.SetDynamicConstantBufferView(0, sizeof(cbData), cbData);
                Context.SetDynamicDescriptor(1, 0, (*g_pMsaaDepth).GetDepthSRV());
                Context.SetDynamicDescriptor(2, 0, (*LinearDepth).GetUAV());
                Context.Dispatch2D(g_pMsaaDepth->GetWidth(), g_pMsaaDepth->GetHeight());

                // We convert linear depth here, but we still need SSAO to get ambient lighting
                SSAO::ComputeLinearZ = false;
            }
            else {
                SSAO::ComputeLinearZ = true;
            }
        }
        else {
            // render to downsized buffer and upsample using compute shader with checkerboard rendering
            ScopedTimer _prof(L"Z PrePass Checkerboard", gfxContext);
            gfxContext.SetDynamicConstantBufferView(1, sizeof(psConstants), &psConstants);
            {
                ScopedTimer _prof(L"Opaque Checkerboard", gfxContext);
                gfxContext.TransitionResource(*g_pCheckerboardDepths[m_FrameOffset], D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
                gfxContext.ClearDepth(*g_pCheckerboardDepths[m_FrameOffset]);
#ifdef _WAVE_OP
                gfxContext.SetPipelineState(EnableWaveOps ? m_DepthWaveOpsPSO : m_DepthMsaaPSO);
#else
                m_DepthMsaaPSO.SetMsaaCount(2);	// 2x MSAA for CBR2x
                m_DepthMsaaPSO.Finalize();
                gfxContext.SetPipelineState(m_DepthMsaaPSO);
#endif
                gfxContext.SetDepthStencilTarget((*g_pCheckerboardDepths[m_FrameOffset]).GetDSV());
                gfxContext.SetViewportAndScissor(m_DownSizedViewport, m_DownSizedScissor);
                RenderObjects(gfxContext, m_ViewProjMatrix, kOpaque);

                {
                    ScopedTimer _prof(L"Cutout Checkerboard", gfxContext);
                    m_CutoutDepthMsaaPSO.SetMsaaCount(2);
                    m_CutoutDepthMsaaPSO.Finalize();
                    gfxContext.SetPipelineState(m_CutoutDepthMsaaPSO);

                    RenderObjects(gfxContext, m_ViewProjMatrix, kCutout);
                }

                {
                    // Resolve the DownSized Depth to Full size depth buffer in linear space
                    const float zMagic = (camera.GetFarClip() - camera.GetNearClip()) / camera.GetNearClip();
                    __declspec(align(16)) float cbData[] = {
                        zMagic, (float) m_FrameOffset
                    };
                    ColorBuffer *LinearDepth = &g_LinearDepth[Graphics::GetFrameCount() % 2];

                    ComputeContext& Context = gfxContext.GetComputeContext();

                    Context.SetRootSignature(m_CheckerboardResolveRootSig);
                    Context.SetPipelineState(m_CheckerboardDepthResolvePSO);

                    Context.TransitionResource(*g_pCheckerboardDepths[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                    Context.TransitionResource(*g_pCheckerboardDepths[1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                    Context.TransitionResource(*LinearDepth, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                    Context.SetDynamicConstantBufferView(0, sizeof(cbData), cbData);

                    // resolution just changed, our previous frame is invalid
                    if ( TemporalEffects::TriggerReset )
                    {
                        Context.SetDynamicDescriptor(1, 0, (*g_pCheckerboardDepths[m_FrameOffset]).GetDepthSRV());
                        Context.SetDynamicDescriptor(1, 1, (*g_pCheckerboardDepths[m_FrameOffset]).GetDepthSRV());
                    }
                    else
                    {
                        Context.SetDynamicDescriptor(1, 0, (*g_pCheckerboardDepths[0]).GetDepthSRV());
                        Context.SetDynamicDescriptor(1, 1, (*g_pCheckerboardDepths[1]).GetDepthSRV());
                    }

                    Context.SetDynamicDescriptor(2, 0, (*LinearDepth).GetUAV());
                    Context.Dispatch2D(LinearDepth->GetWidth(), LinearDepth->GetHeight());

                    // We convert linear depth here, but we still need SSAO to get ambient lighting
                    SSAO::ComputeLinearZ = false;
                    gfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);
                }
            }
        }
    }


    SSAO::Render(gfxContext, camera);

    Lighting::FillLightGrid(gfxContext, camera);

    // -- Primary shading pass Normal, Upsampling,CBR
    {
        if (!SSAO::DebugDraw)
        {
            ScopedTimer _prof(L"Main Render", gfxContext);

            gfxContext.TransitionResource(*g_pSceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
            gfxContext.ClearColor(*g_pSceneColorBuffer);

            pfnSetupGraphicsState();

            {
                ScopedTimer _prof(L"Render Shadow Map", gfxContext);

                m_SunShadow.UpdateMatrix(-m_SunDirection, m_Scene.GetCenter() - m_SunDirection * 0.35f *m_Scene.GetModelRadius(), Vector3(ShadowDimX, ShadowDimY, ShadowDimZ),
                    (uint32_t) g_ShadowBuffer.GetWidth(), (uint32_t) g_ShadowBuffer.GetHeight(), 16);
                //m_SunShadow.UpdateMatrix(-m_SunDirection, Vector3(0, -500.0f, 0), Vector3(ShadowDimX, ShadowDimY, ShadowDimZ),
                //	(uint32_t)g_ShadowBuffer.GetWidth(), (uint32_t)g_ShadowBuffer.GetHeight(), 16);

                g_ShadowBuffer.BeginRendering(gfxContext);
                gfxContext.SetPipelineState(m_ShadowPSO);
                RenderObjects(gfxContext, m_SunShadow.GetViewProjMatrix(), kOpaque);
                gfxContext.SetPipelineState(m_CutoutShadowPSO);
                RenderObjects(gfxContext, m_SunShadow.GetViewProjMatrix(), kCutout);
                g_ShadowBuffer.EndRendering(gfxContext);
            }

            if (SSAO::AsyncCompute)
            {
                gfxContext.Flush();
                pfnSetupGraphicsState();

                // Make the 3D queue wait for the Compute queue to finish SSAO
                g_CommandManager.GetGraphicsQueue().StallForProducer(g_CommandManager.GetComputeQueue());
            }

            {
                ScopedTimer _prof(L"Render Color", gfxContext);
                ScopedPipelineQuery _colorQuery(L"ColorStats", gfxContext);

                gfxContext.TransitionResource(g_SSAOFullScreen, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

                gfxContext.SetDynamicDescriptors(4, 0, _countof(m_ExtraTextures), m_ExtraTextures);
                gfxContext.SetDynamicConstantBufferView(1, sizeof(psConstants), &psConstants);
#ifdef _WAVE_OP
                gfxContext.SetPipelineState(EnableWaveOps ? m_ModelWaveOpsPSO : m_ModelPSO);
#else
                m_ModelPSO.SetPixelShader(g_pModelViewerPS, sizeof(g_pModelViewerPS));
                m_ModelPSO.Finalize();
                gfxContext.SetPipelineState(ShowWaveTileCounts ? m_WaveTileCountPSO : m_ModelPSO);
#endif
                if (!CbrEnabled) {
                    if (MsaaEnabled) {
                        m_ModelMsaaPSO.SetMsaaCount((UINT) std::pow((UINT) 2, (UINT) MsaaMode));
                        m_ModelMsaaPSO.SetPixelShader(g_pModelViewerPS, sizeof(g_pModelViewerPS));
                        m_ModelMsaaPSO.Finalize();
                        gfxContext.SetPipelineState(m_ModelMsaaPSO);
                        if (ShowWaveTileCounts) {
                            m_WaveTileCountPSO.SetMsaaCount((UINT) std::pow((UINT) 2, (UINT) MsaaMode));
                            m_WaveTileCountPSO.SetRasterizerState(RasterizerDefaultMsaa);
                            m_WaveTileCountPSO.Finalize();
                            gfxContext.SetPipelineState(m_WaveTileCountPSO);
                        }
                        gfxContext.TransitionResource(*g_pMsaaColor, D3D12_RESOURCE_STATE_RENDER_TARGET, 1);
                        gfxContext.ClearColor(*g_pMsaaColor);
                        gfxContext.TransitionResource(*g_pMsaaDepth, D3D12_RESOURCE_STATE_DEPTH_READ);
                        gfxContext.SetRenderTarget((*g_pMsaaColor).GetRTV(), (*g_pMsaaDepth).GetDSV_DepthReadOnly());
                    }
                    else {
                        gfxContext.TransitionResource(*g_pSceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
                        gfxContext.TransitionResource(*g_pSceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
                        gfxContext.SetRenderTarget(g_pSceneColorBuffer->GetRTV(), g_pSceneDepthBuffer->GetDSV_DepthReadOnly());
                    }

                    gfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);
                }
                else {
                    m_ModelMsaaPSO.SetMsaaCount(2);
                    m_ModelMsaaPSO.SetPixelShader(g_pModelViewerPS_CBR, sizeof(g_pModelViewerPS_CBR));
                    m_ModelMsaaPSO.Finalize();
                    gfxContext.SetPipelineState(m_ModelMsaaPSO);

                    if (ShowWaveTileCounts) {
                        m_WaveTileCountPSO.SetRasterizerState(RasterizerDefault);
                        m_WaveTileCountPSO.Finalize();
                        gfxContext.SetPipelineState(m_WaveTileCountPSO);
                    }
                    gfxContext.TransitionResource(*g_pCheckerboardColors[m_FrameOffset], D3D12_RESOURCE_STATE_RENDER_TARGET, 1);
                    gfxContext.ClearColor(*g_pCheckerboardColors[m_FrameOffset]);
                    gfxContext.TransitionResource(*g_pCheckerboardDepths[m_FrameOffset], D3D12_RESOURCE_STATE_DEPTH_READ);
                    gfxContext.SetRenderTarget((*g_pCheckerboardColors[m_FrameOffset]).GetRTV(), (*g_pCheckerboardDepths[m_FrameOffset]).GetDSV_DepthReadOnly());
                    gfxContext.SetViewportAndScissor(m_DownSizedViewport, m_DownSizedScissor);
                }
                // Shade the scene with color
                RenderObjects(gfxContext, m_ViewProjMatrix, kOpaque);

                if (!ShowWaveTileCounts)	// shade cutout of scene with color
                {
                    if (!CbrEnabled) {
                        if (MsaaEnabled) {
                            m_CutoutModelMsaaPSO.SetMsaaCount((UINT) std::pow((UINT) 2, (UINT) MsaaMode));
                            m_CutoutModelMsaaPSO.SetPixelShader(g_pModelViewerPS, sizeof(g_pModelViewerPS));
                            m_CutoutModelMsaaPSO.Finalize();
                            gfxContext.SetPipelineState(m_CutoutModelMsaaPSO);
                        }
                        else {
                            m_CutoutModelPSO.SetPixelShader(g_pModelViewerPS, sizeof(g_pModelViewerPS));
                            m_CutoutModelPSO.Finalize();
                            gfxContext.SetPipelineState(m_CutoutModelPSO);
                        }
                    }
                    else {
                        m_CutoutModelMsaaPSO.SetMsaaCount(2);
                        m_CutoutModelMsaaPSO.SetPixelShader(g_pModelViewerPS_CBR, sizeof(g_pModelViewerPS_CBR));
                        m_CutoutModelMsaaPSO.Finalize();
                        gfxContext.SetPipelineState(m_CutoutModelMsaaPSO);
                    }
                    RenderObjects(gfxContext, m_ViewProjMatrix, kCutout);
                }

                // Resolve color to output frame
                if (!CbrEnabled) {
                    if (MsaaEnabled) {
                        if (MsaaResolver == 1) {
                            // use MsaaColorResolveCS to resolve MSAA texture to non-MSAA texture
                            ScopedTimer _prof(L"MsaaColorResolve", gfxContext);
                            ComputeContext& Context = gfxContext.GetComputeContext();
                            Context.SetRootSignature(m_MsaaResolveRootSig);
                            Context.SetPipelineState(m_MsaaColorResolvePSO);
                            Context.TransitionResource(*g_pMsaaColor, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                            Context.TransitionResource(*g_pSceneColorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                            Context.SetDynamicDescriptor(1, 0, (*g_pMsaaColor).GetSRV());
                            Context.SetDynamicDescriptor(2, 0, g_pSceneColorBuffer->GetUAV());
                            Context.Dispatch2D(g_pSceneColorBuffer->GetWidth(), g_pSceneColorBuffer->GetHeight());
                        }
                        else {
                            if (MsaaMode > 0) {
                                gfxContext.TransitionResource(*g_pMsaaColor, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
                                gfxContext.TransitionResource(*g_pSceneColorBuffer, D3D12_RESOURCE_STATE_RESOLVE_DEST);
                                gfxContext.ResolveSubresource(*g_pSceneColorBuffer, 0, *g_pMsaaColor, 0, g_pSceneColorBuffer->GetFormat());
                            }
                            else {
                                gfxContext.TransitionResource(*g_pMsaaColor, D3D12_RESOURCE_STATE_COPY_SOURCE);
                                gfxContext.TransitionResource(*g_pSceneColorBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
                                gfxContext.CopySubresource(*g_pSceneColorBuffer, 0, *g_pMsaaColor, 0);
                            }
                        }
                    }
                }
                else {
                    static Matrix4 PrevInvViewProj = Math::Invert(camera.GetViewProjMatrix());
                    static uint64_t FrameCount;

                    struct __declspec(align(16)) CBData
                    {
                        float FrameOffset;
                        float DepthTolerance;
                        uint32_t Flags;
                        float _Pad;
                        float LinearZTransform[4];
                        float CurrViewProj[16];
                        float PrevInvViewProj[16];
                    };

                    // if our prev frame + 1 doesn't match the actual frame
                    // then we've been skipped and we're now being toggled back on
                    bool CbrToggledOn = (FrameCount + 1) != Graphics::GetFrameCount( );

                    CBData cbData;
                    cbData.DepthTolerance = CbrDepthTolerance;
                    cbData.FrameOffset = (float) m_FrameOffset;
                    cbData.Flags = 0;
                    cbData.Flags |= CbrRenderMotionVectors ? 0x01 : 0;
                    cbData.Flags |= CbrRenderMissingPixels ? 0x02 : 0;
                    cbData.Flags |= CbrRenderPixelMotion ? 0x04 : 0;
                    cbData.Flags |= (CbrRenderCheckerPattern == 1) ? 0x08 : 0;
                    cbData.Flags |= (CbrRenderCheckerPattern == 2) ? 0x10 : 0;
                    cbData.Flags |= CbrRenderObstructedPixels ? 0x20 : 0;
                    cbData.Flags |= CbrCheckShadingOcclusion ? 0x40 : 0;
                    cbData.Flags |= TemporalEffects::TriggerReset ? 0x80: 0;
                    cbData.Flags |= CbrToggledOn ? 0x80: 0;

                    FrameCount = Graphics::GetFrameCount( );

                    std::memcpy(cbData.CurrViewProj, &Math::Transpose(camera.GetViewProjMatrix()), sizeof(cbData.CurrViewProj));
                    std::memcpy(cbData.PrevInvViewProj, &Math::Transpose(PrevInvViewProj), sizeof(cbData.PrevInvViewProj));

                    Math::Matrix4 InvViewProj = Math::Invert(camera.GetViewProjMatrix());
                    cbData.LinearZTransform[0] = InvViewProj.GetZ().GetZ();
                    cbData.LinearZTransform[1] = InvViewProj.GetW().GetZ();
                    cbData.LinearZTransform[2] = InvViewProj.GetZ().GetW();
                    cbData.LinearZTransform[3] = InvViewProj.GetW().GetW();

                    PrevInvViewProj = Math::Invert(camera.GetViewProjMatrix());

                    ScopedTimer _prof(L"CheckerboardColorResolve", gfxContext);
                    ComputeContext& Context = gfxContext.GetComputeContext();
                    Context.SetRootSignature(m_CheckerboardResolveRootSig);
                    Context.SetPipelineState(m_CheckerboardColorResolvePSO);

                    Context.TransitionResource(*g_pCheckerboardColors[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                    Context.TransitionResource(*g_pCheckerboardColors[1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                    Context.TransitionResource(*g_pCheckerboardDepths[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                    Context.TransitionResource(*g_pCheckerboardDepths[1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                    Context.TransitionResource(*g_pSceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                    Context.SetDynamicConstantBufferView(0, sizeof(cbData), &cbData);
                    Context.SetDynamicDescriptor(1, 0, (*g_pCheckerboardColors[0]).GetSRV());
                    Context.SetDynamicDescriptor(1, 1, (*g_pCheckerboardColors[1]).GetSRV());

                    Context.SetDynamicDescriptor(1, 2, (*g_pCheckerboardDepths[0]).GetDepthSRV());
                    Context.SetDynamicDescriptor(1, 3, (*g_pCheckerboardDepths[1]).GetDepthSRV());

                    Context.SetDynamicDescriptor(2, 0, g_pSceneColorBuffer->GetUAV());
                    Context.Dispatch2D(g_pSceneColorBuffer->GetWidth(), g_pSceneColorBuffer->GetHeight());

                    gfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);
                }
            }
        }
    }
    // Some systems generate a per-pixel velocity buffer to better track dynamic and skinned meshes.  Everything
    // is static in our scene, so we generate velocity from camera motion and the depth buffer.  A velocity buffer
    // is necessary for all temporal effects (and motion blur).	
    // Set "true" to use Linear Z
    MotionBlur::GenerateCameraVelocityBuffer(gfxContext, camera, true);

    TemporalEffects::ResolveImage(gfxContext);

    //ParticleEffects::Render(gfxContext, camera, *g_pSceneColorBuffer, *g_pSceneDepthBuffer,  g_LinearDepth[FrameIndex]);

    // Until I work out how to couple these two, it's "either-or".
    if (DepthOfField::Enable)
        DepthOfField::Render(gfxContext, camera.GetNearClip(), camera.GetFarClip());
    else
        MotionBlur::RenderObjectBlur(gfxContext, g_VelocityBuffer);

    gfxContext.Finish();

    m_Sequencer.FinishFrame();

}

void ModelViewer::CreateParticleEffects()
{
    ParticleEffectProperties Effect = ParticleEffectProperties();
    Effect.MinStartColor = Effect.MaxStartColor = Effect.MinEndColor = Effect.MaxEndColor = Color(1.0f, 1.0f, 1.0f, 0.0f);
    Effect.TexturePath = L"sparkTex.dds";

    Effect.TotalActiveLifetime = FLT_MAX;
    Effect.Size = Vector4(4.0f, 8.0f, 4.0f, 8.0f);
    Effect.Velocity = Vector4(20.0f, 200.0f, 50.0f, 180.0f);
    Effect.LifeMinMax = XMFLOAT2(1.0f, 3.0f);
    Effect.MassMinMax = XMFLOAT2(4.5f, 15.0f);
    Effect.EmitProperties.Gravity = XMFLOAT3(0.0f, -100.0f, 0.0f);
    Effect.EmitProperties.FloorHeight = -0.5f;
    Effect.EmitProperties.EmitPosW = Effect.EmitProperties.LastEmitPosW = XMFLOAT3(-1200.0f, 185.0f, -445.0f);
    Effect.EmitProperties.MaxParticles = 800;
    Effect.EmitRate = 64.0f;
    Effect.Spread.x = 20.0f;
    Effect.Spread.y = 50.0f;
    ParticleEffects::InstantiateEffect(Effect);

    ParticleEffectProperties Smoke = ParticleEffectProperties();
    Smoke.TexturePath = L"smoke.dds";

    Smoke.TotalActiveLifetime = FLT_MAX;
    Smoke.EmitProperties.MaxParticles = 25;
    Smoke.EmitProperties.EmitPosW = Smoke.EmitProperties.LastEmitPosW = XMFLOAT3(1120.0f, 185.0f, -445.0f);
    Smoke.EmitRate = 64.0f;
    Smoke.LifeMinMax = XMFLOAT2(2.5f, 4.0f);
    Smoke.Size = Vector4(60.0f, 108.0f, 30.0f, 208.0f);
    Smoke.Velocity = Vector4(30.0f, 30.0f, 10.0f, 40.0f);
    Smoke.MassMinMax = XMFLOAT2(1.0, 3.5);
    Smoke.Spread.x = 60.0f;
    Smoke.Spread.y = 70.0f;
    Smoke.Spread.z = 20.0f;
    ParticleEffects::InstantiateEffect(Smoke);

    ParticleEffectProperties Fire = ParticleEffectProperties();
    Fire.MinStartColor = Fire.MaxStartColor = Fire.MinEndColor = Fire.MaxEndColor = Color(8.0f, 8.0f, 8.0f, 0.0f);
    Fire.TexturePath = L"fire.dds";

    Fire.TotalActiveLifetime = FLT_MAX;
    Fire.Size = Vector4(54.0f, 68.0f, 0.1f, 0.3f);
    Fire.Velocity = Vector4(10.0f, 30.0f, 50.0f, 50.0f);
    Fire.LifeMinMax = XMFLOAT2(1.0f, 3.0f);
    Fire.MassMinMax = XMFLOAT2(10.5f, 14.0f);
    Fire.EmitProperties.Gravity = XMFLOAT3(0.0f, 1.0f, 0.0f);
    Fire.EmitProperties.EmitPosW = Fire.EmitProperties.LastEmitPosW = XMFLOAT3(1120.0f, 125.0f, 405.0f);
    Fire.EmitProperties.MaxParticles = 25;
    Fire.EmitRate = 64.0f;
    Fire.Spread.x = 1.0f;
    Fire.Spread.y = 60.0f;
    ParticleEffects::InstantiateEffect(Fire);
}

void ModelViewer::UpdatePlacedResources(GraphicsContext& context)
{
    // At most refresh ~1 a second
    const int FramesBetweenRefresh = (int) Graphics::GetFrameRate();
    
    static bool CbrWasEnabled = CbrEnabled;
    static bool MsaaWasEnabled = MsaaEnabled;
    static bool DrrWasEnabled = DrrEnabled;
    static int PrevMsaaMode = MsaaMode;
    static int PlacedResourceIndex;
    static int FramesUntilSafeRefresh;
    
    // PlacedResourceIndex + 2 because the next frame (PlacedResourceIndex + 1) will be used if DRR has to change
    // + 1 is valid because an extra buffer is allocated for cbr
    int CheckerAlternateIndex = (PlacedResourceIndex + 2) % NUM_CHECKER_BUFFERS;

    if (DrrEnabled) 
    {
        bool DrrRefresh = false;

        // force refresh if we are in a new mode
        DrrRefresh |= CbrWasEnabled != CbrEnabled;
        DrrRefresh |= MsaaWasEnabled != MsaaEnabled;
        DrrRefresh |= (PrevMsaaMode - MsaaMode) != 0;

        // toggle on render mode change 
        // so we can wait for gpu idle and force a refresh
        if ( DrrRefresh )
        {
            g_CommandManager.IdleGPU();
            FramesUntilSafeRefresh = 0;
        }
        else
        {
            FramesUntilSafeRefresh = FramesUntilSafeRefresh - 1;
            FramesUntilSafeRefresh = FramesUntilSafeRefresh < 0 ? 0 : FramesUntilSafeRefresh;
        }

        if ( FramesUntilSafeRefresh == 0 )
        {
            g_ResolutionScale = RecalculateResolutionScale( );

            // force 4 pixel alignment for checkerboarding
            int drrWidth = ALIGN((int) (g_DisplayWidth * g_ResolutionScale), 4);
            int drrHeight = ALIGN((int) (g_DisplayHeight * g_ResolutionScale), 4);

            int diffX = g_pSceneColorBuffer->GetWidth() - drrWidth;
            int diffY = g_pSceneColorBuffer->GetHeight() - drrHeight;

            //only change if it's more than N pixels, prevents jittering
            DrrRefresh |= abs(diffX) >= 4;
            DrrRefresh |= abs(diffY) >= 4;
        
            if (DrrRefresh)
            {
                FramesUntilSafeRefresh = FramesBetweenRefresh;
            
                TemporalEffects::TriggerReset = true;

                PlacedResourceIndex = (PlacedResourceIndex + 1) % NUM_SCENE_BUFFERS;

                ScopedTimer recreate( L"Recreated Placed Res" );

                if ( CbrEnabled )
                {
                    // PlacedResourceIndex + 2 because the next frame (PlacedResourceIndex + 1) will be used if DRR has to change
                    // + 1 is valid because an extra buffer is allocated for cbr
                    CheckerAlternateIndex = (PlacedResourceIndex + 2) % NUM_CHECKER_BUFFERS;
                    Graphics::RecreateCBRBuffers( PlacedResourceIndex, CheckerAlternateIndex, drrWidth >> 1, drrHeight >> 1 );
                }
                else if ( MsaaEnabled )
                    Graphics::RecreateMsaaBuffers( PlacedResourceIndex, MsaaMode, drrWidth, drrHeight );
                else
                    Graphics::RecreateSceneDepthBuffer( PlacedResourceIndex, drrWidth, drrHeight );

                // every time because other types (cbr or msaa) will be resolved here
                Graphics::RecreateSceneColorBuffer( PlacedResourceIndex, drrWidth, drrHeight );
            }
        }
    }
    else if ( DrrWasEnabled )
    {
        // Drr is now disabled, recreate all our buffers at full resolution
        g_CommandManager.IdleGPU();

        Graphics::RecreateCBRBuffers( PlacedResourceIndex, CheckerAlternateIndex, g_DisplayWidth >> 1, g_DisplayHeight >> 1 );
        
        Graphics::RecreateMsaaBuffers( PlacedResourceIndex, 1, g_DisplayWidth, g_DisplayHeight );
        Graphics::RecreateMsaaBuffers( PlacedResourceIndex, 2, g_DisplayWidth, g_DisplayHeight );
        Graphics::RecreateMsaaBuffers( PlacedResourceIndex, 3, g_DisplayWidth, g_DisplayHeight );

        Graphics::RecreateSceneDepthBuffer( PlacedResourceIndex, g_DisplayWidth, g_DisplayHeight );
        Graphics::RecreateSceneColorBuffer( PlacedResourceIndex, g_DisplayWidth, g_DisplayHeight );

        g_ResolutionScale = 1.0f;
    }

    SetFrameBuffers( context, PlacedResourceIndex, CheckerAlternateIndex, CbrWasEnabled, MsaaWasEnabled, PrevMsaaMode );

    DrrWasEnabled = DrrEnabled;
    CbrWasEnabled = CbrEnabled;
    MsaaWasEnabled = MsaaEnabled;
    PrevMsaaMode = MsaaMode;
}

float ModelViewer::RecalculateResolutionScale( )
{
    float ResolutionScale;

    static float FilteredFrameRate = Graphics::GetFrameRate();

    // filter noise out of the frame rate
    FilteredFrameRate = FilteredFrameRate * (1 - DrrFrameRateLowPassFilter) + (Graphics::GetFrameRate() * DrrFrameRateLowPassFilter);

    // loosely based on the original algorithm here: https://software.intel.com/en-us/articles/dynamic-resolution-rendering-article/
    float TimeRatio = FilteredFrameRate / DrrDesiredFrameRate;

    // round our time ratio based on nearest 10th, 100th, 1000th, whatever the user has set
    // this allows some headroom in the framerate to prevent resolution popping 
    // when the time+perf combination is hovering on a threshold
    TimeRatio = ((int) (TimeRatio * (1.0f / DrrFrameRateDeltaResolution))) * DrrFrameRateDeltaResolution - 1.0f;

    // increment or decrement our DRR scale based on the rate of change and framerate delta
    float Scale = DrrInternalScale + DrrInternalScale * TimeRatio * DrrRateOfChange;

    // clamp at 1.0 resolution: we do not allow super sampling until a downscale algorithm is implemented
    Scale = Math::Min( Math::Max( Scale, DrrMinScale ), 1 );

    // actual rendered scale is our internal scale rounded up to the nearest user specified resolution increment
    ResolutionScale = ((int) (((Scale + .05f) * (1.0f / DrrResolutionIncrements)))) * DrrResolutionIncrements;

    if ( DrrForceScale )
        ResolutionScale = DrrMinScale;

    DrrInternalFrameRate = (int) FilteredFrameRate;
    DrrInternalScale = Scale;

    return ResolutionScale;
}

void ModelViewer::SetFrameBuffers( GraphicsContext& context, int index, int alt_index, bool CbrWasEnabled, bool MsaaWasEnabled, int PrevMsaaMode )
{
    // NULL out all pointers so we can catch an error later 
    // if one is used but that mode isn't set
    g_pMsaaColor = NULL;
    g_pMsaaDepth = NULL;
    g_pSceneDepthBuffer = NULL;
    g_pCheckerboardColors[0] = NULL;
    g_pCheckerboardDepths[0] = NULL;
    g_pCheckerboardColors[1] = NULL;
    g_pCheckerboardDepths[1] = NULL;

    if ( CbrEnabled )
    {
        g_pCheckerboardColors[0] = &g_SceneColorBuffersCB2x[index];
        g_pCheckerboardDepths[0] = &g_SceneDepthBuffersCB2x[index];
        g_pCheckerboardColors[1] = &g_SceneColorBuffersCB2x[alt_index];
        g_pCheckerboardDepths[1] = &g_SceneDepthBuffersCB2x[alt_index];

        if ( CbrWasEnabled == false )
        {
            // Before barrier can be NULL but miniengine API takes reference so 
            // there seems to be no harm by setting a before barrier
            context.InsertAliasBarrier( *g_pCheckerboardColors[0], *g_pCheckerboardColors[0] );
            context.InsertAliasBarrier( *g_pCheckerboardColors[1], *g_pCheckerboardColors[1] );
            context.InsertAliasBarrier( *g_pCheckerboardDepths[0], *g_pCheckerboardDepths[0] );
            context.InsertAliasBarrier( *g_pCheckerboardDepths[1], *g_pCheckerboardDepths[1] );
        }
    }
    else if ( MsaaEnabled )
    {
        switch ( MsaaMode )
        {
        case 1 : 
            g_pMsaaColor = &g_SceneColorBuffers2x[index];
            g_pMsaaDepth = &g_SceneDepthBuffers2x[index];
            break;
        case 2:
            g_pMsaaColor = &g_SceneColorBuffers4x[index];
            g_pMsaaDepth = &g_SceneDepthBuffers4x[index];
            break;
        case 3:
            g_pMsaaColor = &g_SceneColorBuffers8x[index];
            g_pMsaaDepth = &g_SceneDepthBuffers8x[index];
            break;

        default:
            break;
        }

        if ( (MsaaWasEnabled == false || (PrevMsaaMode - MsaaMode) != 0) )
        {
            // Before barrier can be NULL but miniengine API takes reference so 
            // there seems to be no harm by setting a before barrier
            context.InsertAliasBarrier( *g_pMsaaColor, *g_pMsaaColor );
            context.InsertAliasBarrier( *g_pMsaaDepth, *g_pMsaaDepth );
        }
    }
    else
    {
        g_pSceneDepthBuffer = &g_SceneDepthBuffers[index];

        if ( CbrWasEnabled || MsaaWasEnabled )
            // Before barrier can be NULL but miniengine API takes reference so 
            // there seems to be no harm by setting a before barrier
            context.InsertAliasBarrier( *g_pSceneDepthBuffer, *g_pSceneDepthBuffer );
    }

    g_pSceneColorBuffer = &g_SceneColorBuffers[index];
}
