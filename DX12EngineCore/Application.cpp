#include <DX12LibPCH.h>
#include "Application.h"

#include <iostream>
#include <comdef.h>
#include <fcntl.h>
#include <io.h>

#include "CommandQueue.h"
#include "DescriptorAllocator.h"
#include "Window.h"

#include "imgui.h"
#include "imgui_impl_win32.h"

namespace {
    Application* sp_Singleton = nullptr;

    constexpr wchar_t WINDOW_CLASS_NAME[] = L"DX12RenderWindowClass";

    std::map<HWND,         std::weak_ptr<Window>> s_WindowMap;
    std::map<std::wstring, std::weak_ptr<Window>> s_WindowMapByName;

    std::mutex s_WindowHandlesMutex;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

// A wrapper struct to allow shared pointers for the window class.
struct MakeWindow : public Window {
    MakeWindow(HWND hWnd, const std::wstring& windowName, int clientWidth, int clientHeight, IGame& game)
        : Window(hWnd, windowName, clientWidth, clientHeight, game) {}
};

Application::Application(HINSTANCE hInst)
    : m_hInstance(hInst)
    , mb_IsInitialized(false)
    , m_RequestQuit(false)
    , mb_CursorClientAreaLockState(false) {

    // Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
    // Using this awareness context allows the client area of the window
    // to achieve 100% scaling while still allowing non-client window content to
    // be rendered in a DPI sensitive fashion.
    // @see https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setthreaddpiawarenesscontext
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Initializes the COM library for use by the calling thread, sets the thread's concurrency model, and creates a new
    // apartment for the thread if one is required.
    // This must be called at least once for each thread that uses the COM library.
    // @see https://docs.microsoft.com/en-us/windows/win32/api/objbase/nf-objbase-coinitialize
    HRESULT hr = CoInitialize(NULL);
    if(FAILED(hr)) {
        _com_error err(hr);
        //spdlog::critical("CoInitialize failed: {}", err.ErrorMessage());
        throw new std::exception((char*)(err.ErrorMessage()));
    }

    WNDCLASSEXW wndClass = {0};

    wndClass.cbSize = sizeof(WNDCLASSEX);
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = &WndProc;
    wndClass.hInstance = m_hInstance;
    wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    //wndClass.hIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(APP_ICON));
    wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wndClass.lpszMenuName = nullptr;
    wndClass.lpszClassName = WINDOW_CLASS_NAME;
    //wndClass.hIconSm = LoadIcon(m_hInstance, MAKEINTRESOURCE(APP_ICON));

    if(!RegisterClassExW(&wndClass)) {
        MessageBoxA(NULL, "Unable to register the window class.", "Error", MB_OK | MB_ICONERROR);
    }
}

Application::~Application() {
    s_WindowMap.clear();
    s_WindowMapByName.clear();
}

Application& Application::Create(HINSTANCE hInst) {
    if(!sp_Singleton) {
        sp_Singleton = new Application(hInst);
    }
    return *sp_Singleton;
}

Application& Application::Get() {
    assert(sp_Singleton != nullptr);
    return *sp_Singleton;
}

void Application::Destroy() {
    if(sp_Singleton) {
        delete sp_Singleton;
        sp_Singleton = nullptr;
    }
}

std::shared_ptr<Window> Application::CreateRenderWindow(const std::wstring& windowName, int clientWidth, int clientHeight, IGame& game) {
    int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

    RECT windowRect = {0, 0, static_cast<LONG>(clientWidth), static_cast<LONG>(clientHeight)};

    // Insures client area has exact width and height given above, adjusting for window elements e.g. frame
    ::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    uint32_t width = windowRect.right - windowRect.left;
    uint32_t height = windowRect.bottom - windowRect.top;

    int windowX = std::max<int>(0, (screenWidth  - (int)width)  / 2);
    int windowY = std::max<int>(0, (screenHeight - (int)height) / 2);

    HWND hWindow = ::CreateWindowExW(
        NULL, WINDOW_CLASS_NAME, windowName.c_str(), WS_OVERLAPPEDWINDOW, 
        windowX, windowY, width, height, NULL, NULL, m_hInstance, NULL
    );

    if(!hWindow) {
        // TODO: log error
        return nullptr;
    }

    auto pWindow = std::make_shared<MakeWindow>(hWindow, windowName, clientWidth, clientHeight, game);

    s_WindowMap.emplace(hWindow, pWindow);
    s_WindowMapByName.emplace(windowName, pWindow);

    return pWindow;
}

std::shared_ptr<Window> Application::GetWindowByName(const std::wstring& windowName) const {
    auto iter = s_WindowMapByName.find(windowName);
    return (iter != s_WindowMapByName.end()) ? iter->second.lock() : nullptr;
}


int32_t Application::Run() {
    assert(!mb_IsInitialized);
    mb_IsInitialized = true;

    // Initialize Raw Input (Mouse only)
    {
        RAWINPUTDEVICE Rid[1];
        Rid[0].usUsagePage = 0x01;          // HID_USAGE_PAGE_GENERIC
        Rid[0].usUsage = 0x02;              // HID_USAGE_GENERIC_MOUSE

        // WARNING: Not using RIDEV_NOLEGACY will degrade performance if moving high polling mouse a lot, not worried about it for now.
        //          Can use Buffered RawInput or DirectInput if this becomes an issue
        //          See: https://ph3at.github.io/posts/Windows-Input/
        Rid[0].dwFlags = 0;

        Rid[0].hwndTarget = 0;
        if(RegisterRawInputDevices(Rid, 1, sizeof(Rid[0])) == FALSE)
            OutputDebugString(TEXT("No device found for raw input.\n"));
    }

    MSG msg = {};
    while(::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);

        // Quit() sets m_RequestQuit to true
        if(m_RequestQuit) {
            ::PostQuitMessage(0);
            m_RequestQuit = false;
        }

        if(msg.message == WM_QUIT) {
            break; 
        }
    }

    mb_IsInitialized = false;
    return static_cast<int32_t>(msg.wParam);
}

void Application::Quit() {
    // When called from another thread other than the main thread,
    // the WM_QUIT message goes to that thread and will not be handled
    // in the main thread. To circumvent this, we also set a boolean flag
    // to indicate that the user has requested to quit the application.
    m_RequestQuit = true;
}

void Application::LockCursorToClientArea(HWND hwnd, bool state) {
    mb_CursorClientAreaLockState = state;

    if(!state) {
        ClipCursor(nullptr);
        return;
    }

    RECT rect {};
    GetClientRect(hwnd, &rect);

    POINT ul;
    ul.x = rect.left;
    ul.y = rect.top;

    POINT lr;
    lr.x = rect.right;
    lr.y = rect.bottom;

    MapWindowPoints(hwnd, nullptr, &ul, 1);
    MapWindowPoints(hwnd, nullptr, &lr, 1);

    rect.left = ul.x;
    rect.top = ul.y;

    rect.right = lr.x;
    rect.bottom = lr.y;

    ClipCursor(&rect);
};

static void DecodeMouseData(UINT messageID, WPARAM wParam, LPARAM lParam, MouseButtonEventArgs::MouseButton& out_MouseButton, 
    bool& out_LButton, bool& out_RButton, bool& out_MButton, bool& out_Shift, bool& out_Control,
    int& out_X, int& out_Y) {

    switch(messageID) {
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
        out_MouseButton = MouseButtonEventArgs::Left;
        break;
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
        out_MouseButton = MouseButtonEventArgs::Right;
        break;
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
        out_MouseButton = MouseButtonEventArgs::Middle;
        break;
    }

    short keyStates = (short)LOWORD(wParam);

    out_LButton = (keyStates & MK_LBUTTON) != 0;
    out_RButton = (keyStates & MK_RBUTTON) != 0;
    out_MButton = (keyStates & MK_MBUTTON) != 0;
    out_Shift   = (keyStates & MK_SHIFT)   != 0;
    out_Control = (keyStates & MK_CONTROL) != 0;

    out_X = ((int)(short)LOWORD(lParam));
    out_Y = ((int)(short)HIWORD(lParam));
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if(ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam))
        return true;

    std::shared_ptr<Window> pWindow;
    {
        auto iter = s_WindowMap.find(hwnd);
        if(iter != s_WindowMap.end()) {
            pWindow = iter->second.lock();
        }
    }

    if(pWindow) {
        switch(message) {

        case WM_ACTIVATE:
        case WM_ACTIVATEAPP:
        {
            Application::Get().LockCursorToClientArea(hwnd, wParam);
        }
        break;

        case WM_PAINT:
        {
            // Delta time will be filled in by the Window.
            UpdateEventArgs updateEventArgs(0.0f, 0.0f);
            pWindow->OnUpdate(updateEventArgs);
        }
        break;

        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            MSG charMsg;
            // Get the Unicode character (UTF-16)
            unsigned int c = 0;
            // For printable characters, the next message will be WM_CHAR.
            // This message contains the character code we need to send the KeyPressed event.
            // Inspired by the SDL 1.2 implementation.
            if(PeekMessage(&charMsg, hwnd, 0, 0, PM_NOREMOVE) && charMsg.message == WM_CHAR) {
                c = static_cast<unsigned int>(charMsg.wParam);
            }
            bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            KeyCode::Key key = (KeyCode::Key)wParam;
            unsigned int scanCode = (lParam & 0x00FF0000) >> 16;
            KeyEventArgs keyEventArgs(key, c, KeyEventArgs::Pressed, shift, control, alt);
            pWindow->OnKeyPressed(keyEventArgs);
        }
        break;

        case WM_SYSKEYUP:
        case WM_KEYUP:
        {
            bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            KeyCode::Key key = (KeyCode::Key)wParam;
            unsigned int c = 0;
            unsigned int scanCode = (lParam & 0x00FF0000) >> 16;

            // Determine which key was released by converting the key code and the scan code
            // to a printable character (if possible).
            // Inspired by the SDL 1.2 implementation.
            unsigned char keyboardState[256];
            std::ignore = GetKeyboardState(keyboardState);
            wchar_t translatedCharacters[4];
            if(int result = ToUnicodeEx(static_cast<UINT>(wParam), scanCode, keyboardState, translatedCharacters, 4, 0, NULL) > 0) {
                c = translatedCharacters[0];
            }

            KeyEventArgs keyEventArgs(key, c, KeyEventArgs::Released, shift, control, alt);
            pWindow->OnKeyReleased(keyEventArgs);
        }
        break;

        // The default window procedure will play a system notification sound when pressing the Alt+Enter keyboard combination if this message is not handled.
        case WM_SYSCHAR:
            break;

        // Handle Raw Mouse Input
        // https://learn.microsoft.com/en-us/windows/win32/inputdev/using-raw-input
        case WM_INPUT:
        {
            UINT dwSize {};

            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
            LPBYTE lpb = new BYTE[dwSize];

            if(GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
                OutputDebugString(TEXT("GetRawInputData does not return correct size !\n"));

            RAWINPUT* raw = (RAWINPUT*)lpb;

            if(raw->header.dwType == RIM_TYPEMOUSE) {
                bool lButton = raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN;
                bool mButton = raw->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN;
                bool rButton = raw->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN;

                int deltaX = raw->data.mouse.lLastX;
                int deltaY = raw->data.mouse.lLastY;

                // Calculate client mouse coords
                POINT cursorPos;
                GetCursorPos(&cursorPos);
                ScreenToClient(hwnd, &cursorPos);

                MouseMotionEventArgs mouseMotionEventArgs { lButton, mButton, rButton, deltaX, deltaY, cursorPos.x, cursorPos.y };
                pWindow->OnMouseMove(mouseMotionEventArgs);
            }

            delete[] lpb;
        }
        break;

        case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
        case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
        {
            MouseButtonEventArgs::MouseButton mouseButton;
            bool lButton, rButton, mButton, shift, control;
            int x, y;
            DecodeMouseData(message, wParam, lParam, mouseButton, lButton, rButton, mButton, shift, control, x, y);

            MouseButtonEventArgs::ButtonState buttonState = MouseButtonEventArgs::Released;
            if(message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN)
                buttonState = MouseButtonEventArgs::Pressed;

            MouseButtonEventArgs mouseButtonEventArgs(mouseButton, buttonState, lButton, mButton, rButton, control, shift, x, y);

            if(buttonState == MouseButtonEventArgs::Pressed) {
                pWindow->OnMouseButtonPressed(mouseButtonEventArgs);
            }
            else {
                pWindow->OnMouseButtonReleased(mouseButtonEventArgs);
            }
        }
        break;

        case WM_MOUSEWHEEL:
        {
            // mouseButton is unused
            MouseButtonEventArgs::MouseButton mouseButton;
            bool lButton, rButton, mButton, shift, control;
            int x, y;
            DecodeMouseData(message, wParam, lParam, mouseButton, lButton, rButton, mButton, shift, control, x, y);

            // The distance the mouse wheel is rotated.
            // A positive value indicates the wheel was rotated to the right.
            // A negative value indicates the wheel was rotated to the left.
            float zDelta = ((int)(short)HIWORD(wParam)) / (float)WHEEL_DELTA;

            // Convert the screen coordinates to client coordinates.
            POINT clientToScreenPoint;
            clientToScreenPoint.x = x;
            clientToScreenPoint.y = y;
            ScreenToClient(hwnd, &clientToScreenPoint);

            MouseWheelEventArgs mouseWheelEventArgs(
                zDelta, lButton, mButton, rButton, control, shift, (int)clientToScreenPoint.x, (int)clientToScreenPoint.y
            );
            pWindow->OnMouseWheel(mouseWheelEventArgs);
        }
        break;

        case WM_SIZE:
        {
            int width = ((int)(short)LOWORD(lParam));
            int height = ((int)(short)HIWORD(lParam));

            // Set "LockCursorToClientArea" to true again if cursor is locked to refresh bounds for ClipCursor()
            if(Application::Get().GetCursorClientAreaLockState()) Application::Get().LockCursorToClientArea(hwnd, true);

            ResizeEventArgs resizeEventArgs(width, height);
            pWindow->OnResize(resizeEventArgs);
        }
        break;

        case WM_CLOSE:
        {
            WindowCloseEventArgs windowCloseEventArgs;
            pWindow->OnClose(windowCloseEventArgs);

            // Check to see if the user canceled the close event.
            if(windowCloseEventArgs.ConfirmClose) {
                // DestroyWindow( hwnd );
                // Just hide the window.
                // Windows will be destroyed when the application quits.
                pWindow->Hide();
            }
        }
        break;

        case WM_DESTROY:
        {
            std::lock_guard<std::mutex> lock(s_WindowHandlesMutex);
            auto iter = s_WindowMap.find(hwnd);
            if(iter != s_WindowMap.end()) {
                s_WindowMap.erase(iter);
            }
            ::PostQuitMessage(0);
            return 0;
        }
        break;

        default:
            return ::DefWindowProcW(hwnd, message, wParam, lParam);
        }
    }
    else {
        switch(message) {
        case WM_CREATE:
            break;
        default:
            return ::DefWindowProcW(hwnd, message, wParam, lParam);
        }
    }

    return 0;
}