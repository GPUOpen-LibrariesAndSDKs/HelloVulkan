#ifndef ANTERU_D3D12_SAMPLE_WINDOW_H_
#define ANTERU_D3D12_SAMPLE_WINDOW_H_

#include <string>
#include <Windows.h>
#include <memory>

namespace AMD {
/**
* Encapsulate a window class.
*
* Calls <c>::RegisterClass()</c> on create, and <c>::UnregisterClass()</c>
* on destruction.
*/
struct WindowClass final
{
public:
	WindowClass (const std::string& name,
		::WNDPROC procedure = ::DefWindowProc);
	~WindowClass ();

	const std::string& GetName () const;

	WindowClass (const WindowClass&) = delete;
	WindowClass& operator= (const WindowClass&) = delete;

private:
	std::string name_;
};

struct IWindow
{
public:
	IWindow () = default;
	IWindow (const IWindow&) = delete;
	IWindow& operator=(const IWindow&) = delete;

	virtual ~IWindow ();

	bool IsClosed () const;
	virtual void OnClose () = 0;

	int GetWidth () const;
	int GetHeight () const;

private:
	virtual bool IsClosedImpl () const = 0;
	virtual int GetWidthImpl () const = 0;
	virtual int GetHeightImpl () const = 0;
};

class Window : public IWindow
{
public:
	Window (const std::string& title, int width, int height);

	HWND GetHWND () const;

	void OnClose () override
	{
		isClosed_ = true;
	}

private:
	bool IsClosedImpl () const override
	{
		return isClosed_;
	}

	int GetWidthImpl () const override
	{
		return width_;
	}

	int GetHeightImpl () const override
	{
		return height_;
	}

	std::unique_ptr<WindowClass> windowClass_;
	HWND hwnd_ = 0;
	bool isClosed_ = false;
	int width_ = -1, height_ = -1;
};
}

#endif
