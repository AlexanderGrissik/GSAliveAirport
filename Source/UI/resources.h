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

// Application version - single source of truth. Consumed by:
//  - C++ (GSAliveAirportApp::s_Version -> startup log banner -> UI version chip)
//  - App.rc VS_VERSION_INFO (Windows Explorer .exe properties)
#define APP_VERSION_MAJOR 0
#define APP_VERSION_MINOR 6
#define APP_VERSION_PATCH 0
#define APP_VERSION "0.6.0"
