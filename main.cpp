#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Shlwapi.h>

// For CommandLineToArgvW
//#include <shellapi.h>

#include "DX12EngineCore/Application.h"
#include "DX12EngineCore/Device.h"
#include "DemoGame.h"
#include "Logger.h"
#include "AssetImporter.h"
#include "Helpers.h"

#include <dxgidebug.h>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {
    int retCode = 0;

#if defined( _DEBUG )
    // Enable DirectX debug layer
    Microsoft::WRL::ComPtr<ID3D12Debug> debugInterface;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();
#endif

    AssetImporter::Create();

    // Initialize basic global console logger
    Logger::InitializeConsole();

    Application::Create(hInstance);
    {
        std::unique_ptr<IGame> demo = std::make_unique<DemoGame>(L"DX12 Render", 1280, 720, /*vSync*/ true, false);
        retCode = demo->Run();
    }
    Application::Destroy();

#if defined( _DEBUG )
    IDXGIDebug1* dxgiDebug;
    DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));
    dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
    dxgiDebug->Release();
#endif

    return retCode;
}