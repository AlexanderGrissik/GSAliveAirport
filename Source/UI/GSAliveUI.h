// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
#pragma once

// WebView2.h is a MIDL-generated COM header: it needs the full <windows.h>
// (combaseapi.h defines the `interface` macro) plus the SDK "winrt" include
// directory for EventToken.h. So WIN32_LEAN_AND_MEAN must not be active here.
#ifdef WIN32_LEAN_AND_MEAN
# undef WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <combaseapi.h>
#include <WebView2.h>
#include <wrl.h>

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace NS_GSAliveAirport
{
class GSAliveUI final
{
public:
    using CommandHandler = std::function<void(std::string_view)>;

    GSAliveUI() = default;
    ~GSAliveUI() { Shutdown(); }
    GSAliveUI(const GSAliveUI &) = delete;
    GSAliveUI &operator=(const GSAliveUI &) = delete;

    bool Initialize(const CommandHandler &commandHandler);
    void Shutdown();
    void AppendLog(std::string_view text);
    void PostMessageUI(const std::string &str);
    void RequestClose() { if (m_hwnd != nullptr) PostMessageW(m_hwnd, WM_CLOSE, 0, 0); }

private:
    static constexpr wchar_t s_WindowClassName[] = L"GSAliveAirportUIWindow";

    static constexpr UINT s_WM_APP_LOG = WM_APP + 2;
    static constexpr UINT s_WM_APP_ERROR = WM_APP + 3;
    static constexpr UINT s_WM_APP_UI = WM_APP + 4;
    static constexpr UINT_PTR s_InitTimer = 1;
    static constexpr DWORD s_InitTimeoutMs = 60'000;

    HRESULT OnEnvironmentCreated(HRESULT result, ICoreWebView2Environment *environment);
    HRESULT OnControllerCreated(HRESULT result, ICoreWebView2Controller *controller);
    HRESULT ConfigureWebView();
    void ReportError(std::wstring_view operation, HRESULT result);
    bool RegisterWindowClass();
    HWND CreateHostWindow();
    void SetWebViewBounds();
    std::wstring GetModuleDirectory() const;
    void FlushLogQueue();
    void FlushUiQueue();
    void HandleWebMessage(std::wstring_view text);
    static std::wstring GetProfileFolder();
    void LogDiagnostic(const std::wstring &line);

    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    static std::wstring ToWideUtf8(std::string_view text);
    static std::string ToNarrowUtf8(std::wstring_view text);
    static std::string TrimTrailingNewline(std::string_view text);

    CommandHandler m_commandHandler;
    std::atomic_bool m_alive{false};
    std::atomic<HWND> m_hwnd{nullptr};
    // Callbacks run on the UI thread; invalidate them before shutdown or destruction.
    std::shared_ptr<int> m_callbackLifetime;

    Microsoft::WRL::ComPtr<ICoreWebView2Controller> m_controller;
    Microsoft::WRL::ComPtr<ICoreWebView2> m_webView;



    bool m_pageReady{false};
    bool m_failed{false};
    std::wstring m_errorMessage;
    std::mutex m_logMutex;
    std::deque<std::string> m_logQueue;
    std::mutex m_uiMutex;
    std::deque<std::string> m_uiQueue;
};
} // namespace NS_GSAliveAirport
