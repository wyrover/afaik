#include "../Include/Window.h"
#include "../Include/UIComponent.h"

// much of this class is inspired by https://github.com/LaurentGomila/SFML :D

struct GdiInit
{
  Gdiplus::GdiplusStartupInput gdiplusStartupInput;
  ULONG_PTR gdiplusToken;

  GdiInit()
  {
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
  }

  ~GdiInit()
  {
    Gdiplus::GdiplusShutdown(gdiplusToken);
  }
};

namespace {
  GdiInit gdiInit;
}

namespace 
{
  const LPCWSTR className = L"SketchbookWindow";
  int windowCount = 0;
}

Window::Window(void) :
  m_isOpen(true),
  m_width(640),
  m_height(480),
  m_bgColor(Gdiplus::Color::Black)
{
  
  if (windowCount == 0) // only register once
  {
    registerWindowClass();
  }
  createWindow();
}

Window::~Window(void)
{

  if (m_handle)
  {
    DestroyWindow(m_handle);
  }

  windowCount--;

  // unregister window class if this is the last window
  if (windowCount == 0)
  {
    UnregisterClassW(className, GetModuleHandleW(NULL));
  }

}

void Window::registerWindowClass()
{

  // register the window class
  WNDCLASSW windowClass;
  windowClass.style         = 0;
  windowClass.lpfnWndProc   = &globalWindowsEvent;
  windowClass.cbClsExtra    = 0;
  windowClass.cbWndExtra    = 0;
  windowClass.hInstance     = GetModuleHandleA(NULL);
  windowClass.hIcon         = m_icon;
  windowClass.hCursor       = 0;
  windowClass.hbrBackground = 0;
  windowClass.lpszMenuName  = NULL;
  windowClass.lpszClassName = className;
  RegisterClassW(&windowClass);
}

void Window::createWindow()
{
  // create the window
  m_handle = CreateWindowW(
      className, 
      L"", 
      WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU, 
      CW_USEDEFAULT, CW_USEDEFAULT, 300, 200, 
      NULL, NULL, GetModuleHandle(NULL), this);

  ++windowCount;
}

HWND Window::getHandle()
{
  return m_handle;
}

void Window::show()
{
  if (!m_handle) return;
  ShowWindow(m_handle, SW_SHOW);
}

void Window::hide()
{
  if (!m_handle) return;
  ShowWindow(m_handle, SW_HIDE);
}

void Window::setSize(long width, long height)
{
  if (!m_handle) return;
  // adjust for the contents size not the window border size
  RECT rectangle = {0, 0, width, height};
  AdjustWindowRect(&rectangle, GetWindowLong(m_handle, GWL_STYLE), false);
  m_width  = rectangle.right - rectangle.left;
  m_height = rectangle.bottom - rectangle.top;

  SetWindowPos(m_handle, NULL, 0, 0, m_width, m_height, SWP_NOMOVE | SWP_NOZORDER);
}

void Window::setTitle(std::wstring title)
{
  if (!m_handle) return;
  SetWindowTextW(m_handle, title.c_str());
}

void Window::loadIcon(std::wstring filename)
{
  m_icon = (HICON) ::LoadImage(
    NULL,
    filename.c_str(),
    IMAGE_ICON,
    0,
    0,
    LR_LOADFROMFILE | LR_DEFAULTSIZE
  );
  ::SendMessage(m_handle, (UINT)WM_SETICON, ICON_BIG, (LPARAM)m_icon);
}

void Window::setBackgroundColor(Gdiplus::Color color)
{
  m_bgColor = color;
}

bool Window::isOpen()
{
  return m_isOpen;
}

bool Window::pollEvents(UIEvent& ev)
{
  // pump the messages
  processEvents();
  // return the top of the stack
  if (m_eventQueue.empty())
  {
    return false;
  }
  else
  {
    return m_eventQueue.try_pop(ev);
  }
}

void Window::processEvents()
{
  // message pump
  MSG message;
  while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
  {
      TranslateMessage(&message);
      DispatchMessageW(&message);
  }
}

void Window::addUIComponent(std::shared_ptr<UIComponent> component)
{
  m_uiComponents.push_back(component);
}

void Window::paint()
{
  PAINTSTRUCT ps;
  // this does shit all, it returns a useless hdc
  BeginPaint(m_handle, &ps);

  HDC windowDC = GetDC(m_handle);

  // our back buffer
  HDC memoryDC = CreateCompatibleDC(windowDC);
  HBITMAP memoryBitmap = CreateCompatibleBitmap(windowDC, m_width, m_height);
  SelectObject(memoryDC, memoryBitmap);

  // draw to the back buffer with this
  Gdiplus::Graphics graphics(memoryDC);

  // make it look nice
  graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
  graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQuality);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

  // fill the background
  Gdiplus::SolidBrush bgBrush(m_bgColor);
  graphics.FillRectangle(&bgBrush, 0, 0, m_width, m_height);

  // draw each componenet
  for (std::shared_ptr<UIComponent> component : m_uiComponents)
  {
    component->paint(graphics);
  }

  // swap buffers
  BitBlt(windowDC, 0, 0, m_width, m_height, memoryDC, 0, 0, SRCCOPY);

  // delete everything, these might be good as members for performance?
  DeleteDC(memoryDC);
  DeleteDC(windowDC);
  DeleteObject(memoryBitmap);

  EndPaint(m_handle, &ps);
}

void Window::propagate(UIEvent ev)
{
  for (std::shared_ptr<UIComponent> component : m_uiComponents)
  {
    component->trigger(ev);
  }
}

// called by the os

void Window::handleEvent(UINT message, WPARAM wParam, LPARAM lParam)
{ 
  UIEvent ev;
  switch (message) 
  {
  case WM_LBUTTONDOWN:
    ev.type = UIEvent::MouseDown;
    ev.mouseX = static_cast<int>(LOWORD(lParam));
    ev.mouseY = static_cast<int>(HIWORD(lParam));
    propagate(ev);
    m_eventQueue.push(ev);
    break;
  case WM_LBUTTONUP:
    ev.type = UIEvent::MouseUp;
    ev.mouseX = static_cast<int>(LOWORD(lParam));
    ev.mouseY = static_cast<int>(HIWORD(lParam));
    propagate(ev);
    m_eventQueue.push(ev);
    break;
  case WM_MOUSEMOVE:
    ev.type = UIEvent::MouseMove;
    ev.mouseX = static_cast<int>(LOWORD(lParam));
    ev.mouseY = static_cast<int>(HIWORD(lParam));
    propagate(ev);
    m_eventQueue.push(ev);
    break;
  case WM_CLOSE:
    m_isOpen = false;
    break;
  case WM_PAINT:
    paint();
    break;
  default:
    // some other event, pass it up the chain
    ev.type = UIEvent::WM;
    ev.wmMessage = message;
    propagate(ev);
    m_eventQueue.push(ev);
    break;
  }
}

LRESULT CALLBACK Window::globalWindowsEvent(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
{

  // associate handle with the window instance
  if (message == WM_CREATE)
  {
      LONG_PTR window = (LONG_PTR)reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams;
      SetWindowLongPtrW(handle, GWLP_USERDATA, window);
  }

  // get a pointer to the Window this event relates to
  Window* window = handle ? reinterpret_cast<Window*>(GetWindowLongPtr(handle, GWLP_USERDATA)) : NULL;

  // if the window is valid, forward the event
  if (window)
  {
    window->handleEvent(message, wParam, lParam);
  }

  // handle OS messages
  switch (message)
  {
    case WM_CLOSE:
      // dont let the os destroy the window
      return 0;
      break;
    case WM_DESTROY:
      PostQuitMessage(WM_QUIT);
      break;
    case WM_ERASEBKGND:
      return false; // we handle this in the draw
      break;
  }

  return DefWindowProcW(handle, message, wParam, lParam);
}