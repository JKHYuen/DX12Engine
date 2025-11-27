#pragma once

#include <Events.h>

// Interface for game/render loop functionality
class IGame {
public:
    virtual ~IGame() = default;

    // Load content required for the demo.
    virtual bool Initialize() = 0;
    // Unload demo specific content that was loaded in LoadContent.
    virtual void UnloadContent() = 0;
    // Start game loop, return error code
    virtual uint32_t Run() = 0;

    virtual void OnUpdate(const UpdateEventArgs & e) = 0;

    virtual void OnKeyPressed(const KeyEventArgs& e) {};
    virtual void OnKeyReleased(const KeyEventArgs& e) {};
    virtual void OnMouseMove(const MouseMotionEventArgs& e) {};
    virtual void OnMouseButtonPressed(const MouseButtonEventArgs& e) {};
    virtual void OnMouseButtonReleased(const MouseButtonEventArgs& e) {};
    virtual void OnMouseWheel(const MouseWheelEventArgs& e) {};
    virtual void OnResize(const ResizeEventArgs & e) {};
};