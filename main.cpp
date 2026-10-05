// Jampus Client
// A lightweight, frameless game launcher built on Win32, Direct3D 11 and Dear ImGui.
//
// The only process this application ever starts is JampusStrike.exe, located in the
// same directory as the launcher executable. It performs no networking, no
// authentication, no downloads or updates, and installs no tray icon or services.

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <objbase.h>
#include <dwmapi.h>
#include <wincodec.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h" // ImHashData and the nav-cursor visibility flag.
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "windowscodecs.lib")

// Declared in imgui_impl_win32.h behind #if 0 so callers opt in explicitly.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using Microsoft::WRL::ComPtr;

namespace {

// ---------------------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------------------

constexpr wchar_t kWindowClassName[] = L"JampusClientWindow";
constexpr wchar_t kWindowTitle[] = L"Jampus Client";
constexpr wchar_t kTargetExecutable[] = L"JampusStrike.exe";
constexpr wchar_t kAssetsFolder[] = L"assets";

constexpr char kVersionText[] = "v1.1.1";
constexpr char kUpdatingMessage[] = "This module is currently being updated. Check back soon.";
constexpr char kMissingExecutableMessage[] = "Place JampusStrike.exe in the same folder as Jampus Client, then try again.";
constexpr char kModalPopupId[] = "##jampus_modal";

// Layout metrics in logical (96 DPI) pixels; scaled by the window's DPI at runtime.
constexpr float kWindowWidth = 700.0f;
constexpr float kWindowHeight = 500.0f;
constexpr float kTitleBarHeight = 40.0f;
constexpr float kTitleButtonWidth = 46.0f;
constexpr float kTitleButtonInset = 6.0f;
constexpr float kTitleGlyphSize = 10.0f;
constexpr float kContentPadding = 24.0f;
constexpr float kHeaderTopPadding = 22.0f;
constexpr float kHeaderToGrid = 18.0f;
constexpr float kButtonHeight = 36.0f;
constexpr float kButtonRounding = 9.0f;
constexpr float kFooterHeight = 40.0f;

// Game cards (kept to their established design).
constexpr float kCardGap = 14.0f;
constexpr float kCardPadding = 14.0f;
constexpr float kCardMinWidth = 180.0f;
constexpr float kNameToStatusGap = 8.0f;
constexpr float kCardRounding = 12.0f;
constexpr float kCardButtonHeight = 34.0f;
constexpr float kCardButtonRounding = 6.0f;
constexpr float kBannerAspect = 16.0f / 9.0f; // Artwork banner across the top of each card.
constexpr float kArtHoverZoom = 0.04f;        // Extra zoom applied to the artwork on hover.
constexpr float kHoverAnimationSpeed = 12.0f;

// Dialogs.
constexpr float kModalRounding = 16.0f;
constexpr float kModalPadding = 24.0f;
constexpr float kNoticeWidth = 400.0f;
constexpr float kNoticeIconSize = 40.0f;
constexpr float kPrelaunchWidth = 520.0f;
constexpr float kPrelaunchHeight = 376.0f;
constexpr float kPrelaunchHeroHeight = 118.0f;
constexpr float kOptionHeight = 66.0f;
constexpr float kOptionGap = 10.0f;
constexpr float kStageRowHeight = 58.0f;

// Font sizes in logical pixels.
constexpr float kFontBodySize = 14.0f;
constexpr float kFontCaptionSize = 12.5f;
constexpr float kFontLabelSize = 14.0f;
constexpr float kFontChipSize = 12.0f;
constexpr float kFontEyebrowSize = 11.0f;
constexpr float kFontCardTitleSize = 16.0f;
constexpr float kFontCardButtonSize = 14.0f;
constexpr float kFontHeadingSize = 25.0f;
constexpr float kFontModalTitleSize = 21.0f;
constexpr float kFontNoticeTitleSize = 16.5f;

// Motion (seconds, or response rates for exponential smoothing).
constexpr float kHoverSpeed = 14.0f;
constexpr float kPressSpeed = 22.0f;
constexpr float kModalEnterSeconds = 0.22f;
constexpr float kContentFadeSeconds = 0.26f;
constexpr float kCheckDrawSeconds = 0.32f;
constexpr float kSpinnerTurnsPerSecond = 0.9f;
constexpr DWORD kBackgroundWaitMs = 100;

// Palette: layered graphite surfaces, one iris accent, and status colours used sparingly.
// Surface, text, accent and status values are the launcher's established colours.
constexpr ImU32 kColBackground = IM_COL32(12, 13, 19, 255);
constexpr ImU32 kColBackgroundTop = IM_COL32(19, 19, 30, 255);
constexpr ImU32 kColSurface = IM_COL32(23, 24, 34, 255);
constexpr ImU32 kColSurfaceRaised = IM_COL32(31, 32, 44, 255);
constexpr ImU32 kColSurfaceSunken = IM_COL32(17, 18, 26, 255);
constexpr ImU32 kColHairline = IM_COL32(255, 255, 255, 15);
constexpr ImU32 kColHairlineStrong = IM_COL32(255, 255, 255, 30);
constexpr ImU32 kColHighlight = IM_COL32(255, 255, 255, 9);
constexpr ImU32 kColShadow = IM_COL32(0, 0, 0, 255);
constexpr ImU32 kColText = IM_COL32(237, 238, 240, 255);
constexpr ImU32 kColTextSoft = IM_COL32(186, 190, 197, 255);
constexpr ImU32 kColTextMuted = IM_COL32(139, 144, 152, 255);
constexpr ImU32 kColTextDim = IM_COL32(92, 97, 105, 255);
constexpr ImU32 kColAccent = IM_COL32(163, 146, 255, 255);
constexpr ImU32 kColAccentHover = IM_COL32(186, 172, 255, 255);
constexpr ImU32 kColAccentActive = IM_COL32(140, 121, 232, 255);
constexpr ImU32 kColAccentText = IM_COL32(186, 172, 255, 255);
constexpr ImU32 kColOnAccent = IM_COL32(19, 15, 35, 255);
constexpr ImU32 kColReady = IM_COL32(61, 214, 140, 255);
constexpr ImU32 kColUpdating = IM_COL32(245, 165, 36, 255);
constexpr ImU32 kColError = IM_COL32(244, 104, 104, 255);
constexpr ImU32 kColCardBorder = IM_COL32(255, 255, 255, 16);
constexpr ImU32 kColDisabledFill = IM_COL32(255, 255, 255, 12);
constexpr ImU32 kColDisabledBorder = IM_COL32(255, 255, 255, 18);
constexpr ImU32 kColDisabledText = IM_COL32(122, 127, 135, 255);
constexpr ImU32 kColCloseHover = IM_COL32(196, 43, 28, 255);
constexpr ImU32 kColCloseActive = IM_COL32(160, 32, 20, 255);
constexpr ImU32 kColModalDim = IM_COL32(4, 5, 9, 185);
constexpr ImU32 kColGlass = IM_COL32(10, 11, 16, 200);
constexpr COLORREF kWindowFillColor = RGB(12, 13, 19);
constexpr COLORREF kNativeBorderColor = RGB(40, 42, 52);

// DWM attributes introduced in Windows 11; defined locally so older SDKs still compile.
constexpr DWORD kDwmWindowCornerPreference = 33; // DWMWA_WINDOW_CORNER_PREFERENCE
constexpr DWORD kDwmBorderColor = 34;            // DWMWA_BORDER_COLOR
constexpr int kDwmCornerRound = 2;               // DWMWCP_ROUND

// Glyphs baked from the system fonts: Basic Latin, Latin-1 and general punctuation.
constexpr ImWchar kGlyphRanges[] = { 0x0020, 0x00FF, 0x2010, 0x2027, 0 };

// ---------------------------------------------------------------------------------------
// Game catalogue
// ---------------------------------------------------------------------------------------

enum class GameStatus { Ready, Updating };
enum class LoadAction { LaunchJampusStrike, ShowUpdating };

struct GameEntry {
    const char* name;
    GameStatus status;
    LoadAction action;
    const wchar_t* artFile; // Banner artwork inside the assets folder.
    float artFocusX;        // Point of interest (0..1) kept in frame when the art is cropped.
    float artFocusY;
};

constexpr GameEntry kGames[] = {
    { "Counter-Strike 2", GameStatus::Ready, LoadAction::LaunchJampusStrike, L"cs2.jpg", 0.5f, 0.45f },
    { "Roblox", GameStatus::Updating, LoadAction::ShowUpdating, L"roblox.png", 0.5f, 0.5f },
    { "Call of Duty: Modern Warfare 2019", GameStatus::Updating, LoadAction::ShowUpdating, L"cod.png", 0.82f, 0.5f },
};
constexpr int kGameCount = static_cast<int>(sizeof(kGames) / sizeof(kGames[0]));

// ---------------------------------------------------------------------------------------
// Application state
// ---------------------------------------------------------------------------------------

enum class ModalKind { None, Updating, ExecutableMissing, LaunchFailed, Prelaunch, Simulating, SimulationComplete };

struct ModalState {
    ModalKind kind = ModalKind::None;
    std::string message;
    bool openRequested = false;
    int gameIndex = 0;            // Card that opened the dialog.
    ULONGLONG simulationStarted = 0;
    double openedAt = 0.0;        // ImGui time stamps driving the dialog's transitions.
    double stageChangedAt = 0.0;
    double completedAt = 0.0;
};

// Actions requested by the UI are executed after the frame has been presented, so that
// window-state changes and process launches never happen in the middle of an ImGui frame.
enum class PendingAction { None, Minimize, Close, LaunchJampusStrike };

struct Fonts {
    ImFont* body = nullptr;
    ImFont* caption = nullptr;
    ImFont* label = nullptr;
    ImFont* chip = nullptr;
    ImFont* eyebrow = nullptr;
    ImFont* cardTitle = nullptr;
    ImFont* cardButton = nullptr;
    ImFont* heading = nullptr;
    ImFont* modalTitle = nullptr;
    ImFont* noticeTitle = nullptr;
};

// Decoded artwork is kept on the CPU so textures can be rebuilt after a device loss.
struct GameArt {
    std::vector<unsigned char> pixels; // RGBA8
    UINT width = 0;
    UINT height = 0;
    ComPtr<ID3D11ShaderResourceView> texture;
};

struct InitState {
    bool com = false;
    bool windowClass = false;
    bool imguiContext = false;
    bool win32Backend = false;
    bool dx11Backend = false;
};

HINSTANCE g_instance = nullptr;
HWND g_hwnd = nullptr;
HBRUSH g_backgroundBrush = nullptr;
InitState g_init;

ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_context;
ComPtr<IDXGISwapChain> g_swapChain;
ComPtr<ID3D11RenderTargetView> g_renderTarget;

float g_dpiScale = 1.0f;
bool g_fontsDirty = false;
bool g_nativeRoundedCorners = false;
UINT g_pendingWidth = 0;
UINT g_pendingHeight = 0;

Fonts g_fonts;
GameArt g_art[kGameCount];
ModalState g_modal;
PendingAction g_pendingAction = PendingAction::None;
bool g_launchSucceeded = false;
bool g_animating = false;
float g_cardHover[kGameCount] = {};
std::unordered_map<ImGuiID, float> g_anim; // Smoothed hover/press values for dialog controls.

// ---------------------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------------------

float Px(float logical) {
    return std::floor(logical * g_dpiScale + 0.5f);
}

ImVec2 FloorVec(ImVec2 v) {
    return ImVec2(std::floor(v.x), std::floor(v.y));
}

ImVec4 ToVec4(ImU32 color) {
    return ImGui::ColorConvertU32ToFloat4(color);
}

ImU32 WithAlpha(ImU32 color, unsigned alpha) {
    return (color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT);
}

ImU32 LerpColor(ImU32 a, ImU32 b, float t) {
    const ImVec4 va = ToVec4(a);
    const ImVec4 vb = ToVec4(b);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(va.x + (vb.x - va.x) * t, va.y + (vb.y - va.y) * t,
                                                 va.z + (vb.z - va.z) * t, va.w + (vb.w - va.w) * t));
}

ImVec2 MeasureText(ImFont* font, const char* text, float wrapWidth = 0.0f) {
    return font->CalcTextSizeA(font->FontSize, FLT_MAX, wrapWidth, text);
}

void DrawString(ImDrawList* drawList, ImFont* font, ImVec2 pos, ImU32 color, const char* text, float wrapWidth = 0.0f) {
    drawList->AddText(font, font->FontSize, FloorVec(pos), color, text, nullptr, wrapWidth);
}

ImTextureID ToTextureId(ID3D11ShaderResourceView* view) {
    return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(view));
}

float TitleBarHeightPx() {
    return Px(kTitleBarHeight);
}

// Left edge (client coordinates) of the minimize/close button group. Shared by the hit
// test and the renderer so the draggable region never overlaps the buttons.
float TitleButtonsLeftPx(float clientWidth) {
    return clientWidth - 2.0f * Px(kTitleButtonWidth);
}

std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty())
        return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return {};
    std::string utf8(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), utf8.data(), size, nullptr, nullptr);
    return utf8;
}

void LogError(const wchar_t* message) {
    OutputDebugStringW(L"[Jampus Client] ");
    OutputDebugStringW(message);
    OutputDebugStringW(L"\n");
}

// ---------------------------------------------------------------------------------------
// File system and launching
// ---------------------------------------------------------------------------------------

// Returns the directory containing the running executable, or an empty string on failure.
std::wstring GetLauncherDirectory() {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0)
            return {};
        if (length < path.size()) {
            path.resize(length);
            break;
        }
        if (path.size() >= 32768)
            return {};
        path.resize(path.size() * 2);
    }

    const size_t separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos)
        return {};

    std::wstring directory = path.substr(0, separator);
    if (!directory.empty() && directory.back() == L':')
        directory.push_back(L'\\'); // Keep drive roots absolute ("C:\" rather than "C:").
    return directory;
}

std::wstring JoinPath(const std::wstring& directory, const wchar_t* fileName) {
    std::wstring result = directory;
    if (!result.empty() && result.back() != L'\\' && result.back() != L'/')
        result.push_back(L'\\');
    result += fileName;
    return result;
}

bool IsExistingFile(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::string DescribeShellExecuteError(INT_PTR code) {
    const char* reason = "An unexpected error occurred.";
    switch (code) {
    case 0:
    case SE_ERR_OOM: reason = "The system is out of memory or resources."; break;
    case ERROR_FILE_NOT_FOUND: reason = "The file could not be found."; break;
    case ERROR_PATH_NOT_FOUND: reason = "The path could not be found."; break;
    case ERROR_BAD_FORMAT: reason = "The file is not a valid Windows application."; break;
    case SE_ERR_ACCESSDENIED: reason = "Access was denied, the file is not a valid application, or elevation was cancelled."; break;
    case SE_ERR_SHARE: reason = "A sharing violation occurred."; break;
    case SE_ERR_ASSOCINCOMPLETE:
    case SE_ERR_NOASSOC: reason = "No application is associated with this file."; break;
    case SE_ERR_DDETIMEOUT:
    case SE_ERR_DDEFAIL:
    case SE_ERR_DDEBUSY: reason = "Windows could not complete the launch request."; break;
    case SE_ERR_DLLNOTFOUND: reason = "A required library was not found."; break;
    default: break;
    }

    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "Windows could not start JampusStrike.exe. %s (Error code %d)",
                  reason, static_cast<int>(code));
    return buffer;
}

void ShowModal(ModalKind kind, std::string message, int gameIndex = 0) {
    g_modal = ModalState{};
    g_modal.kind = kind;
    g_modal.message = std::move(message);
    g_modal.gameIndex = gameIndex;
    g_modal.openRequested = true;
    g_modal.openedAt = ImGui::GetTime();
}

void LaunchJampusStrike() {
    const std::wstring directory = GetLauncherDirectory();
    if (directory.empty()) {
        ShowModal(ModalKind::LaunchFailed, "The launcher could not determine its own folder, so JampusStrike.exe cannot be started.");
        return;
    }

    const std::wstring executablePath = JoinPath(directory, kTargetExecutable);
    if (!IsExistingFile(executablePath)) {
        ShowModal(ModalKind::ExecutableMissing, kMissingExecutableMessage);
        return;
    }

    // ShellExecuteExW is the ShellExecuteW call with options: the same verb, file, working
    // directory and show command, returning the same >32 success code in hInstApp.
    // SEE_MASK_FLAG_NO_UI plus a critical-error mode stop Windows from showing its own
    // native error dialogs (e.g. "Unsupported 16-Bit Application"), so every failure is
    // reported by the in-app modal. SEE_MASK_NOASYNC completes the launch before the call
    // returns, because the launcher exits immediately afterwards.
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
    info.hwnd = g_hwnd;
    info.lpVerb = L"open";
    info.lpFile = executablePath.c_str();
    info.lpDirectory = directory.c_str();
    info.nShow = SW_SHOWNORMAL;

    DWORD previousErrorMode = 0;
    const BOOL errorModeSet = SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &previousErrorMode);
    const BOOL launched = ShellExecuteExW(&info);
    if (errorModeSet)
        SetThreadErrorMode(previousErrorMode, nullptr);

    const INT_PTR code = reinterpret_cast<INT_PTR>(info.hInstApp);
    if (launched && code > 32) {
        // Windows reported success: close the launcher through the normal shutdown path.
        g_launchSucceeded = true;
        PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
        return;
    }

    ShowModal(ModalKind::LaunchFailed, DescribeShellExecuteError(code));
}

// ---------------------------------------------------------------------------------------
// Game artwork (decoded with the Windows Imaging Component)
// ---------------------------------------------------------------------------------------

bool DecodeImageRgba(IWICImagingFactory* factory, const std::wstring& path, GameArt& art) {
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)))
        return false;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame)))
        return false;
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter)))
        return false;
    if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                     WICBitmapPaletteTypeCustom)))
        return false;

    UINT width = 0;
    UINT height = 0;
    if (FAILED(converter->GetSize(&width, &height)) || width == 0 || height == 0 || width > 8192 || height > 8192)
        return false;

    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    if (FAILED(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data())))
        return false;

    art.pixels = std::move(pixels);
    art.width = width;
    art.height = height;
    return true;
}

// Loads each card's artwork from "<launcher>\assets", falling back to "<launcher>\..\assets"
// for development layouts. Missing artwork is not an error: the card renders without it.
void LoadGameArt() {
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) {
        LogError(L"WIC is unavailable; game artwork will not be shown.");
        return;
    }

    const std::wstring launcherDir = GetLauncherDirectory();
    if (launcherDir.empty())
        return;
    const std::wstring searchDirs[] = { JoinPath(launcherDir, kAssetsFolder), JoinPath(JoinPath(launcherDir, L".."), kAssetsFolder) };

    for (int i = 0; i < kGameCount; ++i) {
        for (const std::wstring& dir : searchDirs) {
            const std::wstring path = JoinPath(dir, kGames[i].artFile);
            if (IsExistingFile(path) && DecodeImageRgba(factory.Get(), path, g_art[i]))
                break;
        }
    }
}

// Creates a mipmapped texture so artwork stays smooth when drawn smaller than its source.
bool CreateTextureFromRgba(const unsigned char* pixels, UINT width, UINT height, ComPtr<ID3D11ShaderResourceView>& view) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 0; // Full mip chain.
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(g_device->CreateTexture2D(&desc, nullptr, &texture)))
        return false;
    if (FAILED(g_device->CreateShaderResourceView(texture.Get(), nullptr, &view)))
        return false;

    g_context->UpdateSubresource(texture.Get(), 0, nullptr, pixels, width * 4, 0);
    g_context->GenerateMips(view.Get());
    return true;
}

void CreateArtTextures() {
    for (GameArt& art : g_art) {
        if (!art.pixels.empty())
            CreateTextureFromRgba(art.pixels.data(), art.width, art.height, art.texture);
    }
}

void ReleaseArtTextures() {
    for (GameArt& art : g_art)
        art.texture.Reset();
}

// ---------------------------------------------------------------------------------------
// Direct3D 11
// ---------------------------------------------------------------------------------------

bool CreateRenderTarget() {
    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
        return false;
    return SUCCEEDED(g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &g_renderTarget));
}

void CleanupDeviceD3D() {
    ReleaseArtTextures();
    g_renderTarget.Reset();
    if (g_context)
        g_context->ClearState();
    g_swapChain.Reset();
    g_context.Reset();
    g_device.Reset();
}

bool CreateDeviceD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferCount = 2;
    desc.BufferDesc.Width = 0;
    desc.BufferDesc.Height = 0;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferDesc.RefreshRate.Numerator = 60;
    desc.BufferDesc.RefreshRate.Denominator = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = hwnd;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL obtained = {};

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, ARRAYSIZE(levels),
                                               D3D11_SDK_VERSION, &desc, &g_swapChain, &g_device, &obtained, &g_context);
    if (FAILED(hr)) {
        // Fall back to the WARP software rasterizer when no suitable hardware device exists.
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, ARRAYSIZE(levels),
                                           D3D11_SDK_VERSION, &desc, &g_swapChain, &g_device, &obtained, &g_context);
    }
    if (FAILED(hr)) {
        LogError(L"Direct3D 11 device creation failed.");
        CleanupDeviceD3D();
        return false;
    }

    // The launcher is a fixed-size window; never let DXGI switch it to fullscreen.
    ComPtr<IDXGIFactory> factory;
    if (SUCCEEDED(g_swapChain->GetParent(IID_PPV_ARGS(&factory))))
        factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    if (!CreateRenderTarget()) {
        LogError(L"Render target creation failed.");
        CleanupDeviceD3D();
        return false;
    }

    CreateArtTextures();
    return true;
}

// Rebuilds the device after removal/reset, keeping the ImGui context intact.
bool RecreateGraphics() {
    if (g_init.dx11Backend) {
        ImGui_ImplDX11_Shutdown();
        g_init.dx11Backend = false;
    }
    CleanupDeviceD3D();
    if (!CreateDeviceD3D(g_hwnd))
        return false;
    if (!ImGui_ImplDX11_Init(g_device.Get(), g_context.Get())) {
        LogError(L"Dear ImGui DX11 backend re-initialization failed.");
        return false;
    }
    g_init.dx11Backend = true;
    return true;
}

bool ApplyPendingResize() {
    if (g_pendingWidth == 0 || g_pendingHeight == 0)
        return true;

    const UINT width = g_pendingWidth;
    const UINT height = g_pendingHeight;
    g_pendingWidth = g_pendingHeight = 0;

    g_context->OMSetRenderTargets(0, nullptr, nullptr);
    g_renderTarget.Reset();
    if (FAILED(g_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0)))
        return RecreateGraphics();
    return CreateRenderTarget() || RecreateGraphics();
}

// ---------------------------------------------------------------------------------------
// Fonts and style
// ---------------------------------------------------------------------------------------

std::wstring GetSystemFontPath(const wchar_t* fileName) {
    wchar_t windowsDir[MAX_PATH] = {};
    const UINT length = GetWindowsDirectoryW(windowsDir, MAX_PATH);
    const std::wstring base = (length > 0 && length < MAX_PATH) ? std::wstring(windowsDir, length) : std::wstring(L"C:\\Windows");
    return JoinPath(JoinPath(base, L"Fonts"), fileName);
}

// Loads the first available system font from the candidate list, or falls back to the
// built-in ImGui font at the same size.
ImFont* LoadFont(std::initializer_list<const wchar_t*> candidates, float logicalSize) {
    ImGuiIO& io = ImGui::GetIO();
    const float sizePx = Px(logicalSize);

    for (const wchar_t* fileName : candidates) {
        const std::wstring path = GetSystemFontPath(fileName);
        if (!IsExistingFile(path))
            continue;
        ImFontConfig config;
        config.PixelSnapH = true;
        config.OversampleH = 3;
        if (ImFont* font = io.Fonts->AddFontFromFileTTF(WideToUtf8(path).c_str(), sizePx, &config, kGlyphRanges))
            return font;
    }

    ImFontConfig fallback;
    fallback.SizePixels = sizePx;
    return io.Fonts->AddFontDefault(&fallback);
}

void BuildFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    // Segoe UI Regular for reading text, Semibold for titles and controls, and Bold only
    // for the game cards and small tracked labels. Semibold falls back to Bold if missing.
    const auto regular = { L"segoeui.ttf" };
    const auto semibold = { L"seguisb.ttf", L"segoeuib.ttf" };
    const auto bold = { L"segoeuib.ttf" };

    // The first font added becomes the default font.
    g_fonts.body = LoadFont(regular, kFontBodySize);
    g_fonts.caption = LoadFont(regular, kFontCaptionSize);
    g_fonts.label = LoadFont(semibold, kFontLabelSize);
    g_fonts.chip = LoadFont(bold, kFontChipSize);
    g_fonts.eyebrow = LoadFont(bold, kFontEyebrowSize);
    g_fonts.cardTitle = LoadFont(bold, kFontCardTitleSize);
    g_fonts.cardButton = LoadFont(bold, kFontCardButtonSize);
    g_fonts.heading = LoadFont(semibold, kFontHeadingSize);
    g_fonts.modalTitle = LoadFont(semibold, kFontModalTitleSize);
    g_fonts.noticeTitle = LoadFont(semibold, kFontNoticeTitleSize);

    io.FontDefault = g_fonts.body;
}

void ApplyStyle() {
    ImGuiStyle style;
    ImGui::StyleColorsDark(&style);

    style.WindowRounding = 0.0f;
    style.WindowBorderSize = 0.0f;
    style.WindowPadding = ImVec2(20.0f, 20.0f);
    style.PopupRounding = 16.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameRounding = kButtonRounding;
    style.FramePadding = ImVec2(12.0f, 8.0f);
    style.ItemSpacing = ImVec2(10.0f, 10.0f);
    style.ScrollbarSize = 10.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ToVec4(kColBackground);
    colors[ImGuiCol_PopupBg] = ToVec4(kColSurface);
    colors[ImGuiCol_Border] = ToVec4(kColHairlineStrong);
    colors[ImGuiCol_Text] = ToVec4(kColText);
    colors[ImGuiCol_TextDisabled] = ToVec4(kColTextMuted);
    colors[ImGuiCol_Button] = ToVec4(kColAccent);
    colors[ImGuiCol_ButtonHovered] = ToVec4(kColAccentHover);
    colors[ImGuiCol_ButtonActive] = ToVec4(kColAccentActive);
    colors[ImGuiCol_ModalWindowDimBg] = ToVec4(kColModalDim);
    colors[ImGuiCol_NavCursor] = ToVec4(kColAccent);

    style.ScaleAllSizes(g_dpiScale);
    ImGui::GetStyle() = style;
}

void RebuildFontsAndStyle() {
    BuildFonts();
    ApplyStyle();
}

// ---------------------------------------------------------------------------------------
// Motion and drawing primitives
// ---------------------------------------------------------------------------------------

float Saturate(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float EaseOutCubic(float t) {
    const float inv = 1.0f - Saturate(t);
    return 1.0f - inv * inv * inv;
}

float EaseInOutCubic(float t) {
    t = Saturate(t);
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
}

double Now() {
    return ImGui::GetTime();
}

float Length(ImVec2 v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

// 0..1 progress of a one-shot transition that started at `startedAt` (ImGui time). Keeps
// frames coming while it runs, even when the launcher is in the background.
float TransitionProgress(double startedAt, float durationSeconds) {
    const float t = Saturate(static_cast<float>(Now() - startedAt) / durationSeconds);
    if (t < 1.0f)
        g_animating = true;
    return t;
}

// Exponentially smoothed per-widget value (hover, press). Frame-rate independent.
float Animate(ImGuiID id, float target, float speed = kHoverSpeed) {
    float& value = g_anim[id];
    const float dt = std::min(ImGui::GetIO().DeltaTime, 0.05f);
    value += (target - value) * (1.0f - std::exp(-speed * dt));
    if (std::fabs(target - value) < 0.002f)
        value = target;
    else
        g_animating = true;
    return value;
}

// Derives a second, stable animation key from a widget id (e.g. its pressed state).
ImGuiID SubId(ImGuiID id, int salt) {
    return ImHashData(&salt, sizeof(salt), id);
}

ImU32 ScaleAlpha(ImU32 color, float factor) {
    const float alpha = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF);
    return WithAlpha(color, static_cast<unsigned>(alpha * Saturate(factor) + 0.5f));
}

// Applies opacity and a vertical offset to everything drawn into `drawList` since
// `firstVertex`, so a composed block (a dialog, a step's content) can enter as one piece.
void FadeVertices(ImDrawList* drawList, int firstVertex, float alpha, float offsetY) {
    if (alpha >= 1.0f && offsetY == 0.0f)
        return;
    for (int i = firstVertex; i < drawList->VtxBuffer.Size; ++i) {
        ImDrawVert& vertex = drawList->VtxBuffer[i];
        vertex.pos.y += offsetY;
        vertex.col = ScaleAlpha(vertex.col, alpha);
    }
}

// A soft drop shadow built from a few translucent, progressively larger rounded rects.
void DrawSoftShadow(ImDrawList* drawList, ImVec2 min, ImVec2 max, float rounding, float spread, float offsetY, float strength) {
    constexpr int kLayers = 6;
    for (int i = kLayers; i >= 1; --i) {
        const float f = static_cast<float>(i) / kLayers;
        const float grow = spread * f;
        const float alpha = std::min(255.0f, strength * (1.0f - f + 1.0f / kLayers) * 255.0f / kLayers);
        drawList->AddRectFilled(min - ImVec2(grow, grow - offsetY), max + ImVec2(grow, grow + offsetY),
                                WithAlpha(kColShadow, static_cast<unsigned>(alpha)), rounding + grow);
    }
}

// Vertical fade from transparent to `color`, used to blend artwork into a surface.
void DrawFadeDown(ImDrawList* drawList, ImVec2 min, ImVec2 max, ImU32 color) {
    drawList->AddRectFilledMultiColor(min, max, WithAlpha(color, 0), WithAlpha(color, 0), color, color);
}

// Horizontal fade from `color` (left) to transparent (right).
void DrawFadeRight(ImDrawList* drawList, ImVec2 min, ImVec2 max, ImU32 color) {
    drawList->AddRectFilledMultiColor(min, max, color, WithAlpha(color, 0), WithAlpha(color, 0), color);
}

// Draws ASCII text with extra letter spacing; used for small uppercase eyebrow labels.
void DrawTrackedString(ImDrawList* drawList, ImFont* font, ImVec2 pos, ImU32 color, const char* text, float tracking) {
    float x = std::floor(pos.x);
    for (const char* c = text; *c; ++c) {
        drawList->AddText(font, font->FontSize, ImVec2(x, std::floor(pos.y)), color, c, c + 1);
        x += font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, c, c + 1).x + tracking;
    }
}

// Draws artwork scaled to cover the rectangle (like CSS object-fit: cover) with optional
// zoom. `focus` (0..1 in each axis) picks which part of the image stays in frame when it
// is cropped. Only the corners selected by `flags` are rounded.
void DrawCoverImage(ImDrawList* drawList, const GameArt& art, ImVec2 min, ImVec2 max, float zoom, float rounding,
                    ImDrawFlags flags, ImVec2 focus, ImU32 tint) {
    const ImVec2 size = max - min;
    const float boxAspect = size.x / std::max(1.0f, size.y);
    const float imageAspect = static_cast<float>(art.width) / static_cast<float>(art.height);
    float spanU = 1.0f;
    float spanV = 1.0f;
    if (imageAspect > boxAspect)
        spanU = boxAspect / imageAspect;
    else
        spanV = imageAspect / boxAspect;
    spanU /= zoom;
    spanV /= zoom;
    const float u0 = std::clamp(focus.x - spanU * 0.5f, 0.0f, 1.0f - spanU);
    const float v0 = std::clamp(focus.y - spanV * 0.5f, 0.0f, 1.0f - spanV);

    // Triangulate the rounded outline as a fan around its centre so every triangle is
    // well-formed (ImDrawList::AddImageRounded fans from a corner vertex instead).
    drawList->PathRect(min, max, rounding, flags);
    const ImVector<ImVec2> outline = drawList->_Path;
    drawList->PathClear();
    const int count = outline.Size;
    if (count < 3)
        return;

    const auto uvAt = [&](ImVec2 p) {
        return ImVec2(u0 + spanU * (p.x - min.x) / size.x, v0 + spanV * (p.y - min.y) / size.y);
    };

    drawList->PushTextureID(ToTextureId(art.texture.Get()));
    drawList->PrimReserve(count * 3, count + 1);
    const ImDrawIdx base = static_cast<ImDrawIdx>(drawList->_VtxCurrentIdx);
    const ImVec2 center = (min + max) * 0.5f;
    drawList->PrimWriteVtx(center, uvAt(center), tint);
    for (const ImVec2& point : outline)
        drawList->PrimWriteVtx(point, uvAt(point), tint);
    for (int i = 0; i < count; ++i) {
        drawList->PrimWriteIdx(base);
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1 + i));
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1 + (i + 1) % count));
    }
    drawList->PopTextureID();
}

// ---------------------------------------------------------------------------------------
// Glyphs (vector strokes, so they stay crisp at every DPI)
// ---------------------------------------------------------------------------------------

float StrokePx(float logical = 1.5f) {
    return std::max(1.0f, Px(logical));
}

// A check mark drawn along its path; `progress` animates the stroke from 0 to 1.
void DrawCheck(ImDrawList* drawList, ImVec2 center, float size, ImU32 color, float thickness, float progress) {
    const ImVec2 a = center + ImVec2(-0.48f * size, 0.02f * size);
    const ImVec2 b = center + ImVec2(-0.14f * size, 0.34f * size);
    const ImVec2 c = center + ImVec2(0.5f * size, -0.32f * size);
    const float first = Length(b - a);
    const float second = Length(c - b);
    const float distance = (first + second) * Saturate(progress);
    if (distance <= 0.0f)
        return;
    drawList->PathLineTo(a);
    if (distance <= first) {
        drawList->PathLineTo(a + (b - a) * (distance / first));
    } else {
        drawList->PathLineTo(b);
        drawList->PathLineTo(b + (c - b) * ((distance - first) / second));
    }
    drawList->PathStroke(color, 0, thickness);
}

void DrawChevronRight(ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
    const float h = size * 0.5f;
    drawList->PathLineTo(center + ImVec2(-h * 0.5f, -h));
    drawList->PathLineTo(center + ImVec2(h * 0.5f, 0.0f));
    drawList->PathLineTo(center + ImVec2(-h * 0.5f, h));
    drawList->PathStroke(color, 0, StrokePx());
}

void DrawPlayGlyph(ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
    const float h = size * 0.5f;
    drawList->AddTriangleFilled(center + ImVec2(-h * 0.7f, -h), center + ImVec2(h, 0.0f), center + ImVec2(-h * 0.7f, h), color);
}

// Two stacked steps (dot + bar): the "sequence" icon for the preparation option.
void DrawStepsGlyph(ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
    const float h = size * 0.5f;
    const float dot = std::max(1.5f, size * 0.12f);
    for (int i = 0; i < 2; ++i) {
        const float y = center.y + (i == 0 ? -h * 0.42f : h * 0.42f);
        drawList->AddCircleFilled(ImVec2(center.x - h * 0.72f, y), dot, color, 12);
        drawList->AddLine(ImVec2(center.x - h * 0.3f, y), ImVec2(center.x + h, y), color, StrokePx(1.75f));
    }
}

void DrawClockGlyph(ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
    const float r = size * 0.5f;
    drawList->AddCircle(center, r, color, 32, StrokePx(1.75f));
    drawList->PathLineTo(center + ImVec2(0.0f, -r * 0.55f));
    drawList->PathLineTo(center);
    drawList->PathLineTo(center + ImVec2(r * 0.42f, r * 0.28f));
    drawList->PathStroke(color, 0, StrokePx(1.75f));
}

void DrawAlertGlyph(ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
    const float r = size * 0.5f;
    drawList->AddCircle(center, r, color, 32, StrokePx(1.75f));
    drawList->AddLine(center + ImVec2(0.0f, -r * 0.5f), center + ImVec2(0.0f, r * 0.12f), color, StrokePx(1.75f));
    drawList->AddCircleFilled(center + ImVec2(0.0f, r * 0.48f), StrokePx(1.1f), color, 12);
}

void DrawInfoGlyph(ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
    const float r = size * 0.5f;
    drawList->AddCircle(center, r, color, 24, StrokePx(1.0f));
    drawList->AddCircleFilled(center + ImVec2(0.0f, -r * 0.45f), std::max(0.75f, Px(0.9f)), color, 8);
    drawList->AddLine(center + ImVec2(0.0f, -r * 0.08f), center + ImVec2(0.0f, r * 0.52f), color, StrokePx(1.0f));
}

// Indeterminate spinner: a breathing arc rotating over a faint track.
void DrawSpinner(ImDrawList* drawList, ImVec2 center, float radius, ImU32 color, float thickness) {
    const float time = static_cast<float>(Now());
    const float start = time * kSpinnerTurnsPerSecond * 2.0f * IM_PI;
    const float sweep = IM_PI * (0.6f + 0.3f * std::sin(time * 2.2f));
    drawList->AddCircle(center, radius, ScaleAlpha(color, 0.18f), 40, thickness);
    drawList->PathArcTo(center, radius, start, start + sweep, 28);
    drawList->PathStroke(color, 0, thickness);
    g_animating = true;
}

// ---------------------------------------------------------------------------------------
// Controls
// ---------------------------------------------------------------------------------------

enum class ButtonKind { Primary, Secondary, Muted };

bool NavCursorVisible() {
    return GImGui->NavCursorVisible;
}

void DrawFocusRing(ImDrawList* drawList, ImVec2 min, ImVec2 max, float rounding) {
    const float gap = Px(3.0f);
    drawList->AddRect(min - ImVec2(gap, gap), max + ImVec2(gap, gap), WithAlpha(kColAccentText, 210), rounding + gap, 0, StrokePx());
}

// The one button used everywhere. Unavailable ("Muted") buttons deliberately stay
// interactive (BeginDisabled() is not used) so clicking them can explain why.
bool UiButton(const char* id, const char* label, ImVec2 pos, ImVec2 size, ButtonKind kind) {
    pos = FloorVec(pos);
    ImGui::SetCursorScreenPos(pos);
    const bool clicked = ImGui::InvisibleButton(id, size, ImGuiButtonFlags_EnableNav);
    const ImGuiID itemId = ImGui::GetItemID();
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive() && hovered;
    const bool focused = ImGui::IsItemFocused();
    const float hover = Animate(itemId, hovered ? 1.0f : 0.0f);
    const float press = Animate(SubId(itemId, 1), held ? 1.0f : 0.0f, kPressSpeed);
    if (hovered)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float inset = press * Px(1.0f);
    const ImVec2 min = pos + ImVec2(inset, inset);
    const ImVec2 max = pos + size - ImVec2(inset, inset);
    const float rounding = Px(kButtonRounding);

    ImU32 fill = 0;
    ImU32 border = 0;
    ImU32 text = kColText;
    switch (kind) {
    case ButtonKind::Primary:
        fill = LerpColor(LerpColor(kColAccent, kColAccentHover, hover), kColAccentActive, press);
        border = IM_COL32(255, 255, 255, 30);
        text = kColOnAccent;
        break;
    case ButtonKind::Secondary:
        fill = LerpColor(IM_COL32(255, 255, 255, 9), IM_COL32(255, 255, 255, 18), hover);
        border = LerpColor(kColHairlineStrong, IM_COL32(255, 255, 255, 46), hover);
        text = LerpColor(kColTextSoft, kColText, hover);
        break;
    case ButtonKind::Muted:
        fill = LerpColor(IM_COL32(255, 255, 255, 5), IM_COL32(255, 255, 255, 10), hover);
        border = kColHairline;
        text = LerpColor(kColTextDim, kColTextMuted, hover);
        break;
    }
    drawList->AddRectFilled(min, max, fill, rounding);
    drawList->AddRect(min, max, border, rounding);

    const ImVec2 textSize = MeasureText(g_fonts.label, label);
    DrawString(drawList, g_fonts.label, min + (max - min - textSize) * 0.5f, text, label);
    if (focused && NavCursorVisible())
        DrawFocusRing(drawList, pos, pos + size, rounding);
    return clicked;
}

enum class TitleButtonKind { Minimize, Close };

bool TitleBarButton(ImDrawList* drawList, const char* id, TitleButtonKind kind, ImVec2 pos, ImVec2 size) {
    ImGui::SetCursorScreenPos(pos);
    const bool clicked = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const bool isClose = kind == TitleButtonKind::Close;
    const float hover = Animate(ImGui::GetItemID(), hovered || held ? 1.0f : 0.0f, kPressSpeed);

    // Hover fills are inset pills rather than full-height slabs.
    const float inset = Px(kTitleButtonInset);
    const ImVec2 min = pos + ImVec2(inset * 0.5f, inset);
    const ImVec2 max = pos + size - ImVec2(inset * 0.5f, inset);
    if (hover > 0.0f) {
        const ImU32 fill = isClose ? (held ? kColCloseActive : kColCloseHover) : IM_COL32(255, 255, 255, held ? 26 : 16);
        drawList->AddRectFilled(min, max, ScaleAlpha(fill, hover), Px(7.0f));
    }

    // The minus (−) and multiplication (×) labels are drawn as crisp, pixel-aligned strokes.
    const ImU32 color = LerpColor(kColTextMuted, isClose ? IM_COL32_WHITE : kColText, hover);
    const float half = std::floor(Px(kTitleGlyphSize) * 0.5f);
    const ImVec2 center = FloorVec(pos + size * 0.5f) + ImVec2(0.5f, 0.5f);
    const float thickness = std::max(1.0f, Px(1.0f));
    if (isClose) {
        drawList->AddLine(center + ImVec2(-half, -half), center + ImVec2(half, half), color, thickness);
        drawList->AddLine(center + ImVec2(-half, half), center + ImVec2(half, -half), color, thickness);
    } else {
        drawList->AddLine(center + ImVec2(-half, 0.0f), center + ImVec2(half, 0.0f), color, thickness);
    }
    return clicked;
}

// A pill with a coloured status dot. `glass` draws it on a dark translucent backing so it
// reads over artwork.
ImVec2 DrawPill(ImDrawList* drawList, ImVec2 pos, ImFont* font, const char* label, ImU32 dotColor, ImU32 textColor, bool glass) {
    const float padX = Px(9.0f);
    const float padY = Px(4.0f);
    const float dot = Px(6.0f);
    const float gap = Px(6.0f);
    const ImVec2 textSize = MeasureText(font, label);
    const ImVec2 size(std::floor(padX + dot + gap + textSize.x + padX), std::floor(textSize.y + 2.0f * padY));

    pos = FloorVec(pos);
    drawList->AddRectFilled(pos, pos + size, glass ? kColGlass : IM_COL32(255, 255, 255, 7), size.y * 0.5f);
    drawList->AddRect(pos, pos + size, glass ? IM_COL32(255, 255, 255, 24) : kColHairline, size.y * 0.5f);
    const ImVec2 dotCenter(pos.x + padX + dot * 0.5f, pos.y + size.y * 0.5f);
    drawList->AddCircleFilled(dotCenter, dot * 0.5f + Px(2.0f), ScaleAlpha(dotColor, 0.22f), 16);
    drawList->AddCircleFilled(dotCenter, dot * 0.5f, dotColor, 16);
    DrawString(drawList, font, ImVec2(pos.x + padX + dot + gap, pos.y + padY), textColor, label);
    return size;
}

// ---------------------------------------------------------------------------------------
// Library screen
// ---------------------------------------------------------------------------------------

// A gentle top-lit gradient behind the whole page.
void DrawBackdrop(ImDrawList* drawList, ImVec2 origin, ImVec2 size) {
    drawList->AddRectFilled(origin, origin + size, kColBackground);
    drawList->AddRectFilledMultiColor(origin, origin + ImVec2(size.x, Px(260.0f)), kColBackgroundTop, kColBackgroundTop,
                                      kColBackground, kColBackground);
}

void DrawTitleBar(ImDrawList* drawList, ImVec2 origin, float width) {
    // Window controls. Their area is reported as HTCLIENT by WM_NCHITTEST, so clicks
    // here reach ImGui and never start a native drag. Everything to their left drags.
    const ImVec2 buttonSize(Px(kTitleButtonWidth), TitleBarHeightPx());
    const float buttonsLeft = origin.x + TitleButtonsLeftPx(width);
    if (TitleBarButton(drawList, "##minimize", TitleButtonKind::Minimize, ImVec2(buttonsLeft, origin.y), buttonSize))
        g_pendingAction = PendingAction::Minimize;
    if (TitleBarButton(drawList, "##close", TitleButtonKind::Close, ImVec2(buttonsLeft + buttonSize.x, origin.y), buttonSize))
        g_pendingAction = PendingAction::Close;
}

float DrawHeader(ImDrawList* drawList, ImVec2 origin, float width) {
    const float left = origin.x + Px(kContentPadding);
    const float right = origin.x + width - Px(kContentPadding);
    const float top = origin.y + TitleBarHeightPx() + Px(kHeaderTopPadding);

    DrawString(drawList, g_fonts.heading, ImVec2(left, top), kColText, "Your games");
    const float captionY = top + g_fonts.heading->FontSize + Px(3.0f);
    DrawString(drawList, g_fonts.caption, ImVec2(left, captionY), kColTextMuted, "Choose a title to get started.");

    int readyCount = 0;
    for (const GameEntry& game : kGames)
        readyCount += game.status == GameStatus::Ready ? 1 : 0;
    char summary[64];
    std::snprintf(summary, sizeof(summary), "%d games \xC2\xB7 %d ready", kGameCount, readyCount);

    // Right-aligned summary pill, centred on the heading/caption block.
    const float pillHeight = g_fonts.caption->FontSize + 2.0f * Px(4.0f);
    const float pillWidth = Px(9.0f) * 2.0f + Px(6.0f) + Px(6.0f) + MeasureText(g_fonts.caption, summary).x;
    const float blockCenter = (top + captionY + g_fonts.caption->FontSize) * 0.5f;
    DrawPill(drawList, ImVec2(right - std::floor(pillWidth), blockCenter - pillHeight * 0.5f), g_fonts.caption, summary,
             kColReady, kColTextSoft, false);

    return captionY + g_fonts.caption->FontSize + Px(kHeaderToGrid);
}

void DrawStatusChip(ImDrawList* drawList, ImVec2 pos, GameStatus status) {
    const char* label = status == GameStatus::Ready ? "Ready" : "Updating";
    const ImU32 color = status == GameStatus::Ready ? kColReady : kColUpdating;

    const float padX = Px(9.0f);
    const float padY = Px(4.0f);
    const float dot = Px(6.0f);
    const float gap = Px(6.0f);
    const ImVec2 textSize = MeasureText(g_fonts.chip, label);
    const ImVec2 size(padX + dot + gap + textSize.x + padX, textSize.y + 2.0f * padY);

    pos = FloorVec(pos);
    drawList->AddRectFilled(pos, pos + size, WithAlpha(color, 28), size.y * 0.5f);
    drawList->AddCircleFilled(ImVec2(pos.x + padX + dot * 0.5f, pos.y + size.y * 0.5f), dot * 0.5f, color, 16);
    DrawString(drawList, g_fonts.chip, ImVec2(pos.x + padX + dot + gap, pos.y + padY), color, label);
}

// Draws a Load button. Unavailable buttons are rendered with muted, disabled styling but
// deliberately remain interactive (BeginDisabled() is not used) so that clicking them can
// explain why the game cannot be loaded. The caller never launches anything for them.
bool LoadButton(ImDrawList* drawList, const char* id, ImVec2 pos, ImVec2 size, bool available) {
    pos = FloorVec(pos);
    ImGui::SetCursorScreenPos(pos);
    const bool clicked = ImGui::InvisibleButton(id, size, ImGuiButtonFlags_EnableNav);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const float rounding = Px(kCardButtonRounding);

    ImU32 textColor;
    if (available) {
        drawList->AddRectFilled(pos, pos + size, held ? kColAccentActive : hovered ? kColAccentHover : kColAccent, rounding);
        textColor = kColOnAccent;
        if (hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    } else {
        drawList->AddRectFilled(pos, pos + size, kColDisabledFill, rounding);
        drawList->AddRect(pos, pos + size, kColDisabledBorder, rounding);
        textColor = kColDisabledText;
        if (hovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    const ImVec2 textSize = MeasureText(g_fonts.cardButton, "Load");
    DrawString(drawList, g_fonts.cardButton, pos + (size - textSize) * 0.5f, textColor, "Load");
    if (ImGui::IsItemFocused() && NavCursorVisible())
        drawList->AddRect(pos - ImVec2(Px(2), Px(2)), pos + size + ImVec2(Px(2), Px(2)), kColAccent, rounding);
    return clicked;
}

float BannerHeightPx(float cardWidth) {
    return std::floor(cardWidth / kBannerAspect);
}

float StatusChipHeightPx() {
    return g_fonts.chip->FontSize + 2.0f * Px(4.0f);
}

void DrawGameCard(ImDrawList* drawList, int index, ImVec2 cardMin, ImVec2 cardSize, float nameAreaHeight) {
    const GameEntry& game = kGames[index];
    const GameArt& art = g_art[index];
    const ImVec2 cardMax = cardMin + cardSize;
    const float pad = Px(kCardPadding);
    const float innerWidth = cardSize.x - 2.0f * pad;
    const float rounding = Px(kCardRounding);
    const bool available = game.action == LoadAction::LaunchJampusStrike;

    // Hover is suppressed automatically while a modal blocks the launcher window.
    const bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(cardMin, cardMax);
    float& hover = g_cardHover[index];
    const float target = hovered ? 1.0f : 0.0f;
    hover += (target - hover) * std::min(1.0f, ImGui::GetIO().DeltaTime * kHoverAnimationSpeed);
    if (std::fabs(target - hover) < 0.002f)
        hover = target;
    else
        g_animating = true;

    // Card surface with the game artwork as a full-width 16:9 banner across the top.
    drawList->AddRectFilled(cardMin + ImVec2(0, Px(5)), cardMax + ImVec2(0, Px(5)), IM_COL32(0, 0, 0, 65), rounding);
    drawList->AddRectFilled(cardMin, cardMax, kColSurface, rounding);
    const ImVec2 bannerMax(cardMax.x, cardMin.y + BannerHeightPx(cardSize.x));
    if (art.texture)
        DrawCoverImage(drawList, art, cardMin, bannerMax, 1.0f + kArtHoverZoom * hover, rounding, ImDrawFlags_RoundCornersTop,
                       ImVec2(0.5f, 0.5f), IM_COL32_WHITE);
    drawList->AddLine(ImVec2(cardMin.x, bannerMax.y), ImVec2(cardMax.x, bannerMax.y), kColCardBorder);
    drawList->AddRect(cardMin, cardMax, LerpColor(kColCardBorder, WithAlpha(kColAccent, 145), hover), rounding, 0, 1.0f);
    if (available)
        drawList->AddLine(ImVec2(cardMin.x + rounding, cardMax.y), ImVec2(cardMax.x - rounding, cardMax.y), WithAlpha(kColAccent, 85), Px(2));

    // Name (wrapping), status and a bottom-anchored Load button. The name area is sized for
    // the tallest name in the grid so status chips line up across cards.
    float y = bannerMax.y + pad;
    DrawString(drawList, g_fonts.cardTitle, ImVec2(cardMin.x + pad, y), kColText, game.name, innerWidth);
    y += nameAreaHeight + Px(kNameToStatusGap);
    DrawStatusChip(drawList, ImVec2(cardMin.x + pad, y), game.status);

    const float buttonHeight = Px(kCardButtonHeight);
    const ImVec2 buttonPos(cardMin.x + pad, cardMax.y - pad - buttonHeight);
    ImGui::PushID(index);
    const bool clicked = LoadButton(drawList, "##load", buttonPos, ImVec2(innerWidth, buttonHeight), available);
    ImGui::PopID();

    if (!clicked || g_launchSucceeded || g_pendingAction != PendingAction::None)
        return;
    if (game.action == LoadAction::LaunchJampusStrike)
        ShowModal(ModalKind::Prelaunch, {}, index);
    else
        ShowModal(ModalKind::Updating, kUpdatingMessage, index);
}

void DrawGameGrid(ImVec2 origin, float width, float top) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float left = origin.x + Px(kContentPadding);
    const float availableWidth = width - 2.0f * Px(kContentPadding);
    const float gap = Px(kCardGap);
    const float pad = Px(kCardPadding);

    const int columns = std::clamp(static_cast<int>((availableWidth + gap) / (Px(kCardMinWidth) + gap)), 1, kGameCount);
    const float cardWidth = std::floor((availableWidth - gap * (columns - 1)) / columns);
    const float innerWidth = cardWidth - 2.0f * pad;

    float nameAreaHeight = 0.0f;
    for (const GameEntry& game : kGames)
        nameAreaHeight = std::max(nameAreaHeight, MeasureText(g_fonts.cardTitle, game.name, innerWidth).y);
    const float cardHeight = std::ceil(BannerHeightPx(cardWidth) + pad + nameAreaHeight + Px(kNameToStatusGap) +
                                       StatusChipHeightPx() + Px(16.0f) + Px(kCardButtonHeight) + pad);

    for (int i = 0; i < kGameCount; ++i) {
        const int row = i / columns;
        const int column = i % columns;
        const ImVec2 cardMin(left + column * (cardWidth + gap), top + row * (cardHeight + gap));
        DrawGameCard(drawList, i, cardMin, ImVec2(cardWidth, cardHeight), nameAreaHeight);
    }
}

void DrawFooter(ImDrawList* drawList, ImVec2 origin, ImVec2 size) {
    const float top = origin.y + size.y - Px(kFooterHeight);
    const float left = origin.x + Px(kContentPadding);
    const float right = origin.x + size.x - Px(kContentPadding);
    drawList->AddLine(ImVec2(left, top), ImVec2(right, top), kColHairline);

    const float textY = std::floor(top + (Px(kFooterHeight) - g_fonts.caption->FontSize) * 0.5f);
    const float glyph = Px(12.0f);
    DrawInfoGlyph(drawList, ImVec2(left + glyph * 0.5f, textY + g_fonts.caption->FontSize * 0.5f + Px(0.5f)), glyph, kColTextDim);
    DrawString(drawList, g_fonts.caption, ImVec2(left + glyph + Px(8.0f), textY), kColTextDim,
               "JampusStrike.exe must be in the launcher folder.");
    const ImVec2 versionSize = MeasureText(g_fonts.caption, kVersionText);
    DrawString(drawList, g_fonts.caption, ImVec2(right - versionSize.x, textY), kColTextDim, kVersionText);
}

// ---------------------------------------------------------------------------------------
// Dialogs
// ---------------------------------------------------------------------------------------

void CloseModal() {
    ImGui::CloseCurrentPopup();
    g_modal = ModalState{};
}

void QueueLaunchFromModal() {
    CloseModal();
    g_pendingAction = PendingAction::LaunchJampusStrike;
}

bool IsPrelaunchKind(ModalKind kind) {
    return kind == ModalKind::Prelaunch || kind == ModalKind::Simulating || kind == ModalKind::SimulationComplete;
}

enum class NoticeGlyph { Info, Clock, Alert };

struct NoticeContent {
    const char* title;
    ImU32 tone;
    NoticeGlyph glyph;
};

NoticeContent GetNoticeContent() {
    switch (g_modal.kind) {
    case ModalKind::Updating: return { "Update in progress", kColUpdating, NoticeGlyph::Clock };
    case ModalKind::ExecutableMissing: return { "JampusStrike.exe not found", kColError, NoticeGlyph::Alert };
    case ModalKind::LaunchFailed: return { "Launch failed", kColError, NoticeGlyph::Alert };
    default: return { "Notice", kColTextMuted, NoticeGlyph::Info };
    }
}

float NoticeTextWidth(float width) {
    return width - 2.0f * Px(kModalPadding) - Px(kNoticeIconSize) - Px(16.0f);
}

float NoticeHeight(float width) {
    const float messageHeight = MeasureText(g_fonts.body, g_modal.message.c_str(), NoticeTextWidth(width)).y;
    const float textHeight = g_fonts.noticeTitle->FontSize + Px(6.0f) + messageHeight;
    return std::ceil(Px(kModalPadding) * 2.0f + std::max(Px(kNoticeIconSize), textHeight) + Px(24.0f) + Px(kButtonHeight));
}

void DrawNotice(ImDrawList* drawList, ImVec2 min, ImVec2 max) {
    const NoticeContent content = GetNoticeContent();
    const float pad = Px(kModalPadding);
    const float icon = Px(kNoticeIconSize);

    // Status tile with a tinted glyph.
    const ImVec2 iconMin = min + ImVec2(pad, pad);
    drawList->AddRectFilled(iconMin, iconMin + ImVec2(icon, icon), ScaleAlpha(content.tone, 0.13f), Px(11.0f));
    drawList->AddRect(iconMin, iconMin + ImVec2(icon, icon), ScaleAlpha(content.tone, 0.28f), Px(11.0f));
    const ImVec2 iconCenter = iconMin + ImVec2(icon, icon) * 0.5f;
    const float glyph = Px(17.0f);
    switch (content.glyph) {
    case NoticeGlyph::Clock: DrawClockGlyph(drawList, iconCenter, glyph, content.tone); break;
    case NoticeGlyph::Alert: DrawAlertGlyph(drawList, iconCenter, glyph, content.tone); break;
    case NoticeGlyph::Info: DrawInfoGlyph(drawList, iconCenter, glyph, content.tone); break;
    }

    const float textX = iconMin.x + icon + Px(16.0f);
    const float titleY = iconMin.y + Px(1.0f);
    DrawString(drawList, g_fonts.noticeTitle, ImVec2(textX, titleY), kColText, content.title);
    DrawString(drawList, g_fonts.body, ImVec2(textX, titleY + g_fonts.noticeTitle->FontSize + Px(6.0f)), kColTextSoft,
               g_modal.message.c_str(), NoticeTextWidth(max.x - min.x));

    const ImVec2 buttonSize(Px(112.0f), Px(kButtonHeight));
    const bool dismissed = UiButton("##dismiss", "Dismiss", max - ImVec2(pad, pad) - buttonSize, buttonSize, ButtonKind::Secondary);
    const bool keyDismiss = ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                            ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
    if (dismissed || keyDismiss)
        CloseModal();
}

// These screens are presentation only. A monotonic local timer drives every stage;
// nothing reads hardware identifiers, touches services, or interacts with other apps.
constexpr float kSimulationDurationMs = 9000.0f;
constexpr int kStageCount = 2;
constexpr const char* kStageNames[kStageCount] = { "HWID Spoofer", "AC Blocker" };

float SimulationProgress(ULONGLONG now, ULONGLONG started) {
    return std::clamp(static_cast<float>(now >= started ? now - started : 0) / kSimulationDurationMs, 0.0f, 1.0f);
}

// Eased 0..1 progress of one stage, given the overall linear progress.
float StageProgress(float overall, int stage) {
    return EaseInOutCubic(overall * kStageCount - static_cast<float>(stage));
}

enum class OptionGlyph { Steps, Play };

// A large selectable row used for the two ways of starting a session.
bool LaunchOption(const char* id, const char* title, const char* description, OptionGlyph glyph, ImVec2 pos, float width, bool primary) {
    pos = FloorVec(pos);
    const ImVec2 size(width, Px(kOptionHeight));
    ImGui::SetCursorScreenPos(pos);
    const bool clicked = ImGui::InvisibleButton(id, size, ImGuiButtonFlags_EnableNav);
    const ImGuiID itemId = ImGui::GetItemID();
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive() && hovered;
    const float hover = Animate(itemId, hovered ? 1.0f : 0.0f);
    const float press = Animate(SubId(itemId, 1), held ? 1.0f : 0.0f, kPressSpeed);
    if (hovered)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float rounding = Px(12.0f);
    const float inset = press * Px(1.0f);
    const ImVec2 min = pos + ImVec2(inset, inset);
    const ImVec2 max = pos + size - ImVec2(inset, inset);
    const ImU32 fill = primary ? LerpColor(WithAlpha(kColAccent, 30), WithAlpha(kColAccent, 50), hover)
                               : LerpColor(IM_COL32(255, 255, 255, 6), IM_COL32(255, 255, 255, 12), hover);
    const ImU32 border = primary ? LerpColor(WithAlpha(kColAccentText, 70), WithAlpha(kColAccentText, 150), hover)
                                 : LerpColor(kColHairline, IM_COL32(255, 255, 255, 40), hover);
    drawList->AddRectFilled(min, max, fill, rounding);
    drawList->AddRect(min, max, border, rounding);

    const float tile = Px(38.0f);
    const ImVec2 tileMin(pos.x + Px(14.0f), std::floor(pos.y + (size.y - tile) * 0.5f));
    drawList->AddRectFilled(tileMin, tileMin + ImVec2(tile, tile), primary ? kColAccent : IM_COL32(255, 255, 255, 14), Px(10.0f));
    const ImVec2 tileCenter = tileMin + ImVec2(tile, tile) * 0.5f;
    const ImU32 glyphColor = primary ? kColOnAccent : kColTextSoft;
    if (glyph == OptionGlyph::Steps)
        DrawStepsGlyph(drawList, tileCenter, Px(16.0f), glyphColor);
    else
        DrawPlayGlyph(drawList, tileCenter + ImVec2(Px(1.0f), 0.0f), Px(13.0f), glyphColor);

    const float textX = tileMin.x + tile + Px(14.0f);
    const float textTop = pos.y + std::floor((size.y - (g_fonts.label->FontSize + Px(3.0f) + g_fonts.caption->FontSize)) * 0.5f) - Px(1.0f);
    DrawString(drawList, g_fonts.label, ImVec2(textX, textTop), kColText, title);
    DrawString(drawList, g_fonts.caption, ImVec2(textX, textTop + g_fonts.label->FontSize + Px(3.0f)), kColTextMuted, description);

    DrawChevronRight(drawList, ImVec2(max.x - Px(22.0f) + EaseOutCubic(hover) * Px(3.0f), pos.y + size.y * 0.5f), Px(10.0f),
                     LerpColor(kColTextMuted, kColText, hover));
    if (ImGui::IsItemFocused() && NavCursorVisible())
        DrawFocusRing(drawList, pos, pos + size, rounding);
    return clicked;
}

void DrawStageRow(ImDrawList* drawList, ImVec2 pos, float width, int index, float progress, float checkProgress) {
    const bool complete = progress >= 1.0f;
    const bool active = progress > 0.0f && !complete;
    const ImU32 tone = complete ? kColReady : active ? kColAccentText : kColTextDim;
    const float rowHeight = Px(kStageRowHeight);

    // Indicator: numbered ring → spinner → drawn check.
    const ImVec2 center(pos.x + Px(30.0f), pos.y + rowHeight * 0.5f);
    const float radius = Px(12.0f);
    const char number[2] = { static_cast<char>('1' + index), '\0' };
    if (complete) {
        drawList->AddCircleFilled(center, radius, ScaleAlpha(kColReady, 0.16f), 32);
        DrawCheck(drawList, center, Px(10.0f), kColReady, StrokePx(1.75f), checkProgress);
    } else {
        if (active)
            DrawSpinner(drawList, center, radius, kColAccentText, StrokePx(1.75f));
        else
            drawList->AddCircle(center, radius, kColHairlineStrong, 32, StrokePx(1.25f));
        DrawString(drawList, g_fonts.chip, center - MeasureText(g_fonts.chip, number) * 0.5f, active ? kColText : kColTextDim, number);
    }

    const float textX = pos.x + Px(56.0f);
    const float textTop = pos.y + std::floor((rowHeight - (g_fonts.label->FontSize + Px(2.0f) + g_fonts.caption->FontSize)) * 0.5f);
    DrawString(drawList, g_fonts.label, ImVec2(textX, textTop), complete || active ? kColText : kColTextSoft, kStageNames[index]);
    DrawString(drawList, g_fonts.caption, ImVec2(textX, textTop + g_fonts.label->FontSize + Px(2.0f)), tone,
               complete ? "Complete" : active ? "In progress\xE2\x80\xA6" : "Queued");

    char percent[16];
    std::snprintf(percent, sizeof(percent), "%d%%", static_cast<int>(progress * 100.0f));
    const ImVec2 percentSize = MeasureText(g_fonts.label, percent);
    DrawString(drawList, g_fonts.label, ImVec2(pos.x + width - Px(18.0f) - percentSize.x, center.y - percentSize.y * 0.5f),
               complete ? kColReady : active ? kColText : kColTextDim, percent);
}

// A segmented bar: one segment per stage, so progress maps visibly onto the list above.
void DrawSegmentedProgress(ImDrawList* drawList, ImVec2 pos, float width, const float* stageProgress, float completion) {
    const float height = Px(6.0f);
    const float gap = Px(6.0f);
    const float segment = std::floor((width - gap * (kStageCount - 1)) / kStageCount);
    const ImU32 fill = LerpColor(kColAccent, kColReady, completion);
    for (int i = 0; i < kStageCount; ++i) {
        const ImVec2 min(pos.x + i * (segment + gap), pos.y);
        drawList->AddRectFilled(min, min + ImVec2(segment, height), IM_COL32(255, 255, 255, 14), height * 0.5f);
        if (stageProgress[i] > 0.0f)
            drawList->AddRectFilled(min, min + ImVec2(std::max(height, segment * stageProgress[i]), height), fill, height * 0.5f);
    }
}

void DrawPrelaunchFlow(ImDrawList* drawList, ImVec2 min, ImVec2 max) {
    const int gameIndex = std::clamp(g_modal.gameIndex, 0, kGameCount - 1);
    const GameEntry& game = kGames[gameIndex];
    const GameArt& art = g_art[gameIndex];
    const float width = max.x - min.x;
    const float pad = Px(kModalPadding);
    const float left = min.x + pad;
    const float contentWidth = width - 2.0f * pad;

    // Advance the cosmetic timeline.
    const bool choosing = g_modal.kind == ModalKind::Prelaunch;
    float progress = 0.0f;
    float elapsedMs = 0.0f;
    if (!choosing) {
        const ULONGLONG now = GetTickCount64();
        elapsedMs = static_cast<float>(now >= g_modal.simulationStarted ? now - g_modal.simulationStarted : 0);
        progress = SimulationProgress(now, g_modal.simulationStarted);
        if (progress >= 1.0f && g_modal.kind == ModalKind::Simulating) {
            g_modal.kind = ModalKind::SimulationComplete;
            g_modal.completedAt = Now();
        } else if (progress < 1.0f) {
            g_animating = true;
        }
    }
    const bool complete = g_modal.kind == ModalKind::SimulationComplete;
    const float completion = complete ? EaseOutCubic(TransitionProgress(g_modal.completedAt, kContentFadeSeconds)) : 0.0f;

    // Hero: the game's artwork bleeds in from the right and fades into the surface.
    const float heroHeight = Px(kPrelaunchHeroHeight);
    if (art.texture) {
        const ImVec2 artMin(std::floor(min.x + width * 0.4f), min.y);
        const ImVec2 artMax(max.x, min.y + heroHeight);
        DrawCoverImage(drawList, art, artMin, artMax, 1.0f, Px(kModalRounding), ImDrawFlags_RoundCornersTopRight,
                       ImVec2(game.artFocusX, game.artFocusY), IM_COL32(210, 210, 218, 255));
        DrawFadeRight(drawList, artMin, ImVec2(artMin.x + (artMax.x - artMin.x) * 0.75f, artMax.y), kColSurface);
        DrawFadeDown(drawList, ImVec2(artMin.x, artMax.y - heroHeight * 0.6f), artMax, kColSurface);
    }

    const int heroTextVertex = drawList->VtxBuffer.Size;
    char eyebrow[64] = {};
    for (size_t i = 0; game.name[i] && i + 1 < sizeof(eyebrow); ++i)
        eyebrow[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(game.name[i])));
    DrawTrackedString(drawList, g_fonts.eyebrow, ImVec2(left, min.y + Px(26.0f)), kColAccentText, eyebrow, Px(1.2f));
    DrawString(drawList, g_fonts.modalTitle, ImVec2(left, min.y + Px(43.0f)), kColText,
               choosing ? "Launch options" : complete ? "Ready to launch" : "Preparing your session");
    char subtitle[128];
    if (complete)
        std::snprintf(subtitle, sizeof(subtitle), "Continue to %s whenever you're ready.", game.name);
    else
        std::snprintf(subtitle, sizeof(subtitle), "%s", choosing ? "Choose how you'd like to start your session." : "Keep this window open while preparation finishes.");
    DrawString(drawList, g_fonts.body, ImVec2(left, min.y + Px(75.0f)), kColTextSoft, subtitle, contentWidth);
    if (complete)
        FadeVertices(drawList, heroTextVertex, completion, 0.0f);

    // Body. Content that changes with the step is faded in as a unit.
    const int bodyVertex = drawList->VtxBuffer.Size;
    const float bodyTop = min.y + heroHeight + Px(6.0f);
    const ImVec2 buttonSize(Px(104.0f), Px(kButtonHeight));
    const float footerY = max.y - pad - buttonSize.y;
    const float footerTextY = std::floor(footerY + (buttonSize.y - g_fonts.caption->FontSize) * 0.5f);
    bool run = false;
    bool launch = false;
    bool cancel = false;

    if (choosing) {
        run = LaunchOption("##prepare", "Prepare Session", "HWID Spoofer and AC Blocker sequence.", OptionGlyph::Steps,
                           ImVec2(left, bodyTop), contentWidth, true);
        launch = LaunchOption("##direct", "Launch Directly", "Open JampusStrike without preparation.", OptionGlyph::Play,
                              ImVec2(left, bodyTop + Px(kOptionHeight + kOptionGap)), contentWidth, false);
        DrawString(drawList, g_fonts.caption, ImVec2(left, footerTextY), kColTextMuted, "Preparation is optional. You can cancel at any time.");
        cancel = UiButton("##cancel", "Cancel", ImVec2(max.x - pad - buttonSize.x, footerY), buttonSize, ButtonKind::Secondary);
    } else {
        float stageProgress[kStageCount];
        for (int i = 0; i < kStageCount; ++i)
            stageProgress[i] = StageProgress(progress, i);

        // Stage list.
        const float rowHeight = Px(kStageRowHeight);
        const ImVec2 panelMin(left, bodyTop);
        const ImVec2 panelMax(left + contentWidth, bodyTop + rowHeight * kStageCount);
        drawList->AddRectFilled(panelMin, panelMax, kColSurfaceSunken, Px(12.0f));
        drawList->AddRect(panelMin, panelMax, kColHairline, Px(12.0f));
        for (int i = 0; i < kStageCount; ++i) {
            const ImVec2 rowPos(left, bodyTop + rowHeight * i);
            if (i > 0)
                drawList->AddLine(rowPos + ImVec2(Px(16.0f), 0.0f), rowPos + ImVec2(contentWidth - Px(16.0f), 0.0f), kColHairline);
            const float completedAtMs = kSimulationDurationMs * static_cast<float>(i + 1) / kStageCount;
            const float checkProgress = Saturate((elapsedMs - completedAtMs) / (kCheckDrawSeconds * 1000.0f));
            if (stageProgress[i] >= 1.0f && checkProgress < 1.0f)
                g_animating = true;
            DrawStageRow(drawList, rowPos, contentWidth, i, stageProgress[i], EaseOutCubic(checkProgress));
        }

        // Overall progress.
        const float labelY = panelMax.y + Px(16.0f);
        float overall = 0.0f;
        for (float value : stageProgress)
            overall += value / kStageCount;
        const int currentStage = std::min(kStageCount, static_cast<int>(progress * kStageCount) + 1);
        char stageText[48];
        if (complete)
            std::snprintf(stageText, sizeof(stageText), "Preparation complete");
        else
            std::snprintf(stageText, sizeof(stageText), "Stage %d of %d", currentStage, kStageCount);
        char overallText[24];
        std::snprintf(overallText, sizeof(overallText), "%d%%", static_cast<int>(overall * 100.0f));
        DrawString(drawList, g_fonts.caption, ImVec2(left, labelY), LerpColor(kColTextSoft, kColReady, completion), stageText);
        DrawString(drawList, g_fonts.caption, ImVec2(left + contentWidth - MeasureText(g_fonts.caption, overallText).x, labelY),
                   kColTextMuted, overallText);
        DrawSegmentedProgress(drawList, ImVec2(left, labelY + g_fonts.caption->FontSize + Px(7.0f)), contentWidth, stageProgress, completion);

        // Footer.
        const int footerVertex = drawList->VtxBuffer.Size;
        if (complete) {
            const float backWidth = Px(140.0f);
            cancel = UiButton("##back", "Back to Library", ImVec2(left, footerY), ImVec2(backWidth, buttonSize.y), ButtonKind::Secondary);
            launch = UiButton("##launch", "Launch Game", ImVec2(left + backWidth + Px(10.0f), footerY),
                              ImVec2(contentWidth - backWidth - Px(10.0f), buttonSize.y), ButtonKind::Primary);
            FadeVertices(drawList, footerVertex, completion, std::floor((1.0f - completion) * Px(4.0f)));
        } else {
            char timing[64];
            std::snprintf(timing, sizeof(timing), "About %d seconds remaining",
                          static_cast<int>(std::ceil((1.0f - progress) * kSimulationDurationMs / 1000.0f)));
            DrawString(drawList, g_fonts.caption, ImVec2(left, footerTextY), kColTextMuted, timing);
            cancel = UiButton("##cancel", "Cancel", ImVec2(max.x - pad - buttonSize.x, footerY), buttonSize, ButtonKind::Secondary);
        }
    }

    const float enter = EaseOutCubic(TransitionProgress(g_modal.stageChangedAt, kContentFadeSeconds));
    FadeVertices(drawList, bodyVertex, enter, std::floor((1.0f - enter) * Px(6.0f)));

    if (cancel || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        CloseModal();
    } else if (launch) {
        QueueLaunchFromModal();
    } else if (run) {
        g_modal.kind = ModalKind::Simulating;
        g_modal.simulationStarted = GetTickCount64();
        g_modal.stageChangedAt = Now();
        g_animating = true;
    }
}

void DrawModal() {
    if (g_modal.openRequested) {
        ImGui::OpenPopup(kModalPopupId);
        g_modal.openRequested = false;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const bool prelaunch = IsPrelaunchKind(g_modal.kind);
    const float margin = Px(kContentPadding);
    const float width = std::min(Px(prelaunch ? kPrelaunchWidth : kNoticeWidth), viewport->Size.x - 2.0f * margin);
    const float height = std::min(prelaunch ? Px(kPrelaunchHeight) : NoticeHeight(width), viewport->Size.y - 2.0f * margin);

    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Px(kModalRounding));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground;
    const bool open = ImGui::BeginPopupModal(kModalPopupId, nullptr, flags);
    ImGui::PopStyleVar(3);

    if (!open) {
        if (g_modal.kind != ModalKind::None && !g_modal.openRequested)
            g_modal = ModalState{};
        return;
    }

    // The dialog draws its own surface so its shadow, border and enter transition can
    // be composed as one piece. The clip rect is widened to let the shadow and the
    // entering offset extend past the window bounds.
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 max = min + ImGui::GetWindowSize();
    const float rounding = Px(kModalRounding);
    const ImVec2 bleed(Px(40.0f), Px(40.0f));
    drawList->PushClipRect(min - bleed, max + bleed, false);
    const int firstVertex = drawList->VtxBuffer.Size;

    DrawSoftShadow(drawList, min, max, rounding, Px(26.0f), Px(10.0f), 1.1f);
    drawList->AddRectFilled(min, max, kColSurface, rounding);
    if (prelaunch)
        DrawPrelaunchFlow(drawList, min, max);
    else
        DrawNotice(drawList, min, max);
    drawList->AddRect(min, max, kColHairlineStrong, rounding);

    const float enter = EaseOutCubic(TransitionProgress(g_modal.openedAt, kModalEnterSeconds));
    FadeVertices(drawList, firstVertex, enter, std::floor((1.0f - enter) * Px(10.0f)));
    drawList->PopClipRect();
    ImGui::EndPopup();
}

void DrawUi() {
    g_animating = false;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("##launcher", nullptr, flags);
    ImGui::PopStyleVar(3);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = viewport->Pos;
    const ImVec2 size = viewport->Size;

    DrawBackdrop(drawList, origin, size);
    DrawTitleBar(drawList, origin, size.x);
    const float gridTop = DrawHeader(drawList, origin, size.x);
    DrawGameGrid(origin, size.x, gridTop);
    DrawFooter(drawList, origin, size);

    // On Windows 11 DWM draws a rounded native border; elsewhere draw our own.
    if (!g_nativeRoundedCorners)
        drawList->AddRect(origin, origin + size, kColHairlineStrong);

    DrawModal();
    ImGui::End();
}

void RenderFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    DrawUi();
    ImGui::Render();

    const ImVec4 clear = ToVec4(kColBackground);
    const float clearColor[4] = { clear.x, clear.y, clear.z, clear.w };
    g_context->OMSetRenderTargets(1, g_renderTarget.GetAddressOf(), nullptr);
    g_context->ClearRenderTargetView(g_renderTarget.Get(), clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void ProcessPendingAction() {
    const PendingAction action = g_pendingAction;
    g_pendingAction = PendingAction::None;

    switch (action) {
    case PendingAction::Minimize: ShowWindow(g_hwnd, SW_MINIMIZE); break;
    case PendingAction::Close: PostMessageW(g_hwnd, WM_CLOSE, 0, 0); break;
    case PendingAction::LaunchJampusStrike:
        if (!g_launchSucceeded)
            LaunchJampusStrike();
        break;
    case PendingAction::None: break;
    }
}

// ---------------------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------------------

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return 1;

    switch (msg) {
    case WM_NCHITTEST: {
        // Empty title-bar space acts as a native caption, which gives Windows-managed
        // dragging (no custom drag code, no capture issues). Everything else is client.
        POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &point);
        RECT client = {};
        GetClientRect(hwnd, &client);
        const bool inTitleBar = point.y >= 0 && point.y < static_cast<LONG>(TitleBarHeightPx());
        const bool beforeButtons = point.x >= 0 && point.x < static_cast<LONG>(TitleButtonsLeftPx(static_cast<float>(client.right)));
        return (inTitleBar && beforeButtons) ? HTCAPTION : HTCLIENT;
    }
    case WM_NCLBUTTONDBLCLK:
        if (wParam == HTCAPTION)
            return 0; // Fixed-size window: never maximize from the caption.
        break;
    case WM_SYSCOMMAND:
        switch (wParam & 0xFFF0) {
        case SC_KEYMENU:  // Disable the Alt application menu.
        case SC_MAXIMIZE:
        case SC_SIZE:
            return 0;
        default:
            break;
        }
        break;
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            g_pendingWidth = LOWORD(lParam);
            g_pendingHeight = HIWORD(lParam);
        }
        return 0;
    case WM_DPICHANGED: {
        g_dpiScale = static_cast<float>(LOWORD(wParam)) / static_cast<float>(USER_DEFAULT_SCREEN_DPI);
        g_fontsDirty = true;
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, static_cast<int>(Px(kWindowWidth)),
                     static_cast<int>(Px(kWindowHeight)), SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_CLOSE:
        // Leave the message loop; the window is destroyed during orderly shutdown.
        PostQuitMessage(0);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool CreateMainWindow() {
    POINT cursor = {};
    GetCursorPos(&cursor);
    const HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);

    RECT workArea = {};
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (GetMonitorInfoW(monitor, &monitorInfo))
        workArea = monitorInfo.rcWork;
    else
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

    g_dpiScale = ImGui_ImplWin32_GetDpiScaleForMonitor(monitor);
    if (g_dpiScale <= 0.0f)
        g_dpiScale = 1.0f;

    const int workWidth = workArea.right - workArea.left;
    const int workHeight = workArea.bottom - workArea.top;
    const int width = std::min(static_cast<int>(Px(kWindowWidth)), workWidth);
    const int height = std::min(static_cast<int>(Px(kWindowHeight)), workHeight);
    const int x = workArea.left + (workWidth - width) / 2;
    const int y = workArea.top + (workHeight - height) / 2;

    g_backgroundBrush = CreateSolidBrush(kWindowFillColor);

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_CLASSDC;
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = g_instance;
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = g_backgroundBrush;
    windowClass.lpszClassName = kWindowClassName;
    if (!RegisterClassExW(&windowClass)) {
        LogError(L"Window class registration failed.");
        return false;
    }
    g_init.windowClass = true;

    // WS_MINIMIZEBOX/WS_SYSMENU add no visible chrome to a popup window but enable normal
    // taskbar minimize/restore behaviour and the caption context menu.
    g_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_APPWINDOW, kWindowClassName, kWindowTitle,
                             WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU, x, y, width, height, nullptr, nullptr, g_instance, nullptr);
    if (!g_hwnd) {
        LogError(L"Window creation failed.");
        return false;
    }

    const int cornerPreference = kDwmCornerRound;
    g_nativeRoundedCorners = SUCCEEDED(DwmSetWindowAttribute(g_hwnd, kDwmWindowCornerPreference, &cornerPreference, sizeof(cornerPreference)));
    if (g_nativeRoundedCorners) {
        const COLORREF borderColor = kNativeBorderColor;
        DwmSetWindowAttribute(g_hwnd, kDwmBorderColor, &borderColor, sizeof(borderColor));
    }
    return true;
}

bool InitImGui() {
    IMGUI_CHECKVERSION();
    if (!ImGui::CreateContext())
        return false;
    g_init.imguiContext = true;

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    RebuildFontsAndStyle();

    if (!ImGui_ImplWin32_Init(g_hwnd)) {
        LogError(L"Dear ImGui Win32 backend initialization failed.");
        return false;
    }
    g_init.win32Backend = true;

    if (!ImGui_ImplDX11_Init(g_device.Get(), g_context.Get())) {
        LogError(L"Dear ImGui DX11 backend initialization failed.");
        return false;
    }
    g_init.dx11Backend = true;
    return true;
}

// Blocks until a message arrives or the timeout elapses, without spinning the CPU.
void WaitForMessages(DWORD timeoutMs) {
    MsgWaitForMultipleObjectsEx(0, nullptr, timeoutMs, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
}

int RunMessageLoop() {
    bool occluded = false;

    for (;;) {
        if (IsIconic(g_hwnd))
            WaitForMessages(INFINITE);
        else if (occluded || (GetForegroundWindow() != g_hwnd && !g_animating))
            WaitForMessages(kBackgroundWaitMs);

        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                return 0;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (IsIconic(g_hwnd))
            continue;

        if (occluded) {
            if (g_swapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
                continue;
            occluded = false;
        }

        if (!ApplyPendingResize()) {
            LogError(L"Unable to resize or recreate the swap chain.");
            return 1;
        }

        if (g_fontsDirty) {
            g_fontsDirty = false;
            ImGui_ImplDX11_InvalidateDeviceObjects(); // Device objects (incl. font texture) rebuild on next NewFrame.
            RebuildFontsAndStyle();
        }

        RenderFrame();

        const HRESULT hr = g_swapChain->Present(1, 0);
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            if (!RecreateGraphics()) {
                LogError(L"Unable to recover from a lost graphics device.");
                return 1;
            }
            continue;
        }
        occluded = hr == DXGI_STATUS_OCCLUDED;

        ProcessPendingAction();
    }
}

void Shutdown() {
    if (g_init.dx11Backend)
        ImGui_ImplDX11_Shutdown();
    if (g_init.win32Backend)
        ImGui_ImplWin32_Shutdown();
    if (g_init.imguiContext)
        ImGui::DestroyContext();
    g_init.dx11Backend = g_init.win32Backend = g_init.imguiContext = false;

    CleanupDeviceD3D();
    for (GameArt& art : g_art)
        art.pixels = {};

    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
    }
    if (g_init.windowClass) {
        UnregisterClassW(kWindowClassName, g_instance);
        g_init.windowClass = false;
    }
    if (g_backgroundBrush) {
        DeleteObject(g_backgroundBrush);
        g_backgroundBrush = nullptr;
    }
    if (g_init.com) {
        CoUninitialize();
        g_init.com = false;
    }
}

} // namespace

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int) {
    g_instance = instance;
    ImGui_ImplWin32_EnableDpiAwareness();

    // COM is required by WIC (artwork decoding) and by ShellExecuteExW's shell extensions.
    g_init.com = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));
    LoadGameArt();

    if (!CreateMainWindow() || !CreateDeviceD3D(g_hwnd) || !InitImGui()) {
        Shutdown();
        return 1;
    }

    ShowWindow(g_hwnd, SW_SHOWNORMAL);
    UpdateWindow(g_hwnd);
    SetForegroundWindow(g_hwnd);

    const int exitCode = RunMessageLoop();
    Shutdown();
    return exitCode;
}
