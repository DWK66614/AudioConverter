#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <uxtheme.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdio>
#include <cwchar>
#include <algorithm>
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define WM_CONVERT_PROGRESS  (WM_USER + 1)
#define WM_CONVERT_DONE      (WM_USER + 2)
#define WM_CONVERT_ERROR     (WM_USER + 3)
#define WM_APPEND_LOG        (WM_USER + 4)

#define IDM_ABOUT            2001
#define IDM_FILE_EXIT        2002
#define IDM_HELP_ABOUT       2003

#define COLOR_BG        RGB(240, 242, 245)
#define COLOR_CARD      RGB(255, 255, 255)
#define COLOR_ACCENT    RGB(22,  119, 255)
#define COLOR_ACCENT_H  RGB(64,  150, 255)
#define COLOR_TEXT      RGB(31,  31,  31)
#define COLOR_SUBTEXT   RGB(102, 102, 102)
#define COLOR_BORDER    RGB(229, 231, 235)
#define COLOR_SUCCESS   RGB(82,  196, 26)
#define COLOR_ERROR     RGB(255, 77,  79)
#define COLOR_WARN      RGB(250, 173, 20)
#define COLOR_HEADER_BG RGB(22,  119, 255)

struct FormatInfo {
    const wchar_t* name;
    const wchar_t* ext;
    const char* codec;
    bool lossless;
};

static const FormatInfo g_formats[] = {
    { L"MP3",                          L".mp3",  "libmp3lame",    false },
    { L"WAV (PCM)",                    L".wav",  "pcm_s16le",     true  },
    { L"FLAC (无损)",                   L".flac", "flac",          true  },
    { L"AAC / M4A",                    L".m4a",  "aac",           false },
    { L"OGG Vorbis",                   L".ogg",  "libvorbis",     false },
    { L"WMA",                          L".wma",  "wmav2",         false },
    { L"Opus",                         L".opus", "libopus",       false },
};

struct QualityInfo {
    const wchar_t* name;
    const char* bitrate;
};

static const QualityInfo g_qualities_lossy[] = {
    { L"低 (128 kbps)",     "128k"  },
    { L"标准 (192 kbps)",   "192k"  },
    { L"高 (256 kbps)",     "256k"  },
    { L"最佳 (320 kbps)",   "320k"  },
};

static const QualityInfo g_qualities_lossless[] = {
    { L"无损",              ""      },
};

static HWND g_hWnd           = nullptr;
static HWND g_hInput         = nullptr;
static HWND g_hFormatCombo   = nullptr;
static HWND g_hQualityCombo  = nullptr;
static HWND g_hOutput        = nullptr;
static HWND g_hConvertBtn    = nullptr;
static HWND g_hProgressBar   = nullptr;
static HWND g_hStatus        = nullptr;
static HWND g_hLog           = nullptr;
static HWND g_hCancelBtn     = nullptr;
static HINSTANCE g_hInst     = nullptr;
static HFONT g_hFontNormal   = nullptr;
static HFONT g_hFontBold     = nullptr;
static HFONT g_hFontTitle    = nullptr;
static HBRUSH g_hBrushBG     = nullptr;
static HBRUSH g_hBrushCard   = nullptr;
static HBRUSH g_hBrushAccent = nullptr;
static HANDLE g_hFFmpegProc  = nullptr;
static bool g_btnHovered     = false;
static bool g_btnPressed     = false;
static TRACKMOUSEEVENT g_tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, 0, 0 };

static std::atomic<bool> g_converting(false);
static std::thread g_workerThread;
static std::vector<std::wstring> g_batchFiles;

static void EnableControls(BOOL enable) {
    EnableWindow(g_hInput,        enable);
    EnableWindow(GetDlgItem(g_hWnd, IDC_BROWSE_INPUT),  enable);
    EnableWindow(g_hFormatCombo,  enable);
    EnableWindow(g_hQualityCombo, enable);
    EnableWindow(g_hOutput,       enable);
    EnableWindow(GetDlgItem(g_hWnd, IDC_BROWSE_OUTPUT), enable);
    ShowWindow(g_hConvertBtn, enable ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hCancelBtn,  enable ? SW_HIDE : SW_SHOW);
    if (!enable) SetFocus(g_hCancelBtn);
}

static void AppendLog(const wchar_t* text) {
    int len = GetWindowTextLength(g_hLog);
    SendMessage(g_hLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessage(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)text);
    SendMessage(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
}

static void SetStatus(const wchar_t* text) {
    SetWindowText(g_hStatus, text);
}

static bool FindFFmpeg(wchar_t* path, DWORD size) {
    const wchar_t* names[] = { L"ffmpeg.exe", L"ffmpeg" };
    wchar_t exeDir[MAX_PATH];
    GetModuleFileName(nullptr, exeDir, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exeDir, L'\\');
    if (lastSlash) *lastSlash = L'\0';
    for (auto name : names) {
        swprintf(path, size, L"%ls\\%ls", exeDir, name);
        if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES) return true;
    }
    wchar_t sysPath[MAX_PATH * 2];
    GetEnvironmentVariable(L"PATH", sysPath, MAX_PATH * 2);
    for (auto name : names) {
        wchar_t* ctx = nullptr;
        wchar_t* token = wcstok(sysPath, L";", &ctx);
        while (token) {
            swprintf(path, size, L"%ls\\%ls", token, name);
            if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES) return true;
            token = wcstok(nullptr, L";", &ctx);
        }
    }
    return false;
}

static double ParseTime(const char* timeStr) {
    int h = 0, m = 0; double s = 0;
    sscanf(timeStr, "%d:%d:%lf", &h, &m, &s);
    return h * 3600.0 + m * 60.0 + s;
}

static void ConvertThreadProc(HWND hWnd, std::wstring inputFile, std::wstring outputFile,
                               const char* codec, const char* bitrate, bool lossless) {
    wchar_t ffmpegPath[MAX_PATH];
    if (!FindFFmpeg(ffmpegPath, MAX_PATH)) {
        const wchar_t* msg = L"未找到 FFmpeg。请将 ffmpeg.exe 放在程序目录下，或添加到系统 PATH 中。\r\n"
                              L"下载地址: https://ffmpeg.org/download.html";
        wchar_t* copy = new wchar_t[wcslen(msg) + 1];
        wcscpy(copy, msg);
        PostMessage(hWnd, WM_CONVERT_ERROR, 0, (LPARAM)copy);
        return;
    }

    std::wstring cmd = L"\"";
    cmd += ffmpegPath;
    cmd += L"\" -y -i \"";
    cmd += inputFile;
    cmd += L"\" -acodec ";
    int clen = (int)strlen(codec);
    wchar_t* wcodec = new wchar_t[clen + 1];
    MultiByteToWideChar(CP_UTF8, 0, codec, -1, wcodec, clen + 1);
    cmd += wcodec;

    if (bitrate && strlen(bitrate) > 0) {
        if (strcmp(codec, "libvorbis") == 0) {
            cmd = L"\"" + std::wstring(ffmpegPath) + L"\" -y -i \"" + inputFile + L"\" -acodec libvorbis -q:a ";
            if (strcmp(bitrate, "128k") == 0) cmd += L"3";
            else if (strcmp(bitrate, "192k") == 0) cmd += L"5";
            else if (strcmp(bitrate, "256k") == 0) cmd += L"7";
            else cmd += L"9";
        } else {
            cmd += L" -b:a ";
            int blen = (int)strlen(bitrate);
            wchar_t* wbr = new wchar_t[blen + 1];
            MultiByteToWideChar(CP_UTF8, 0, bitrate, -1, wbr, blen + 1);
            cmd += wbr;
            delete[] wbr;
        }
    }
    delete[] wcodec;

    cmd += L" -strict -2 -vn -progress pipe:1 -nostats -loglevel error \"";
    cmd += outputFile;
    cmd += L"\"";

    // Capture stderr separately for error messages
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    HANDLE hOutRd = nullptr, hOutWr = nullptr;
    HANDLE hErrRd = nullptr, hErrWr = nullptr;
    if (!CreatePipe(&hOutRd, &hOutWr, &sa, 0)) return;
    if (!CreatePipe(&hErrRd, &hErrWr, &sa, 0)) { CloseHandle(hOutRd); CloseHandle(hOutWr); return; }
    SetHandleInformation(hOutRd, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hErrRd, HANDLE_FLAG_INHERIT, 0);

    PROCESS_INFORMATION pi = {};
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hOutWr;
    si.hStdError  = hErrWr;

    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    if (!CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hOutRd); CloseHandle(hOutWr);
        CloseHandle(hErrRd); CloseHandle(hErrWr);
        g_hFFmpegProc = nullptr;
        const wchar_t* msg = L"无法启动 FFmpeg 进程。";
        wchar_t* copy = new wchar_t[wcslen(msg) + 1];
        wcscpy(copy, msg);
        PostMessage(hWnd, WM_CONVERT_ERROR, 0, (LPARAM)copy);
        return;
    }
    CloseHandle(pi.hThread);
    CloseHandle(hOutWr);
    CloseHandle(hErrWr);
    g_hFFmpegProc = pi.hProcess;

    char buf[4096]; DWORD bytesRead;
    std::string lineBuf;
    std::string errBuf;
    double duration = 0;

    while (true) {
        bool hasData = false;

        // Read stdout (progress)
        DWORD avail = 0;
        if (PeekNamedPipe(hOutRd, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
            if (ReadFile(hOutRd, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                hasData = true;
                buf[bytesRead] = '\0';
                lineBuf += buf;
                size_t pos;
                while ((pos = lineBuf.find('\n')) != std::string::npos) {
                    std::string line = lineBuf.substr(0, pos);
                    lineBuf = lineBuf.substr(pos + 1);
                    if (line.find("out_time_ms=") == 0) {
                        long long timeUs = _atoi64(line.c_str() + 13);
                        double cur = timeUs / 1000000.0;
                        int pct = (duration > 0) ? (int)((cur / duration) * 100.0) : 0;
                        if (pct < 0) pct = 0; if (pct > 100) pct = 100;
                        PostMessage(hWnd, WM_CONVERT_PROGRESS, (WPARAM)pct, 0);
                    } else if (line.find("out_time=") == 0) {
                        std::string ts = line.substr(9);
                        ts.erase(std::remove(ts.begin(), ts.end(), ' '), ts.end());
                        double cur = ParseTime(ts.c_str());
                        int pct = (duration > 0) ? (int)((cur / duration) * 100.0) : 0;
                        if (pct < 0) pct = 0; if (pct > 100) pct = 100;
                        PostMessage(hWnd, WM_CONVERT_PROGRESS, (WPARAM)pct, 0);
                    }
                }
            }
        }

        // Read stderr (error messages)
        avail = 0;
        if (PeekNamedPipe(hErrRd, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
            if (ReadFile(hErrRd, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                hasData = true;
                buf[bytesRead] = '\0';
                errBuf += buf;
            }
        }

        if (!hasData) {
            if (WaitForSingleObject(pi.hProcess, 100) == WAIT_OBJECT_0) break;
        }
    }

    // Drain remaining data
    DWORD avail = 0;
    while (PeekNamedPipe(hOutRd, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
        if (ReadFile(hOutRd, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0)
            lineBuf.append(buf, bytesRead);
        avail = 0;
    }
    avail = 0;
    while (PeekNamedPipe(hErrRd, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
        if (ReadFile(hErrRd, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0)
            errBuf.append(buf, bytesRead);
        avail = 0;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(hOutRd);
    CloseHandle(hErrRd);
    g_hFFmpegProc = nullptr;

    if (exitCode == 0) {
        PostMessage(hWnd, WM_CONVERT_PROGRESS, (WPARAM)100, 0);
        PostMessage(hWnd, WM_CONVERT_DONE, 0, (LPARAM)L"转换成功！");
    } else {
        // Show the actual FFmpeg error
        std::wstring errMsg = L"FFmpeg 转换失败 (错误码 " + std::to_wstring(exitCode) + L")\r\n";
        if (!errBuf.empty()) {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, errBuf.c_str(), -1, nullptr, 0);
            if (wlen > 1) {
                wchar_t* werr = new wchar_t[wlen];
                MultiByteToWideChar(CP_UTF8, 0, errBuf.c_str(), -1, werr, wlen);
                errMsg += werr;
                delete[] werr;
            }
        }
        wchar_t* msgCopy = new wchar_t[errMsg.length() + 1];
        wcscpy(msgCopy, errMsg.c_str());
        PostMessage(hWnd, WM_CONVERT_ERROR, 0, (LPARAM)msgCopy);
    }
    g_converting = false;
}

static void StartConversion() {
    if (g_converting) return;

    if (g_batchFiles.empty()) {
        wchar_t inputFile[MAX_PATH] = {};
        GetWindowText(g_hInput, inputFile, MAX_PATH);
        if (wcslen(inputFile) == 0) {
            MessageBox(g_hWnd, L"请先选择要转换的音频文件。", L"提示", MB_ICONWARNING);
            return;
        }
        g_batchFiles.push_back(inputFile);
    }

    int fmtIdx = (int)SendMessage(g_hFormatCombo, CB_GETCURSEL, 0, 0);
    if (fmtIdx < 0) fmtIdx = 0;
    const FormatInfo& fmt = g_formats[fmtIdx];

    int qualIdx = (int)SendMessage(g_hQualityCombo, CB_GETCURSEL, 0, 0);
    if (qualIdx < 0) qualIdx = 2;

    const char* bitrate = fmt.lossless ? "" : g_qualities_lossy[qualIdx].bitrate;

    SetWindowText(g_hLog, L"");
    AppendLog(L"═══════════════════════════════");
    AppendLog(L"  音频格式转换器 - 开始转换");
    AppendLog(L"═══════════════════════════════");
    wchar_t logMsg[1024];
    swprintf(logMsg, L"  文件数量 : %zu 个", g_batchFiles.size());
    AppendLog(logMsg);
    swprintf(logMsg, L"  输出格式 : %ls", fmt.name);
    AppendLog(logMsg);
    if (bitrate && strlen(bitrate) > 0) {
        swprintf(logMsg, L"  比特率   : %hs", bitrate);
    } else {
        AppendLog(L"  编码模式 : 无损");
    }
    AppendLog(L"═══════════════════════════════");

    SendMessage(g_hProgressBar, PBM_SETMARQUEE, TRUE, 30);
    SetStatus(L"正在转换中...");
    EnableControls(FALSE);
    g_converting = true;

    std::vector<std::wstring> files = g_batchFiles;
    std::string codec(fmt.codec);
    std::string br(bitrate ? bitrate : "");

    g_workerThread = std::thread([files, codec, br, fmt]() {
        for (size_t i = 0; i < files.size() && g_converting; i++) {
            wchar_t outFile[MAX_PATH];
            wcscpy(outFile, files[i].c_str());
            wchar_t* dot = wcsrchr(outFile, L'.');
            if (dot) *dot = L'\0';
            wcscat(outFile, fmt.ext);

            wchar_t msg[1024];
            swprintf(msg, L"[%zu/%zu] %ls", i + 1, files.size(), files[i].c_str());
            PostMessage(g_hWnd, WM_APPEND_LOG, 0, (LPARAM)_wcsdup(msg));

            ConvertThreadProc(g_hWnd, files[i], outFile, codec.c_str(),
                br.empty() ? nullptr : br.c_str(), fmt.lossless);
        }
            if (g_converting) {
                PostMessage(g_hWnd, WM_CONVERT_DONE, 0, (LPARAM)L"全部文件转换完成！");
            }
            g_batchFiles.clear();
            g_converting = false;
    });
    g_workerThread.detach();
}

static void BrowseFile(HWND hEdit, BOOL save) {
    wchar_t file[32768] = {};
    GetWindowText(hEdit, file, 32768);
    OPENFILENAME ofn = { sizeof(OPENFILENAME) };
    ofn.hwndOwner = g_hWnd;
    ofn.lpstrFilter = L"音频文件\0*.wav;*.mp3;*.flac;*.aac;*.m4a;*.ogg;*.opus;*.wma;*.wmv;*.asf;*.aiff;*.aif;*.ape;*.wv\0"
                       "所有文件\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = 32768;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    if (save) {
        int fmtIdx = (int)SendMessage(g_hFormatCombo, CB_GETCURSEL, 0, 0);
        if (fmtIdx < 0) fmtIdx = 0;
        ofn.lpstrDefExt = g_formats[fmtIdx].ext + 1;
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
        if (GetSaveFileName(&ofn)) SetWindowText(hEdit, file);
    } else {
        if (GetOpenFileName(&ofn)) {
            g_batchFiles.clear();
            wchar_t* p = file;
            wchar_t dir[MAX_PATH];
            wcscpy(dir, p);
            p += wcslen(p) + 1;
            if (*p == L'\0') {
                g_batchFiles.push_back(dir);
                SetWindowText(hEdit, dir);
            } else {
                int n = 0;
                wchar_t firstFile[MAX_PATH];
                while (*p) {
                    wchar_t fullPath[MAX_PATH];
                    swprintf(fullPath, L"%ls\\%ls", dir, p);
                    g_batchFiles.push_back(fullPath);
                    if (n == 0) { wcscpy(firstFile, fullPath); SetWindowText(hEdit, fullPath); }
                    p += wcslen(p) + 1;
                    n++;
                }
                wchar_t info[128];
                swprintf(info, L"已选择 %d 个文件", n);
                SetStatus(info);
            }
            if (!g_batchFiles.empty()) {
                wchar_t outputFile[MAX_PATH];
                wcscpy(outputFile, g_batchFiles[0].c_str());
                wchar_t* dot = wcsrchr(outputFile, L'.');
                if (dot) *dot = L'\0';
                int fmtIdx = (int)SendMessage(g_hFormatCombo, CB_GETCURSEL, 0, 0);
                if (fmtIdx < 0) fmtIdx = 0;
                wcscat(outputFile, g_formats[fmtIdx].ext);
                SetWindowText(g_hOutput, outputFile);
            }
        }
    }
}

static void OnFormatChanged() {
    int fmtIdx = (int)SendMessage(g_hFormatCombo, CB_GETCURSEL, 0, 0);
    if (fmtIdx < 0) return;
    const FormatInfo& fmt = g_formats[fmtIdx];
    SendMessage(g_hQualityCombo, CB_RESETCONTENT, 0, 0);
    if (fmt.lossless) {
        for (auto& q : g_qualities_lossless)
            SendMessage(g_hQualityCombo, CB_ADDSTRING, 0, (LPARAM)q.name);
        SendMessage(g_hQualityCombo, CB_SETCURSEL, 0, 0);
    } else {
        for (auto& q : g_qualities_lossy)
            SendMessage(g_hQualityCombo, CB_ADDSTRING, 0, (LPARAM)q.name);
        SendMessage(g_hQualityCombo, CB_SETCURSEL, 2, 0);
    }
    wchar_t inputFile[MAX_PATH] = {};
    GetWindowText(g_hInput, inputFile, MAX_PATH);
    if (wcslen(inputFile) > 0) {
        wchar_t outputFile[MAX_PATH];
        wcscpy(outputFile, inputFile);
        wchar_t* dot = wcsrchr(outputFile, L'.');
        if (dot) *dot = L'\0';
        wcscat(outputFile, fmt.ext);
        SetWindowText(g_hOutput, outputFile);
    }
}

static void DrawRoundedRect(HDC hdc, RECT& r, int radius, COLORREF border, COLORREF fill) {
    HPEN hPen = CreatePen(PS_SOLID, 1, border);
    HBRUSH hBrush = CreateSolidBrush(fill);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    RoundRect(hdc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
    DeleteObject(hBrush);
}

static void DrawButton(HDC hdc, RECT& r) {
    int radius = 8;
    COLORREF bg = g_btnPressed ? COLOR_ACCENT : (g_btnHovered ? COLOR_ACCENT_H : COLOR_ACCENT);
    HPEN hPen = CreatePen(PS_SOLID, 1, bg);
    HBRUSH hBrush = CreateSolidBrush(bg);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    RoundRect(hdc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
    DeleteObject(hBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    SelectObject(hdc, g_hFontBold);
    wchar_t text[] = L"开始转换";
    DrawText(hdc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawCard(HDC hdc, RECT& r) {
    HPEN hPen = CreatePen(PS_SOLID, 1, COLOR_BORDER);
    HBRUSH hBrush = CreateSolidBrush(COLOR_CARD);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    RoundRect(hdc, r.left, r.top, r.right, r.bottom, 10, 10);
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
    DeleteObject(hBrush);
}

static void DrawHeader(HDC hdc, RECT& r) {
    HBRUSH hBrush = CreateSolidBrush(COLOR_HEADER_BG);
    FillRect(hdc, &r, hBrush);
    DeleteObject(hBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    SelectObject(hdc, g_hFontTitle);

    RECT tr = r;
    tr.top += 14;
    DrawText(hdc, L"音频格式转换器", -1, &tr, DT_CENTER | DT_SINGLELINE);

    tr.top += 34;
    SelectObject(hdc, g_hFontNormal);
    SetTextColor(hdc, RGB(200, 220, 255));
    DrawText(hdc, L"支持 MP3 / WAV / FLAC / AAC / OGG / WMA / Opus 等主流格式互转", -1, &tr, DT_CENTER | DT_SINGLELINE);
}

static INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    switch (msg) {
        case WM_INITDIALOG: {
            SetWindowText(hDlg, L"关于 音频格式转换器");
            SetDlgItemText(hDlg, 101, L"音频格式转换器 v1.0");
            SetDlgItemText(hDlg, 102, L"基于 C++ 和 FFmpeg 开发的 Windows 桌面音频转换工具。\r\n\r\n"
                                       L"支持 MP3 / WAV / FLAC / AAC / OGG / WMA / Opus\r\n"
                                       L"7 种主流音频格式互相转换。");
            HFONT hFont = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
            SendDlgItemMessage(hDlg, 101, WM_SETFONT, (WPARAM)hFont, TRUE);
            return TRUE;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, 0);
                return TRUE;
            }
            break;
    }
    return FALSE;
}

static void SaveSettings() {
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\AudioConverter", 0, nullptr,
                       REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD fmtIdx = (DWORD)SendMessage(g_hFormatCombo, CB_GETCURSEL, 0, 0);
        DWORD qualIdx = (DWORD)SendMessage(g_hQualityCombo, CB_GETCURSEL, 0, 0);
        RegSetValueEx(hKey, L"FormatIndex", 0, REG_DWORD, (BYTE*)&fmtIdx, sizeof(DWORD));
        RegSetValueEx(hKey, L"QualityIndex", 0, REG_DWORD, (BYTE*)&qualIdx, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

static void LoadSettings() {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\AudioConverter", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD fmtIdx = 0, qualIdx = 2, size = sizeof(DWORD);
        RegQueryValueEx(hKey, L"FormatIndex", nullptr, nullptr, (BYTE*)&fmtIdx, &size);
        RegQueryValueEx(hKey, L"QualityIndex", nullptr, nullptr, (BYTE*)&qualIdx, &size);
        RegCloseKey(hKey);
        if (fmtIdx < sizeof(g_formats) / sizeof(g_formats[0])) {
            SendMessage(g_hFormatCombo, CB_SETCURSEL, (WPARAM)fmtIdx, 0);
            OnFormatChanged();
            SendMessage(g_hQualityCombo, CB_SETCURSEL, (WPARAM)qualIdx, 0);
        }
    }
}

LRESULT CALLBACK BtnSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                  UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (msg) {
        case WM_MOUSEMOVE:
            if (!g_btnHovered) { g_btnHovered = true; InvalidateRect(hWnd, nullptr, FALSE); }
            TrackMouseEvent(&g_tme);
            break;
        case WM_MOUSELEAVE:
            g_btnHovered = false; g_btnPressed = false;
            InvalidateRect(hWnd, nullptr, FALSE);
            break;
        case WM_LBUTTONDOWN:
            g_btnPressed = true; InvalidateRect(hWnd, nullptr, FALSE);
            break;
        case WM_LBUTTONUP:
            g_btnPressed = false; InvalidateRect(hWnd, nullptr, FALSE);
            break;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT r;
            GetClientRect(hWnd, &r);
            DrawButton(hdc, r);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, BtnSubclassProc, uIdSubclass);
            break;
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hWnd = hWnd;
            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
            g_hInst = hInst;

            // Menu bar
            HMENU hMenu = CreateMenu();
            HMENU hFileMenu = CreatePopupMenu();
            AppendMenu(hFileMenu, MF_STRING, IDM_FILE_EXIT, L"退出\tAlt+F4");
            AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hFileMenu, L"文件(&F)");
            HMENU hHelpMenu = CreatePopupMenu();
            AppendMenu(hHelpMenu, MF_STRING, IDM_HELP_ABOUT, L"关于(&A)...");
            AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hHelpMenu, L"帮助(&H)");
            SetMenu(hWnd, hMenu);

            g_hFontNormal = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
            g_hFontBold = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
            g_hFontTitle = CreateFont(26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
            g_hBrushBG = CreateSolidBrush(COLOR_BG);
            g_hBrushCard = CreateSolidBrush(COLOR_CARD);
            g_hBrushAccent = CreateSolidBrush(COLOR_ACCENT);

            int yBase = 100;
            int gap = 8;
            int card1H = 90;
            int card2Top = yBase + card1H + gap;
            int card2H = 230;

            // Card 1 - Input file label
            CreateWindow(L"STATIC", L"输入文件",
                WS_CHILD | WS_VISIBLE,
                30, yBase + 12, 150, 24, hWnd, (HMENU)IDC_LABEL_INPUT, hInst, nullptr);

            // Input file edit
            g_hInput = CreateWindowEx(0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
                30, yBase + 40, 470, 32, hWnd, (HMENU)IDC_INPUT_FILE, hInst, nullptr);

            // Browse input
            CreateWindow(L"BUTTON", L"浏览...",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                510, yBase + 39, 100, 34, hWnd, (HMENU)IDC_BROWSE_INPUT, hInst, nullptr);

            // Card 2 title
            CreateWindow(L"STATIC", L"输出设置",
                WS_CHILD | WS_VISIBLE,
                30, card2Top + 14, 150, 24, hWnd, (HMENU)IDC_LABEL_LOG, hInst, nullptr);

            // Format label
            CreateWindow(L"STATIC", L"输出格式",
                WS_CHILD | WS_VISIBLE,
                30, card2Top + 46, 100, 22, hWnd, (HMENU)IDC_LABEL_FORMAT, hInst, nullptr);

            // Format combo
            g_hFormatCombo = CreateWindow(L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
                30, card2Top + 66, 235, 200, hWnd, (HMENU)IDC_FORMAT_COMBO, hInst, nullptr);

            for (auto& fmt : g_formats)
                SendMessage(g_hFormatCombo, CB_ADDSTRING, 0, (LPARAM)fmt.name);
            SendMessage(g_hFormatCombo, CB_SETCURSEL, 0, 0);

            // Quality label
            CreateWindow(L"STATIC", L"音质",
                WS_CHILD | WS_VISIBLE,
                280, card2Top + 46, 100, 22, hWnd, (HMENU)IDC_LABEL_QUALITY, hInst, nullptr);

            // Quality combo
            g_hQualityCombo = CreateWindow(L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
                280, card2Top + 66, 235, 200, hWnd, (HMENU)IDC_QUALITY_COMBO, hInst, nullptr);

            for (auto& q : g_qualities_lossy)
                SendMessage(g_hQualityCombo, CB_ADDSTRING, 0, (LPARAM)q.name);
            SendMessage(g_hQualityCombo, CB_SETCURSEL, 2, 0);

            // Output path label
            CreateWindow(L"STATIC", L"输出路径",
                WS_CHILD | WS_VISIBLE,
                30, card2Top + 112, 100, 22, hWnd, (HMENU)IDC_LABEL_OUTPUT, hInst, nullptr);

            // Output file edit
            g_hOutput = CreateWindowEx(0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
                30, card2Top + 132, 470, 32, hWnd, (HMENU)IDC_OUTPUT_FILE, hInst, nullptr);

            // Browse output
            CreateWindow(L"BUTTON", L"浏览...",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                510, card2Top + 131, 100, 34, hWnd, (HMENU)IDC_BROWSE_OUTPUT, hInst, nullptr);

            // Convert button
            g_hConvertBtn = CreateWindow(L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                195, card2Top + 182, 260, 48, hWnd, (HMENU)IDC_CONVERT_BTN, hInst, nullptr);

            // Cancel button (hidden)
            g_hCancelBtn = CreateWindow(L"BUTTON", L"取消转换",
                WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
                195, card2Top + 182, 260, 48, hWnd, (HMENU)IDC_CONVERT_BTN, hInst, nullptr);
            ShowWindow(g_hCancelBtn, SW_HIDE);

            // Progress bar
            int barY = card2Top + card2H + 10;
            g_hProgressBar = CreateWindow(PROGRESS_CLASS, L"",
                WS_CHILD | WS_VISIBLE | PBS_MARQUEE,
                30, barY, 580, 24, hWnd, (HMENU)IDC_PROGRESS_BAR, hInst, nullptr);
            SendMessage(g_hProgressBar, PBM_SETMARQUEE, TRUE, 30);

            // Status
            g_hStatus = CreateWindow(L"STATIC", L"就绪 - 请选择文件开始转换",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                30, barY + 30, 580, 26, hWnd, (HMENU)IDC_STATUS_TEXT, hInst, nullptr);

            // Log
            int logY = barY + 62;
            g_hLog = CreateWindowEx(0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL |
                ES_READONLY | WS_VSCROLL | WS_TABSTOP,
                30, logY, 580, 120, hWnd, (HMENU)IDC_LOG_EDIT, hInst, nullptr);

            // Apply normal font to all children
            EnumChildWindows(hWnd, [](HWND hChild, LPARAM lParam) -> BOOL {
                SendMessage(hChild, WM_SETFONT, (WPARAM)lParam, TRUE);
                return TRUE;
            }, (LPARAM)g_hFontNormal);

            // Bold font for section labels + Convert button
            SendMessage(GetDlgItem(hWnd, IDC_LABEL_INPUT), WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
            SendMessage(GetDlgItem(hWnd, IDC_LABEL_LOG), WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
            SendMessage(g_hConvertBtn, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            // Status font
            HFONT hStatusFont = CreateFont(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
            SendMessage(g_hStatus, WM_SETFONT, (WPARAM)hStatusFont, TRUE);

            // Log font
            HFONT hLogFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
            SendMessage(g_hLog, WM_SETFONT, (WPARAM)hLogFont, TRUE);

            SetWindowSubclass(g_hConvertBtn, BtnSubclassProc, 1, 0);

            // Cancel button font + style
            SendMessage(g_hCancelBtn, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            OnFormatChanged();
            LoadSettings();

            // Enable drag & drop
            DragAcceptFiles(hWnd, TRUE);

            wchar_t ffmpegPath[MAX_PATH];
            if (!FindFFmpeg(ffmpegPath, MAX_PATH)) {
                SetStatus(L"提示: 未找到 FFmpeg，请将 ffmpeg.exe 放在程序目录下");
            }

            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hCtrl = (HWND)lParam;
            SetBkMode(hdc, TRANSPARENT);
            if (hCtrl == g_hStatus) {
                SetTextColor(hdc, COLOR_SUBTEXT);
                return (LRESULT)g_hBrushBG;
            }
            SetTextColor(hdc, COLOR_TEXT);
            // White background for card labels (they sit on white cards)
            SetBkColor(hdc, COLOR_CARD);
            return (LRESULT)g_hBrushCard;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, RGB(250, 250, 250));
            SetTextColor(hdc, COLOR_TEXT);
            HBRUSH hBr = CreateSolidBrush(RGB(250, 250, 250));
            return (LRESULT)hBr;
        }

        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)g_hBrushBG;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            switch (id) {
                case IDC_BROWSE_INPUT: BrowseFile(g_hInput, FALSE); break;
                case IDC_BROWSE_OUTPUT: BrowseFile(g_hOutput, TRUE); break;
                case IDC_CONVERT_BTN:
                    if (g_converting) {
                        if (g_hFFmpegProc) {
                            TerminateProcess(g_hFFmpegProc, 1);
                            CloseHandle(g_hFFmpegProc);
                            g_hFFmpegProc = nullptr;
                        }
                        SetStatus(L"已取消");
                        AppendLog(L"转换已被用户取消");
                        AppendLog(L"═══════════════════════════════");
                        SendMessage(g_hProgressBar, PBM_SETMARQUEE, FALSE, 0);
                        SendMessage(g_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
                        SendMessage(g_hProgressBar, PBM_SETPOS, 0, 0);
                        EnableControls(TRUE);
                        g_batchFiles.clear();
                        g_converting = false;
                    } else {
                        StartConversion();
                    }
                    break;
                case IDC_FORMAT_COMBO:
                    if (HIWORD(wParam) == CBN_SELCHANGE) OnFormatChanged();
                    break;
                case IDM_HELP_ABOUT:
                    DialogBox(g_hInst, MAKEINTRESOURCE(DLG_ABOUT), hWnd, AboutDlgProc);
                    break;
                case IDM_FILE_EXIT:
                    DestroyWindow(hWnd);
                    break;
            }
            return 0;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_RETURN && !g_converting) {
                StartConversion();
                return 0;
            }
            if (wParam == VK_ESCAPE && g_converting && g_hFFmpegProc) {
                TerminateProcess(g_hFFmpegProc, 1);
                CloseHandle(g_hFFmpegProc);
                g_hFFmpegProc = nullptr;
                SetStatus(L"已取消");
                AppendLog(L"转换已被用户取消");
                AppendLog(L"═══════════════════════════════");
                SendMessage(g_hProgressBar, PBM_SETMARQUEE, FALSE, 0);
                SendMessage(g_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
                SendMessage(g_hProgressBar, PBM_SETPOS, 0, 0);
                EnableControls(TRUE);
                g_batchFiles.clear();
                g_converting = false;
                return 0;
            }
            break;
        }

        case WM_APPEND_LOG: {
            if (lParam) {
                AppendLog((const wchar_t*)lParam);
                free((void*)lParam);
            }
            return 0;
        }

        case WM_CONVERT_PROGRESS: {
            int progress = (int)wParam;
            (void)progress;
            return 0;
        }

        case WM_CONVERT_DONE: {
            SendMessage(g_hProgressBar, PBM_SETMARQUEE, FALSE, 0);
            SendMessage(g_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            SendMessage(g_hProgressBar, PBM_SETPOS, 100, 0);
            SetStatus(L"转换完成!");
            if (lParam) AppendLog((const wchar_t*)lParam);
            if (g_batchFiles.size() > 1) {
                wchar_t dir[MAX_PATH];
                wcscpy(dir, g_batchFiles[0].c_str());
                wchar_t* slash = wcsrchr(dir, L'\\');
                if (slash) *slash = L'\0';
                wchar_t msg[512];
                swprintf(msg, L"输出目录: %ls", dir);
                AppendLog(msg);
                ShellExecute(hWnd, L"open", dir, nullptr, nullptr, SW_SHOW);
            }
            AppendLog(L"═══════════════════════════════");
            EnableControls(TRUE);
            g_batchFiles.clear();
            g_converting = false;
            return 0;
        }

        case WM_CONVERT_ERROR: {
            SendMessage(g_hProgressBar, PBM_SETMARQUEE, FALSE, 0);
            SendMessage(g_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            SendMessage(g_hProgressBar, PBM_SETPOS, 0, 0);
            SetStatus(L"转换失败!");
            if (lParam) {
                AppendLog((const wchar_t*)lParam);
                delete[] (wchar_t*)lParam;
            }
            AppendLog(L"═══════════════════════════════");
            EnableControls(TRUE);
            g_batchFiles.clear();
            g_converting = false;
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);

            HBRUSH hBG = CreateSolidBrush(COLOR_BG);
            FillRect(hdc, &rc, hBG);
            DeleteObject(hBG);

            // Header
            RECT hdr = rc;
            hdr.bottom = 84;
            DrawHeader(hdc, hdr);

            int yBase = 100;
            int gap = 8;
            int card1H = 90;
            int card2Top = yBase + card1H + gap;
            int card2H = 230;

            // Card 1: Input
            RECT card1 = { 15, yBase, rc.right - 15, yBase + card1H };
            DrawCard(hdc, card1);

            // Card 2: Settings
            RECT card2 = { 15, card2Top, rc.right - 15, card2Top + card2H };
            DrawCard(hdc, card2);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            mmi->ptMinTrackSize.x = 550;
            mmi->ptMinTrackSize.y = 550;
            return 0;
        }

        case WM_SIZE: {
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);
            if (w == 0 || h == 0) return 0;

            int yBase = 100;
            int cardPad = 15;
            int innerPad = 30;
            int btnW = 100;
            int gapV = 8;
            int card1H = 90;
            int card2Top = yBase + card1H + gapV;
            int card2H = 230;

            // Card 1
            RECT card1 = { cardPad, yBase, w - cardPad, yBase + card1H };
            InvalidateRect(hWnd, &card1, FALSE);

            int editW = w - innerPad - btnW - 10 - innerPad;

            SetWindowPos(GetDlgItem(hWnd, IDC_LABEL_INPUT), nullptr,
                innerPad, yBase + 12, 200, 24, SWP_NOZORDER);
            SetWindowPos(g_hInput, nullptr,
                innerPad, yBase + 40, editW, 32, SWP_NOZORDER);
            SetWindowPos(GetDlgItem(hWnd, IDC_BROWSE_INPUT), nullptr,
                innerPad + editW + 10, yBase + 39, btnW, 34, SWP_NOZORDER);

            // Card 2
            RECT card2 = { cardPad, card2Top, w - cardPad, card2Top + card2H };
            InvalidateRect(hWnd, &card2, FALSE);

            SetWindowPos(GetDlgItem(hWnd, IDC_LABEL_LOG), nullptr,
                innerPad, card2Top + 14, 200, 24, SWP_NOZORDER);

            SetWindowPos(GetDlgItem(hWnd, IDC_LABEL_FORMAT), nullptr,
                innerPad, card2Top + 46, 100, 22, SWP_NOZORDER);
            int halfW = (w - innerPad * 2 - 10) / 2;
            SetWindowPos(g_hFormatCombo, nullptr,
                innerPad, card2Top + 66, halfW, 200, SWP_NOZORDER);

            SetWindowPos(GetDlgItem(hWnd, IDC_LABEL_QUALITY), nullptr,
                innerPad + halfW + 10, card2Top + 46, 100, 22, SWP_NOZORDER);
            SetWindowPos(g_hQualityCombo, nullptr,
                innerPad + halfW + 10, card2Top + 66, halfW, 200, SWP_NOZORDER);

            SetWindowPos(GetDlgItem(hWnd, IDC_LABEL_OUTPUT), nullptr,
                innerPad, card2Top + 112, 100, 22, SWP_NOZORDER);
            SetWindowPos(g_hOutput, nullptr,
                innerPad, card2Top + 132, editW, 32, SWP_NOZORDER);
            SetWindowPos(GetDlgItem(hWnd, IDC_BROWSE_OUTPUT), nullptr,
                innerPad + editW + 10, card2Top + 131, btnW, 34, SWP_NOZORDER);

            // Convert + Cancel buttons
            SetWindowPos(g_hConvertBtn, nullptr,
                (w - 260) / 2, card2Top + 182, 260, 48, SWP_NOZORDER);
            SetWindowPos(g_hCancelBtn, nullptr,
                (w - 260) / 2, card2Top + 182, 260, 48, SWP_NOZORDER);

            // Progress / Status / Log
            int barY = card2Top + card2H + 10;
            SetWindowPos(g_hProgressBar, nullptr,
                innerPad, barY, w - innerPad * 2, 24, SWP_NOZORDER);
            SetWindowPos(g_hStatus, nullptr,
                innerPad, barY + 30, w - innerPad * 2, 26, SWP_NOZORDER);

            int logY = barY + 62;
            int logH = h - logY - 15;
            if (logH < 40) logH = 40;
            SetWindowPos(g_hLog, nullptr,
                innerPad, logY, w - innerPad * 2, logH, SWP_NOZORDER);

            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            UINT count = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
            g_batchFiles.clear();
            wchar_t file[MAX_PATH];
            for (UINT i = 0; i < count; i++) {
                if (DragQueryFile(hDrop, i, file, MAX_PATH) > 0) {
                    g_batchFiles.push_back(file);
                }
            }
            DragFinish(hDrop);
            if (!g_batchFiles.empty()) {
                SetWindowText(g_hInput, g_batchFiles[0].c_str());
                if (g_batchFiles.size() > 1) {
                    wchar_t info[128];
                    swprintf(info, L"已拖入 %zu 个文件", g_batchFiles.size());
                    SetStatus(info);
                }
            }
            return 0;
        }

        case WM_DESTROY:
            SaveSettings();
            DragAcceptFiles(hWnd, FALSE);
            if (g_hFFmpegProc) {
                TerminateProcess(g_hFFmpegProc, 1);
                CloseHandle(g_hFFmpegProc);
                g_hFFmpegProc = nullptr;
            }
            DeleteObject(g_hFontNormal);
            DeleteObject(g_hFontBold);
            DeleteObject(g_hFontTitle);
            DeleteObject(g_hBrushBG);
            DeleteObject(g_hBrushCard);
            DeleteObject(g_hBrushAccent);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine;

    INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    wc.lpszClassName = L"AudioConverterClass";
    RegisterClass(&wc);

    int width = 680, height = 600;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    HWND hWnd = CreateWindowEx(0, L"AudioConverterClass",
        L"音频格式转换器", WS_OVERLAPPEDWINDOW,
        x, y, width, height, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) return 1;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
