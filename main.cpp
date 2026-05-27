#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "AssetImporter.h"
#include "DemoGame.h"
#include "DX12EngineCore/Application.h"
#include "Helpers.h"
#include "Logger.h"

#include <dxgi1_3.h>
#include <dxgidebug.h>

int CALLBACK wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR lpCmdLine, _In_ int nCmdShow) {
    int retCode = 0;

#if defined( _DEBUG )
    // Enable DirectX debug layer
    Microsoft::WRL::ComPtr<ID3D12Debug> debugInterface;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();
#endif

    AssetImporter::Initialize();

    // Initialize basic global console logger
    Logger::InitializeConsole();

    Application::Initialize(hInstance);
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