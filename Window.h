#pragma once

/*
 *  Copyright(c) 2018 Jeremiah van Oosten
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files(the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions :
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *  IN THE SOFTWARE.
 */

 /**
  *  @file Window.h
  *  @date October 24, 2018
  *  @author Jeremiah van Oosten
  *
  *  @brief A window for our application.
  */


#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <memory>
#include <string>

#include "Events.h"
#include "HighResolutionClock.h"

class IGame;

class Window {
    // The Window procedure needs to call protected methods of this class.
    friend LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    // Only the application can create a window.
    friend class Application;

public:
    ~Window();
    Window(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&) = delete;
    Window& operator=(Window&&) = delete;

    // Number of swapchain back buffers.
    static const UINT sk_BufferCount = 2;

    /**
    * Get a handle to this window's instance.
    * @returns The handle to the window instance or nullptr if this is not a valid window.
    */
    HWND GetWindowHandle() const;

    void SetWindowTitle(const std::wstring& windowTitle) const;

    bool IsFullScreen() const;
    void SetFullscreen(bool fullscreen);
    void ToggleFullscreen();

    std::pair<uint32_t, uint32_t> GetCurrentMonitorDimensions() const;

    void Show();
    void Hide();

protected:
    // Only Application should create windows
    Window(HWND hWnd, const std::wstring& windowName, int clientWidth, int clientHeight, IGame& game);

    // Update should only be called by the Application class
    void OnUpdate(UpdateEventArgs& e);
    void OnClose(WindowCloseEventArgs& e);

    void OnKeyPressed(KeyEventArgs& e);
    void OnKeyReleased(KeyEventArgs& e);
    void OnMouseMove(MouseMotionEventArgs& e);
    void OnMouseButtonPressed(MouseButtonEventArgs& e);
    void OnMouseButtonReleased(MouseButtonEventArgs& e);
    void OnMouseWheel(MouseWheelEventArgs& e);

    // The window was resized.
    void OnResize(ResizeEventArgs& e);

private:
    std::wstring m_WindowTitle;

    // Passed by Application class during construction, used for event callbacks
    IGame& m_Game;

    HWND m_hWnd;

    float m_DPIScaling;

    bool m_IsFullscreen;
    bool m_IsMinimized;
    bool m_IsMaximized;

    bool m_bInClientRect;

    // Window RECT when not full screen
    RECT m_WindowRect;

    HighResolutionClock m_Timer;
};