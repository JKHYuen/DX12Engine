#pragma once

#include <Events.h>

// Interface for game loop functionality
class IGame {
public:
    virtual ~IGame() = default;

    // Load content required for the demo.
    virtual bool LoadContent() = 0;
    // Unload demo specific content that was loaded in LoadContent.
    virtual void UnloadContent() = 0;
    // Start game loop, return error code
    virtual uint32_t Run() = 0;

    virtual void OnUpdate(UpdateEventArgs& e) {};
    virtual void OnKeyPressed(KeyEventArgs& e) {};
    virtual void OnKeyReleased(KeyEventArgs& e) {};
    virtual void OnMouseMoved(MouseMotionEventArgs& e) {};
    virtual void OnMouseButtonPressed(MouseButtonEventArgs& e) {};
    virtual void OnMouseButtonReleased(MouseButtonEventArgs& e) {};
    virtual void OnMouseWheel(MouseWheelEventArgs& e) {};
    virtual void OnResize(ResizeEventArgs& e) {};
};