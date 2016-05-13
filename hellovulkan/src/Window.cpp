#include "Window.h"

namespace AMD {
namespace {
///////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK amdWndProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
    )
{
    auto ptr = ::GetWindowLongPtr(hwnd, GWLP_USERDATA);
    auto window = reinterpret_cast<IWindow*> (ptr);

    switch (uMsg) {
    case WM_CLOSE:
        window->OnClose();
        return 0;
    }

    return ::DefWindowProcA(hwnd, uMsg, wParam, lParam);
}
}

///////////////////////////////////////////////////////////////////////////////
IWindow::~IWindow()
{
}

///////////////////////////////////////////////////////////////////////////////
int IWindow::GetWidth() const
{
    return GetWidthImpl();
}

///////////////////////////////////////////////////////////////////////////////
int IWindow::GetHeight() const
{
    return GetHeightImpl();
}

///////////////////////////////////////////////////////////////////////////////
Window::Window(const std::string& title, const int width, const int height)
    : width_ (width)
    , height_ (height)
{
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    ::RECT rect;
    ::SetRect(&rect, 0, 0, width, height);
    ::AdjustWindowRect(&rect, style, FALSE);

    windowClass_.reset(new WindowClass("D3D12SampleWindowClass", amdWndProc));

    // Create the main window.
    hwnd_ = ::CreateWindowA(windowClass_->GetName().c_str(),
        title.c_str(),
        style, CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top, (HWND)NULL,
        (HMENU)NULL, NULL, (LPVOID)NULL);

    ::SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR> (this));

    // Show the window and paint its contents.
    ::ShowWindow(hwnd_, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd_);
}

///////////////////////////////////////////////////////////////////////////////
bool IWindow::IsClosed() const
{
    return IsClosedImpl();
}

///////////////////////////////////////////////////////////////////////////////
HWND Window::GetHWND () const
{
    return hwnd_;
}

/////////////////////////////////////////////////////////////////////////
WindowClass::WindowClass(const std::string& name, ::WNDPROC procedure)
    : name_(name)
{
    ::WNDCLASSA wc;

    // Register the window class for the main window.
    wc.style = 0;
    wc.lpfnWndProc = procedure;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = NULL;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = name_.c_str();

    ::RegisterClassA(&wc);
}

/////////////////////////////////////////////////////////////////////////
const std::string& WindowClass::GetName() const
{
    return name_;
}

/////////////////////////////////////////////////////////////////////////
WindowClass::~WindowClass()
{
    ::UnregisterClassA(name_.c_str(),
        (HINSTANCE)::GetModuleHandle(NULL));
}
}