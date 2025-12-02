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
  *  @file Application.h
  *  @date October 22, 2018
  *  @author Jeremiah van Oosten
  *
  *  @brief The application class is used to create windows for our application.
  */

// Additional Notes - KHY
// 
// Handles windows message loop.
// Mouse buttons and keyboard buttons are handled by windows api.
// Mouse movement is handled by raw input (experimental).

/// NOTE: Cursor visibility is currently solely controlled by ImGui, not ideal but works for now

#include "DescriptorAllocation.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <memory>
#include <string>

class Window;
class IGame;

class Application {
    friend LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
    // Hard coded until something more complicated needed (e.g. game switching in editor app)
    static const inline std::wstring assetFolderPath = L"assets";

    // Create an application instance.
    Application(HINSTANCE hInst);
    ~Application();

    /**
    * Create the application singleton with the application instance handle.
    */
    static Application& Create(HINSTANCE hInst);

    /**
    * Destroy the application instance and all windows created by this application instance.
    */
    static void Destroy();
    /**
    * Get the application singleton.
    */
    static Application& Get();

    /**
     * Create a render window.
     *
     * @param windowName The name of the window instance. This will also be the
     * name that appears in the title of the Window.
     * @param clientWidth The width (in pixels) of the window's client area.
     * @param clientHeight The height (in pixels) of the window's client area.
     * @returns The created window instance.
     */
    std::shared_ptr<Window> CreateRenderWindow(const std::wstring& windowName, int clientWidth, int clientHeight, IGame& game);

    /**
     * Get a window by name.
     *
     * @param windowName The name that was used to create the window.
     */
    std::shared_ptr<Window> GetWindowByName(const std::wstring& windowName) const;

    /**
     * Start the main application run loop.
     *
     * @returns The error code (if an error occurred).
     */
    int32_t Run();

    void LockCursorToClientArea(HWND hwnd, bool state);
    bool GetCursorClientAreaLockState() const { return mb_CursorClientAreaLockState; };

    void Quit();

private:
    // Singleton
    Application(const Application&)       = delete;
    Application(Application&&)            = delete;
    Application& operator=(Application&)  = delete;
    Application& operator=(Application&&) = delete;

    HINSTANCE m_hInstance;

    // Set to true while the application is running. 
    // Used to make sure singleton is initialized before running message loop
    std::atomic_bool mb_IsInitialized;
    // Quit() sets this to true, ending message loop
    std::atomic_bool m_RequestQuit;

    bool mb_CursorClientAreaLockState;
};