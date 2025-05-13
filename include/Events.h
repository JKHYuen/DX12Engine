// From https://github.com/jpvanoosten/LearningDirectX12/tree/v0.0.2/DX12Lib/inc

#pragma once
#include "KeyCodes.h"
#include <cstdint>

// Base class for all event args
class EventArgs {
public:
    EventArgs() {}
};

class WindowCloseEventArgs : public EventArgs {
public:
    using base = EventArgs;
    WindowCloseEventArgs()
        : base()
        , ConfirmClose(true) {}

    /**
     * The user can cancel a window closing operation by registering for the
     * Window::Close event on the Window and setting the
     * WindowCloseEventArgs::ConfirmClose property to false if the window should
     * be kept open (for example, if closing the window would cause unsaved
     * file changes to be lost).
     */
    bool ConfirmClose;
};

class KeyEventArgs : public EventArgs {
public:
    enum KeyState {
        Released = 0,
        Pressed  = 1
    };

    using base = EventArgs;
    KeyEventArgs( KeyCode::Key key, unsigned int c, KeyState state, bool control, bool shift, bool alt )
        : Key(key)
        , Char(c)
        , State(state)
        , Control(control)
        , Shift(shift)
        , Alt(alt)
    {}

    KeyCode::Key    Key;    // The Key Code that was pressed or released.
    unsigned int    Char;   // The 32-bit character code that was pressed. This value will be 0 if it is a non-printable character.
    KeyState        State;  // Was the key pressed or released?
    bool            Control;// Is the Control modifier pressed
    bool            Shift;  // Is the Shift modifier pressed
    bool            Alt;    // Is the Alt modifier pressed
};

class MouseMotionEventArgs : public EventArgs {
public:
    using base = EventArgs;
    MouseMotionEventArgs( bool leftButton, bool middleButton, bool rightButton, int deltaX, int deltaY, int x = 0, int y = 0)
        : LeftButton( leftButton )
        , MiddleButton( middleButton )
        , RightButton( rightButton )
        , X( x )
        , Y( y )
        , DeltaX(deltaX)
        , DeltaY(deltaY)
    {}

    bool LeftButton;    // Is the left mouse button down?
    bool MiddleButton;  // Is the middle mouse button down?
    bool RightButton;   // Is the right mouse button down?

    int X;              // The X-position of the cursor relative to the upper-left corner of the client area.
    int Y;              // The Y-position of the cursor relative to the upper-left corner of the client area.
    int DeltaX;           // How far the mouse moved since the last event.
    int DeltaY;           // How far the mouse moved since the last event.
};

class MouseButtonEventArgs : public EventArgs {
public:
    enum MouseButton {
        None   = 0,
        Left   = 1,
        Right  = 2,
        Middle = 3
    };
    enum ButtonState {
        Released = 0,
        Pressed  = 1
    };

    using base = EventArgs;
    MouseButtonEventArgs( MouseButton buttonID, ButtonState state, bool leftButton, bool middleButton, bool rightButton, bool control, bool shift, int x, int y )
        : Button( buttonID )
        , State( state )
        , LeftButton( leftButton )
        , MiddleButton( middleButton )
        , RightButton( rightButton )
        , Control( control )
        , Shift( shift )
        , X( x )
        , Y( y )
    {}

    MouseButton Button; // The mouse button that was pressed or released.
    ButtonState State;  // Was the button pressed or released?
    bool LeftButton;    // Is the left mouse button down?
    bool MiddleButton;  // Is the middle mouse button down?
    bool RightButton;   // Is the right mouse button down?
    bool Control;       // Is the CTRL key down?
    bool Shift;         // Is the Shift key down?

    int X;              // The X-position of the cursor relative to the upper-left corner of the client area.
    int Y;              // The Y-position of the cursor relative to the upper-left corner of the client area.
};

class MouseWheelEventArgs: public EventArgs {
public:
    using base = EventArgs;
    MouseWheelEventArgs( float wheelDelta, bool leftButton, bool middleButton, bool rightButton, bool control, bool shift, int x, int y )
        : WheelDelta( wheelDelta )
        , LeftButton( leftButton )
        , MiddleButton( middleButton )
        , RightButton( rightButton )
        , Control( control )
        , Shift( shift )
        , X( x )
        , Y( y )
    {}

    float WheelDelta;   // How much the mouse wheel has moved. A positive value indicates that the wheel was moved to the right. A negative value indicates the wheel was moved to the left.
    bool LeftButton;    // Is the left mouse button down?
    bool MiddleButton;  // Is the middle mouse button down?
    bool RightButton;   // Is the right mouse button down?
    bool Control;       // Is the CTRL key down?
    bool Shift;         // Is the Shift key down?

    int X;              // The X-position of the cursor relative to the upper-left corner of the client area.
    int Y;              // The Y-position of the cursor relative to the upper-left corner of the client area.
};

class ResizeEventArgs : public EventArgs {
public:
    using base = EventArgs;
    ResizeEventArgs( int width, int height )
        : Width( width )
        , Height( height )
    {}

    // The new width of the window
    int Width;
    // The new height of the window.
    int Height;
};

class UpdateEventArgs : public EventArgs {
public:
    using base = EventArgs;
    UpdateEventArgs(double deltaTime, double time)
        : DeltaTime(deltaTime)
        , Time(time)
        {}

    double DeltaTime;
    double Time;
};

class UserEventArgs : public EventArgs {
public:
    using base = EventArgs;
    UserEventArgs( int code, void* data1, void* data2 )
        : Code( code )
        , Data1( data1 )
        , Data2( data2 )
    {}

    int     Code;
    void*   Data1;
    void*   Data2;
};