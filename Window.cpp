#include <DX12LibPCH.h>

#include "Window.h"

#include "IGame.h"
#include "Application.h"
#include "CommandQueue.h"
#include "CommandList.h"
#include "RenderTarget.h"
#include "ResourceStateTracker.h"
#include "Texture.h"

Window::Window(HWND hWnd, const std::wstring& windowName, int clientWidth, int clientHeight, IGame& game)
    : m_hWnd(hWnd)
    , m_WindowTitle(windowName)
    , m_ClientWidth(clientWidth)
    , m_ClientHeight(clientHeight)
    , m_IsFullscreen(false)
    , m_IsMinimized(false)
    , m_IsMaximized(false)
    , m_bInClientRect(false)
    , m_WindowRect()
    , m_Timer()
    , m_Game(game) {
        m_DPIScaling = ::GetDpiForWindow(hWnd) / 96.0f;
}

Window::~Window() {
    ::DestroyWindow(m_hWnd);
}

HWND Window::GetWindowHandle() const {
    return m_hWnd;
}

void Window::Show() {
    ::ShowWindow(m_hWnd, SW_SHOW);
}

void Window::Hide() {
    ::ShowWindow(m_hWnd, SW_HIDE);
}

uint32_t Window::GetClientWidth() const {
    return m_ClientWidth;
}

uint32_t Window::GetClientHeight() const {
    return m_ClientHeight;
}

bool Window::IsFullScreen() const {
    return m_IsFullscreen;
}

void Window::SetWindowTitle(const std::wstring& windowTitle) const {
    ::SetWindowTextW(m_hWnd, (m_WindowTitle + windowTitle).c_str());
}

void Window::SetFullscreen(bool fullscreen) {
    if(m_IsFullscreen != fullscreen) {
        m_IsFullscreen = fullscreen;

        if(m_IsFullscreen) // Switching to fullscreen.
        {
            // Store the current window dimensions so they can be restored 
            // when switching out of fullscreen state.
            ::GetWindowRect(m_hWnd, &m_WindowRect);

            // Set the window style to a borderless window so the client area fills
            // the entire screen.
            UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

            ::SetWindowLongW(m_hWnd, GWL_STYLE, windowStyle);

            // Query the name of the nearest display device for the window.
            // This is required to set the fullscreen dimensions of the window
            // when using a multi-monitor setup.
            HMONITOR hMonitor = ::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFOEX monitorInfo = {};
            monitorInfo.cbSize = sizeof(MONITORINFOEX);
            ::GetMonitorInfo(hMonitor, &monitorInfo);

            ::SetWindowPos(m_hWnd, HWND_TOP,
                monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.top,
                monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ::ShowWindow(m_hWnd, SW_MAXIMIZE);
        }
        else {
            // Restore all the window decorators.
            ::SetWindowLong(m_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

            ::SetWindowPos(m_hWnd, HWND_NOTOPMOST,
                m_WindowRect.left,
                m_WindowRect.top,
                m_WindowRect.right - m_WindowRect.left,
                m_WindowRect.bottom - m_WindowRect.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ::ShowWindow(m_hWnd, SW_NORMAL);
        }
    }
}

void Window::ToggleFullscreen() {
    SetFullscreen(!m_IsFullscreen);
}

void Window::OnUpdate(UpdateEventArgs& e) {
    m_Timer.Tick();

    e.DeltaTime = m_Timer.GetDeltaSeconds();
    e.Time = m_Timer.GetTotalSeconds();

    m_Game.OnUpdate(e);
}

// TODO: WindowCloseEventArgs is unused
void Window::OnClose(WindowCloseEventArgs& e) {
    UpdateEventArgs updateEventArgs(m_Timer.GetDeltaSeconds(), m_Timer.GetTotalSeconds());
    m_Game.OnUpdate(updateEventArgs);
};

void Window::OnKeyPressed(KeyEventArgs& e) {
    m_Game.OnKeyPressed(e);
}

void Window::OnKeyReleased(KeyEventArgs& e) {
    m_Game.OnKeyReleased(e);
}

void Window::OnMouseMove(MouseMotionEventArgs& e) {
    m_Game.OnMouseMove(e);
}

void Window::OnMouseButtonPressed(MouseButtonEventArgs& e) {
    m_Game.OnMouseButtonPressed(e);
}

void Window::OnMouseButtonReleased(MouseButtonEventArgs& e) {
    m_Game.OnMouseButtonReleased(e);
}

void Window::OnMouseWheel(MouseWheelEventArgs& e) {
    m_Game.OnMouseWheel(e);
}

void Window::OnResize(ResizeEventArgs& e) {
    if(m_ClientWidth != e.Width || m_ClientHeight != e.Height) {
        m_ClientWidth = std::max(1, e.Width);
        m_ClientHeight = std::max(1, e.Height);
    }

    m_Game.OnResize(e);
}