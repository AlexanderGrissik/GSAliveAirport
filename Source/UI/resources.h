// Copyright (c) 2026 Alexander Grissik
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// See LICENSE for details
// Shared between the C++ sources and App.rc so both reference the same resource ID.
// NOTE: rc.exe requires this file to end with a trailing newline (RC1004 otherwise).
#define IDR_APP_ICON 101
#define IDR_UI_HTML 102
#define IDR_UI_CSS 103
#define IDR_UI_JS 104
#define IDR_UI_LOGO 105

// Application version printed in the startup log. Keep the version values in
// App.rc, resource.h, and index.html in sync.
#define APP_VERSION_MAJOR 1
#define APP_VERSION_MINOR 0
#define APP_VERSION_PATCH 0
#define APP_VERSION "1.0.0"
