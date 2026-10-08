// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#include "GSAliveUI.h"
#include <chrono>
#include <cwchar>
#include <fstream>
#include <thread>
#include <utility>

namespace NS_GSAliveAirport
{

bool GSAliveUI::Initialize(const CommandHandler &commandHandler)
{
    if (m_alive.load()) return false;
    m_commandHandler = commandHandler;
    m_failed = false;
    m_pageReady = false;
    m_errorMessage.clear();

    using SetDpiAwarenessContext_t = BOOL(WINAPI *)(DPI_AWARENESS_CONTEXT);
    if (auto *setter = reinterpret_cast<SetDpiAwarenessContext_t>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext"))) {
        setter(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }

    if (!RegisterWindowClass()) return false;
    m_hwnd = CreateHostWindow();
    if (m_hwnd == nullptr) return false;
    m_alive.store(true);
    m_callbackLifetime = std::make_shared<int>(0);
    ShowWindow(m_hwnd, SW_SHOW);
    SetTimer(m_hwnd, s_InitTimer, s_InitTimeoutMs, nullptr);

    LogDiagnostic(L"initializing WebView2");
    const std::weak_ptr<int> LIFETIME = m_callbackLifetime;
    auto handler = Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [this, LIFETIME](HRESULT result, ICoreWebView2Environment *environment) -> HRESULT {
            if (LIFETIME.expired()) return S_OK;
            return OnEnvironmentCreated(result, environment);
        });
    const std::wstring USER_DATA_FOLDER = GetProfileFolder();
    const HRESULT RESULT = CreateCoreWebView2EnvironmentWithOptions(nullptr, USER_DATA_FOLDER.empty() ? nullptr : USER_DATA_FOLDER.c_str(), nullptr, handler.Get());
    if (FAILED(RESULT)) ReportError(L"Creating the WebView2 environment", RESULT);
    // Completion callbacks run through the application's normal message loop.
    return true;
}

HRESULT GSAliveUI::OnEnvironmentCreated(HRESULT result, ICoreWebView2Environment *environment)
{
    if (FAILED(result) || environment == nullptr) {
        ReportError(L"Creating the WebView2 environment", FAILED(result) ? result : E_POINTER);
        return S_OK;
    }

    LogDiagnostic(L"environment-ready");
    const std::weak_ptr<int> LIFETIME = m_callbackLifetime;
    auto handler = Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
        [this, LIFETIME](HRESULT controllerResult, ICoreWebView2Controller *controller) -> HRESULT {
            if (LIFETIME.expired()) {
                if (controller != nullptr) controller->Close();
                return S_OK;
            }
            return OnControllerCreated(controllerResult, controller);
        });
    const HRESULT RESULT = environment->CreateCoreWebView2Controller(m_hwnd, handler.Get());
    if (FAILED(RESULT)) ReportError(L"Creating the WebView2 controller", RESULT);
    return S_OK;
}

HRESULT GSAliveUI::OnControllerCreated(HRESULT result, ICoreWebView2Controller *controller)
{
    if (FAILED(result) || controller == nullptr) {
        ReportError(L"Creating the WebView2 controller", FAILED(result) ? result : E_POINTER);
        return S_OK;
    }
    // ComPtr assignment retains the callback's borrowed controller reference.
    m_controller = controller;
    const HRESULT RESULT = ConfigureWebView();
    if (FAILED(RESULT)) ReportError(L"Loading the application UI", RESULT);
    return S_OK;
}

HRESULT GSAliveUI::ConfigureWebView()
{
    HRESULT result = m_controller->get_CoreWebView2(m_webView.ReleaseAndGetAddressOf());
    if (FAILED(result)) return result;
    if (!m_webView) return E_POINTER;
    LogDiagnostic(L"webview-ready");
    SetWebViewBounds();
    result = m_controller->put_IsVisible(TRUE);
    if (FAILED(result)) return result;

    const std::weak_ptr<int> LIFETIME = m_callbackLifetime;
    auto messageHandler = Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
        [this, LIFETIME](ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
            if (LIFETIME.expired() || args == nullptr) return S_OK;
            LPWSTR text = nullptr;
            if (SUCCEEDED(args->TryGetWebMessageAsString(&text)) && text != nullptr)
            {
                HandleWebMessage(text);
                CoTaskMemFree(text);
            }
            return S_OK;
        });
    EventRegistrationToken messageToken{};
    result = m_webView->add_WebMessageReceived(messageHandler.Get(), &messageToken);
    if (FAILED(result)) return result;

    auto startingHandler = Microsoft::WRL::Callback<ICoreWebView2NavigationStartingEventHandler>(
        [this, LIFETIME](ICoreWebView2 *, ICoreWebView2NavigationStartingEventArgs *) -> HRESULT {
            if (LIFETIME.expired()) return S_OK;
            m_pageReady = false;
            SetTimer(m_hwnd, s_InitTimer, s_InitTimeoutMs, nullptr);
            return S_OK;
        });
    EventRegistrationToken startingToken{};
    result = m_webView->add_NavigationStarting(startingHandler.Get(), &startingToken);
    if (FAILED(result)) return result;

    auto completedHandler = Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
        [this, LIFETIME](ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT {
            if (LIFETIME.expired() || args == nullptr) return S_OK;
            BOOL success = FALSE;
            COREWEBVIEW2_WEB_ERROR_STATUS status = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
            args->get_IsSuccess(&success);
            args->get_WebErrorStatus(&status);
            LogDiagnostic(L"nav-completed success=" + std::to_wstring(success) + L" errorStatus=" + std::to_wstring(static_cast<int>(status)));
            if (!success && status != COREWEBVIEW2_WEB_ERROR_STATUS_OPERATION_CANCELED) {
                ReportError(L"Navigating to the application UI (WebView2 status " + std::to_wstring(static_cast<int>(status)) + L")", E_FAIL);
            }
            return S_OK;
        });
    EventRegistrationToken completedToken{};
    result = m_webView->add_NavigationCompleted(completedHandler.Get(), &completedToken);
    if (FAILED(result)) return result;

    Microsoft::WRL::ComPtr<ICoreWebView2_3> webView3;
    result = m_webView.As(&webView3);
    if (FAILED(result)) return result;
    result = webView3->SetVirtualHostNameToFolderMapping(L"gsalive", GetModuleDirectory().c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
    if (FAILED(result)) return result;
    LogDiagnostic(L"navigating https://gsalive/index.html");
    return m_webView->Navigate(L"https://gsalive/index.html");
}

void GSAliveUI::ReportError(std::wstring_view operation, HRESULT result)
{
    if (m_failed || !m_alive.load()) return;
    m_failed = true;
    wchar_t code[32]{};
    swprintf_s(code, L"0x%08lX", static_cast<unsigned long>(result));
    m_errorMessage = std::wstring(operation) + L" failed (" + code + L").\n\nCheck that the WebView2 Runtime and the application's UI files are available.";
    LogDiagnostic(m_errorMessage);
    KillTimer(m_hwnd, s_InitTimer);
    // Show modal UI only after the WebView2 callback has returned.
    PostMessageW(m_hwnd, s_WM_APP_ERROR, 0, 0);
}

void GSAliveUI::Shutdown()
{
    m_alive.store(false);
    m_pageReady = false;
    m_callbackLifetime.reset();
    if (m_hwnd != nullptr) KillTimer(m_hwnd, s_InitTimer);
    // Close detaches WebView2's handlers before releasing our owning references.
    if (m_controller) m_controller->Close();
    m_webView.Reset();
    m_controller.Reset();

    {
        std::scoped_lock lock(m_logMutex);
        m_logQueue.clear();
    }

    if (const HWND WINDOW = m_hwnd.exchange(nullptr); WINDOW != nullptr) DestroyWindow(WINDOW);
    m_commandHandler = nullptr;
}

void GSAliveUI::LogDiagnostic(const std::wstring &line)
{
    // Diagnostics file next to the exe (visible even when the UI itself is broken).
    const std::wstring path = GetModuleDirectory() + L"ui-load.log";
    std::wofstream file(path, std::ios::app);
    if (!file) return;
    file << line << L'\n';
}

void GSAliveUI::AppendLog(std::string_view text)
{
    {
        std::scoped_lock lock(m_logMutex);
        m_logQueue.push_back(TrimTrailingNewline(text));
    }
    if (m_alive.load() && m_hwnd != nullptr) PostMessageW(m_hwnd, s_WM_APP_LOG, 0, 0);
}

bool GSAliveUI::RegisterWindowClass()
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WndProcThunk;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = s_WindowClassName;
    const ATOM atom = RegisterClassExW(&windowClass);
    return atom != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

HWND GSAliveUI::CreateHostWindow()
{
    const int DESKTOP_WIDTH = GetSystemMetrics(SM_CXSCREEN);
    const int DESKTOP_HEIGHT = GetSystemMetrics(SM_CYSCREEN);
    const int WIDTH = 1180;
    const int HEIGHT = 740;
    return CreateWindowExW(0, s_WindowClassName, L"GSAliveAirport", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, (DESKTOP_WIDTH - WIDTH) / 2, (DESKTOP_HEIGHT - HEIGHT) / 2, WIDTH, HEIGHT, nullptr, nullptr, GetModuleHandleW(nullptr), this);
}

void GSAliveUI::SetWebViewBounds()
{
    if (m_controller == nullptr || m_hwnd == nullptr) return;
    RECT bounds{};
    GetClientRect(m_hwnd, &bounds);
    m_controller->SetBoundsAndZoomFactor(bounds, 1.0);
}

std::wstring GSAliveUI::GetModuleDirectory() const
{
    wchar_t buffer[4096]{};
    if (GetModuleFileNameW(nullptr, buffer, 4096) == 0) return {};
    const std::wstring path(buffer);
    const auto lastSlash = path.find_last_of(L'\\');
    if (lastSlash == std::wstring::npos) return path;
    return path.substr(0, lastSlash + 1);
}

void GSAliveUI::FlushLogQueue()
{
    if (!m_pageReady || !m_webView) return;
    std::deque<std::string> lines;
    {
        std::scoped_lock lock(m_logMutex);
        lines.swap(m_logQueue);
    }
    if (lines.empty()) return;

    // Plain string payload: lines joined with newlines (the page splits them back).
    std::wstring payload;
    for (const std::string &line : lines)
    {
        if (!payload.empty()) payload += L'\n';
        payload += ToWideUtf8(line);
    }
    const HRESULT RESULT = m_webView->PostWebMessageAsString(payload.c_str());
    if (FAILED(RESULT)) ReportError(L"Sending UI log messages", RESULT);
}

void GSAliveUI::HandleWebMessage(std::wstring_view text)
{
    if (!m_alive.load() || m_failed) return;
    if (text == L"ready")
    {
        m_pageReady = true;
        if (m_hwnd != nullptr) KillTimer(m_hwnd, s_InitTimer);
        LogDiagnostic(L"page-ready");
        if (m_hwnd != nullptr) PostMessageW(m_hwnd, s_WM_APP_LOG, 0, 0);
        return;
    }
    if (!m_pageReady) return;
    // Same UI thread as the window procedure: run the command handler directly.
    const std::string command = ToNarrowUtf8(text);
    if (command.empty() || !m_commandHandler) return;
    m_commandHandler(command);
}

LRESULT CALLBACK GSAliveUI::WndProcThunk(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    GSAliveUI *self = nullptr;
    if (message == WM_NCCREATE)
    {
        const auto *createStruct = reinterpret_cast<CREATESTRUCTW *>(lParam);
        self = static_cast<GSAliveUI *>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<GSAliveUI *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self == nullptr) return DefWindowProcW(hwnd, message, wParam, lParam);
    return self->WndProc(hwnd, message, wParam, lParam);
}

LRESULT GSAliveUI::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {

    case s_WM_APP_LOG:
        FlushLogQueue();
        return 0;
    case s_WM_APP_ERROR:
        MessageBoxW(hwnd, m_errorMessage.c_str(), L"GSAliveAirport - UI error", MB_OK | MB_ICONERROR);
        Shutdown();
        return 0;
    case WM_TIMER:
        if (wParam == s_InitTimer) ReportError(L"Waiting for the application UI to become ready", HRESULT_FROM_WIN32(ERROR_TIMEOUT));
        return 0;
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) SetWebViewBounds();
        return 0;
    case WM_CLOSE:
        Shutdown();
        return 0;
    case WM_DESTROY:
        m_hwnd = nullptr;
        PostQuitMessage(m_failed ? 1 : 0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

std::wstring GSAliveUI::ToWideUtf8(std::string_view text)
{
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

std::string GSAliveUI::ToNarrowUtf8(std::wstring_view text)
{
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring GSAliveUI::GetProfileFolder()
{
    wchar_t localAppData[MAX_PATH]{};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH) == 0) return {};
    return std::wstring(localAppData) + L"\\GSAliveAirport";
}

namespace
{
bool RemoveFolderTree(const std::wstring &path)
{
    WIN32_FIND_DATAW find{};
    const HANDLE handle = FindFirstFileW((path + L"\\*").c_str(), &find);
    if (handle == INVALID_HANDLE_VALUE)
    {
        const BOOL removed = RemoveDirectoryW(path.c_str());
        return removed || GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND;
    }
    bool complete = true;
    do
    {
        const std::wstring name(find.cFileName);
        if (name == L"." || name == L"..") continue;
        const std::wstring child = path + L"\\" + name;
        if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            complete = RemoveFolderTree(child) && complete;
        }
        else
        {
            SetFileAttributesW(child.c_str(), find.dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);
            complete = DeleteFileW(child.c_str()) && complete;
        }
    } while (FindNextFileW(handle, &find));
    FindClose(handle);
    return complete && RemoveDirectoryW(path.c_str()) != FALSE;
}
} // namespace

void GSAliveUI::DeleteProfileFolder()
{
    // Wait for the WebView2 browser processes to release their profile files,
    // then remove the folder so nothing is left behind after a clean exit.
    const std::wstring folder = GetProfileFolder();
    if (folder.empty()) return;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    for (;;)
    {
        if (RemoveFolderTree(folder)) return;
        if (std::chrono::steady_clock::now() >= deadline) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

std::string GSAliveUI::TrimTrailingNewline(std::string_view text)
{
    std::string result(text);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
    {
        result.pop_back();
    }
    return result;
}

} // namespace NS_GSAliveAirport
