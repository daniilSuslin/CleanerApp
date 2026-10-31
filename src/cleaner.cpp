#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <wchar.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#define ID_CHK_USERTEMP 101
#define ID_CHK_WINTEMP  102
#define ID_CHK_PREFETCH 103
#define ID_CHK_RECYCLE  104
#define ID_LIST_CUSTOM  105
#define ID_EDIT_PATH    106
#define ID_BTN_ADD      107
#define ID_BTN_BROWSE   108
#define ID_BTN_REMOVE   109
#define ID_BTN_CLEAN    110
#define ID_LIST_FILES   111
#define WM_AUTOSCAN     (WM_USER + 1)

HWND hChkUserTemp, hChkWinTemp, hChkPrefetch, hChkRecycle;
HWND hListCustom, hEditPath, hListFiles, hLabelTotalSize, hStatusLabel;
HFONT hFont, hFontBold;

std::wstring GetIniPath() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring s(path);
    size_t pos = s.find_last_of(L".");
    if (pos != std::wstring::npos) s = s.substr(0, pos) + L".ini";
    else s += L".ini";
    return s;
}

void SaveCustomFolders() {
    std::wstring ini = GetIniPath();
    WritePrivateProfileStringW(L"CustomFolders", NULL, NULL, ini.c_str());
    int count = (int)SendMessageW(hListCustom, LB_GETCOUNT, 0, 0);
    for (int i = 0; i < count; i++) {
        wchar_t buf[MAX_PATH];
        SendMessageW(hListCustom, LB_GETTEXT, i, (LPARAM)buf);
        std::wstring key = L"Folder" + std::to_wstring(i);
        WritePrivateProfileStringW(L"CustomFolders", key.c_str(), buf, ini.c_str());
    }
    WritePrivateProfileStringW(L"CustomFolders", L"Count", std::to_wstring(count).c_str(), ini.c_str());
}

void LoadCustomFolders() {
    std::wstring ini = GetIniPath();
    int count = GetPrivateProfileIntW(L"CustomFolders", L"Count", 0, ini.c_str());
    for (int i = 0; i < count; i++) {
        std::wstring key = L"Folder" + std::to_wstring(i);
        wchar_t buf[MAX_PATH];
        GetPrivateProfileStringW(L"CustomFolders", key.c_str(), L"", buf, MAX_PATH, ini.c_str());
        if (wcslen(buf) > 0) SendMessageW(hListCustom, LB_ADDSTRING, 0, (LPARAM)buf);
    }
}

std::wstring FormatSingleSize(ULONGLONG b) {
    wchar_t buf[64];
    double mb = (double)b / (1024.0 * 1024.0);
    if (mb >= 0.01) swprintf(buf, 64, L"%.2f МБ", mb);
    else swprintf(buf, 64, L"%.1f КБ", (double)b / 1024.0);
    return buf;
}

ULONGLONG ProcessDir(const std::wstring& dir, bool del, int& cnt) {
    ULONGLONG total = 0;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        std::wstring p = dir + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            total += ProcessDir(p, del, cnt);
            if (del) { SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL); RemoveDirectoryW(p.c_str()); }
        } else {
            ULARGE_INTEGER sz = { fd.nFileSizeLow, fd.nFileSizeHigh };
            if (del) {
                SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL);
                if (DeleteFileW(p.c_str())) { total += sz.QuadPart; cnt++; }
            } else {
                total += sz.QuadPart; cnt++;
                std::wstring item = p + L"  [" + FormatSingleSize(sz.QuadPart) + L"]";
                SendMessageW(hListFiles, LB_ADDSTRING, 0, (LPARAM)item.c_str());
            }
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return total;
}

void RunCleanerAction(HWND hwnd, bool del) {
    SetWindowTextW(hStatusLabel, del ? L"Статус: Удаление..." : L"Статус: Сканирование...");
    UpdateWindow(hwnd);
    if (!del) { SendMessageW(hListFiles, LB_RESETCONTENT, 0, 0); SendMessageW(hListFiles, WM_SETREDRAW, FALSE, 0); }

    ULONGLONG bytes = 0; int files = 0; wchar_t buf[MAX_PATH];
    std::vector<std::wstring> paths;
    if (SendMessageW(hChkUserTemp, BM_GETCHECK, 0, 0) == BST_CHECKED && GetTempPathW(MAX_PATH, buf)) {
        size_t l = wcslen(buf); if (l > 0 && buf[l - 1] == L'\\') buf[l - 1] = 0;
        paths.push_back(buf);
    }
    if (SendMessageW(hChkWinTemp, BM_GETCHECK, 0, 0) == BST_CHECKED && GetWindowsDirectoryW(buf, MAX_PATH))
        paths.push_back(std::wstring(buf) + L"\\Temp");
    if (SendMessageW(hChkPrefetch, BM_GETCHECK, 0, 0) == BST_CHECKED && GetWindowsDirectoryW(buf, MAX_PATH))
        paths.push_back(std::wstring(buf) + L"\\Prefetch");

    int customCnt = (int)SendMessageW(hListCustom, LB_GETCOUNT, 0, 0);
    for (int i = 0; i < customCnt; i++) {
        SendMessageW(hListCustom, LB_GETTEXT, i, (LPARAM)buf);
        paths.push_back(buf);
    }
    for (const auto& p : paths) bytes += ProcessDir(p, del, files);

    if (SendMessageW(hChkRecycle, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        SHQUERYRBINFO rb = { sizeof(SHQUERYRBINFO) };
        if (SUCCEEDED(SHQueryRecycleBinW(NULL, &rb)) && rb.i64Size > 0) {
            bytes += (ULONGLONG)rb.i64Size; files += (int)rb.i64NumItems;
            if (!del) {
                std::wstring s = L"[Корзина] Объектов: " + std::to_wstring(rb.i64NumItems) + L" [" + FormatSingleSize(rb.i64Size) + L"]";
                SendMessageW(hListFiles, LB_ADDSTRING, 0, (LPARAM)s.c_str());
            } else SHEmptyRecycleBinW(hwnd, NULL, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
        }
    }
    if (!del) {
        SendMessageW(hListFiles, WM_SETREDRAW, TRUE, 0);
        SendMessageW(hListFiles, LB_SETHORIZONTALEXTENT, 1200, 0);
        InvalidateRect(hListFiles, NULL, TRUE);
    }
    double mb = (double)bytes / (1024.0 * 1024.0);
    wchar_t msg[256];
    if (del) {
        swprintf(msg, 256, L"Очищено: %.2f МБ | Удалено объектов: %d", mb, files);
        MessageBoxW(hwnd, msg, L"Готово", MB_OK | MB_ICONINFORMATION);
        RunCleanerAction(hwnd, false);
    } else {
        swprintf(msg, 256, L"Будет очищено: %.2f МБ (найдено объектов: %d)", mb, files);
        SetWindowTextW(hLabelTotalSize, msg);
        SetWindowTextW(hStatusLabel, L"Статус: Сканирование завершено.");
    }
}

BOOL CALLBACK SetChildFont(HWND c, LPARAM l) { SendMessageW(c, WM_SETFONT, (WPARAM)l, TRUE); return TRUE; }

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_CREATE) {
        hFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        hFontBold = CreateFontW(16, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        CreateWindowW(L"BUTTON", L"Стандартные папки для очистки", WS_VISIBLE | WS_CHILD | BS_GROUPBOX, 15, 10, 595, 120, hwnd, 0, 0, 0);
        hChkUserTemp = CreateWindowW(L"BUTTON", L"Временные файлы пользователя (%TEMP%)", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 30, 32, 550, 20, hwnd, (HMENU)ID_CHK_USERTEMP, 0, 0);
        hChkWinTemp = CreateWindowW(L"BUTTON", L"Системный кэш (C:\\Windows\\Temp)", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 30, 55, 550, 20, hwnd, (HMENU)ID_CHK_WINTEMP, 0, 0);
        hChkPrefetch = CreateWindowW(L"BUTTON", L"Файлы Prefetch (C:\\Windows\\Prefetch)", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 30, 78, 550, 20, hwnd, (HMENU)ID_CHK_PREFETCH, 0, 0);
        hChkRecycle = CreateWindowW(L"BUTTON", L"Очистить Корзину Windows", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 30, 101, 550, 20, hwnd, (HMENU)ID_CHK_RECYCLE, 0, 0);
        SendMessageW(hChkUserTemp, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(hChkWinTemp, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(hChkRecycle, BM_SETCHECK, BST_CHECKED, 0);

        CreateWindowW(L"BUTTON", L"Дополнительные папки", WS_VISIBLE | WS_CHILD | BS_GROUPBOX, 15, 138, 595, 160, hwnd, 0, 0, 0);
        hListCustom = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", 0, WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY, 30, 160, 565, 70, hwnd, (HMENU)ID_LIST_CUSTOM, 0, 0);
        hEditPath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, 30, 236, 365, 24, hwnd, (HMENU)ID_EDIT_PATH, 0, 0);
        CreateWindowW(L"BUTTON", L"Добавить путь", WS_VISIBLE | WS_CHILD, 402, 236, 100, 24, hwnd, (HMENU)ID_BTN_ADD, 0, 0);
        CreateWindowW(L"BUTTON", L"Обзор...", WS_VISIBLE | WS_CHILD, 508, 236, 87, 24, hwnd, (HMENU)ID_BTN_BROWSE, 0, 0);
        CreateWindowW(L"BUTTON", L"Удалить выбранную папку", WS_VISIBLE | WS_CHILD, 30, 266, 565, 24, hwnd, (HMENU)ID_BTN_REMOVE, 0, 0);

        hLabelTotalSize = CreateWindowW(L"STATIC", L"Подсчет...", WS_VISIBLE | WS_CHILD, 15, 310, 595, 22, hwnd, 0, 0, 0);
        CreateWindowW(L"BUTTON", L"Список файлов, которые будут очищены:", WS_VISIBLE | WS_CHILD | BS_GROUPBOX, 15, 336, 595, 215, hwnd, 0, 0, 0);
        hListFiles = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", 0, WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_HSCROLL | LBS_NOINTEGRALHEIGHT, 30, 358, 565, 180, hwnd, (HMENU)ID_LIST_FILES, 0, 0);
        CreateWindowW(L"BUTTON", L"ЗАПУСТИТЬ ОЧИСТКУ", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 15, 562, 595, 38, hwnd, (HMENU)ID_BTN_CLEAN, 0, 0);
        hStatusLabel = CreateWindowW(L"STATIC", L"Статус: Готов", WS_VISIBLE | WS_CHILD, 15, 608, 595, 20, hwnd, 0, 0, 0);

        EnumChildWindows(hwnd, SetChildFont, (LPARAM)hFont);
        SendMessageW(hLabelTotalSize, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        
        LoadCustomFolders();
        
        PostMessageW(hwnd, WM_AUTOSCAN, 0, 0);
        return 0;
    }
    if (uMsg == WM_AUTOSCAN) { RunCleanerAction(hwnd, false); return 0; }
    if (uMsg == WM_COMMAND) {
        int id = LOWORD(wParam);
        if ((id >= ID_CHK_USERTEMP && id <= ID_CHK_RECYCLE) && HIWORD(wParam) == BN_CLICKED) RunCleanerAction(hwnd, false);
        else if (id == ID_BTN_ADD) {
            wchar_t p[MAX_PATH]; GetWindowTextW(hEditPath, p, MAX_PATH);
            DWORD a = GetFileAttributesW(p);
            if (wcslen(p) > 0 && a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) {
                SendMessageW(hListCustom, LB_ADDSTRING, 0, (LPARAM)p); 
                SetWindowTextW(hEditPath, L""); 
                SaveCustomFolders();
                RunCleanerAction(hwnd, false);
            } else MessageBoxW(hwnd, L"Папка не найдена!", L"Ошибка", MB_OK | MB_ICONWARNING);
        } else if (id == ID_BTN_BROWSE) {
            BROWSEINFOW bi = { hwnd, 0, 0, L"Выберите папку:", BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE };
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t p[MAX_PATH];
                if (SHGetPathFromIDListW(pidl, p)) { 
                    SendMessageW(hListCustom, LB_ADDSTRING, 0, (LPARAM)p); 
                    SaveCustomFolders();
                    RunCleanerAction(hwnd, false); 
                }
                CoTaskMemFree(pidl);
            }
        } else if (id == ID_BTN_REMOVE) {
            int s = (int)SendMessageW(hListCustom, LB_GETCURSEL, 0, 0);
            if (s != LB_ERR) { 
                SendMessageW(hListCustom, LB_DELETESTRING, s, 0); 
                SaveCustomFolders();
                RunCleanerAction(hwnd, false); 
            }
        } else if (id == ID_BTN_CLEAN) {
            if (MessageBoxW(hwnd, L"Удалить найденные файлы?", L"Подтверждение", MB_YESNO | MB_ICONQUESTION) == IDYES) RunCleanerAction(hwnd, true);
        }
        return 0;
    }
    if (uMsg == WM_DESTROY) { DeleteObject(hFont); DeleteObject(hFontBold); PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nShow) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    WNDCLASSW wc = { 0, WindowProc, 0, 0, hInst, LoadIcon(0, IDI_APPLICATION), LoadCursor(0, IDC_ARROW), (HBRUSH)(COLOR_BTNFACE + 1), 0, L"CleanerCls" };
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, L"CleanerCls", L"CleanerApp", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 640, 675, 0, 0, hInst, 0);
    ShowWindow(hwnd, nShow); UpdateWindow(hwnd);
    MSG msg; while (GetMessageW(&msg, 0, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    CoUninitialize(); return 0;
}