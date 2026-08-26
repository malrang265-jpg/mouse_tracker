#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <string>
#include <fstream>
#include <sstream>
#include <cctype>
#include <vector>

// 전역 변수
HWND g_hWnd = NULL;
HHOOK g_hMouseHook = NULL;
int g_clickCount = 1;

// 설정 값
int g_hotkeyModifiers = MOD_CONTROL | MOD_SHIFT;
int g_hotkeyKey = 'S';
bool g_saveOnMouseClick = true;
bool g_saveOnMiddleClick = true;
bool g_saveOnWheelTilt = true;

// 설정 다이얼로그 관련
HWND g_hConfigDlg = NULL;
bool g_isWaitingForKey = false;

// 📖 설정 파일 읽기
void LoadConfig() {
    wchar_t configPath[MAX_PATH];
    GetModuleFileNameW(NULL, configPath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(configPath, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    wcscat_s(configPath, MAX_PATH, L"config.ini");
    
    std::wifstream file(configPath);
    if (!file.is_open()) return;
    
    std::wstring line;
    std::wstring modifiers, key, mouseClick, middleClick, wheelTilt;
    
    while (std::getline(file, line)) {
        size_t start = line.find_first_not_of(L" \t\r\n");
        if (start == std::wstring::npos) continue;
        line = line.substr(start);
        if (line.empty() || line[0] == L'#') continue;
        
        size_t eqPos = line.find(L'=');
        if (eqPos == std::wstring::npos) continue;
        
        std::wstring keyName = line.substr(0, eqPos);
        std::wstring value = line.substr(eqPos + 1);
        keyName.erase(0, keyName.find_first_not_of(L" \t"));
        keyName.erase(keyName.find_last_not_of(L" \t") + 1);
        value.erase(0, value.find_first_not_of(L" \t"));
        value.erase(value.find_last_not_of(L" \t") + 1);
        
        if (keyName == L"Modifiers") modifiers = value;
        else if (keyName == L"Key") key = value;
        else if (keyName == L"SaveOnMouseClick") mouseClick = value;
        else if (keyName == L"SaveOnMiddleClick") middleClick = value;
        else if (keyName == L"SaveOnWheelTilt") wheelTilt = value;
    }
    
    // Modifiers 파싱
    if (!modifiers.empty()) {
        int mod = 0;
        std::wstringstream ss(modifiers);
        std::wstring token;
        while (std::getline(ss, token, L'+')) {
            token.erase(0, token.find_first_not_of(L" \t"));
            token.erase(token.find_last_not_of(L" \t") + 1);
            if (token == L"Ctrl") mod |= MOD_CONTROL;
            else if (token == L"Shift") mod |= MOD_SHIFT;
            else if (token == L"Alt") mod |= MOD_ALT;
            else if (token == L"Win") mod |= MOD_WIN;
        }
        if (mod != 0) g_hotkeyModifiers = mod;
    }
    
    // Key 파싱
    if (!key.empty()) {
        if (key == L"Space") g_hotkeyKey = VK_SPACE;
        else if (key == L"Tab") g_hotkeyKey = VK_TAB;
        else if (key == L"Enter") g_hotkeyKey = VK_RETURN;
        else if (key == L"Escape") g_hotkeyKey = VK_ESCAPE;
        else if (key.length() == 1 && iswalpha(key[0])) {
            g_hotkeyKey = toupper(key[0]);
        } else if (key.find(L"F") == 0 && key.length() <= 3) {
            int num = _wtoi(key.substr(1).c_str());
            if (num >= 1 && num <= 12) g_hotkeyKey = VK_F1 + num - 1;
        }
    }
    
    // 불리언 값 파싱
    auto parseBool = [](const std::wstring& str) -> bool {
        return (str == L"Yes" || str == L"yes" || str == L"True" || str == L"true");
    };
    if (!mouseClick.empty()) g_saveOnMouseClick = parseBool(mouseClick);
    if (!middleClick.empty()) g_saveOnMiddleClick = parseBool(middleClick);
    if (!wheelTilt.empty()) g_saveOnWheelTilt = parseBool(wheelTilt);
}

// 💾 설정 파일 저장
void SaveConfig() {
    wchar_t configPath[MAX_PATH];
    GetModuleFileNameW(NULL, configPath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(configPath, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    wcscat_s(configPath, MAX_PATH, L"config.ini");
    
    std::wofstream file(configPath);
    if (!file.is_open()) return;
    
    // Modifier 문자열 생성
    std::wstring modStr;
    if (g_hotkeyModifiers & MOD_CONTROL) modStr += L"Ctrl+";
    if (g_hotkeyModifiers & MOD_SHIFT) modStr += L"Shift+";
    if (g_hotkeyModifiers & MOD_ALT) modStr += L"Alt+";
    if (g_hotkeyModifiers & MOD_WIN) modStr += L"Win+";
    if (!modStr.empty()) modStr.pop_back();  // 마지막 '+' 제거
    
    // Key 문자열 생성
    std::wstring keyStr;
    if (g_hotkeyKey >= 'A' && g_hotkeyKey <= 'Z') {
        keyStr = (wchar_t)g_hotkeyKey;
    } else if (g_hotkeyKey == VK_SPACE) keyStr = L"Space";
    else if (g_hotkeyKey == VK_TAB) keyStr = L"Tab";
    else if (g_hotkeyKey == VK_RETURN) keyStr = L"Enter";
    else if (g_hotkeyKey == VK_ESCAPE) keyStr = L"Escape";
    else if (g_hotkeyKey >= VK_F1 && g_hotkeyKey <= VK_F12) {
        keyStr = L"F" + std::to_wstring(g_hotkeyKey - VK_F1 + 1);
    }
    
    file << L"[Hotkey]\n";
    file << L"Modifiers=" << modStr << L"\n";
    file << L"Key=" << keyStr << L"\n";
    file << L"\n";
    file << L"# 마우스 입력 설정 (Yes/No)\n";
    file << L"SaveOnMouseClick=" << (g_saveOnMouseClick ? L"Yes" : L"No") << L"\n";
    file << L"SaveOnMiddleClick=" << (g_saveOnMiddleClick ? L"Yes" : L"No") << L"\n";
    file << L"SaveOnWheelTilt=" << (g_saveOnWheelTilt ? L"Yes" : L"No") << L"\n";
    file.close();
}

// 📝 좌표 저장 함수
void SaveClickPosition(int x, int y) {
    wchar_t filePath[MAX_PATH];
    GetModuleFileNameW(NULL, filePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(filePath, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    wcscat_s(filePath, MAX_PATH, L"click_coordinates.txt");
    
    HANDLE hFile = CreateFileW(filePath, FILE_APPEND_DATA, FILE_SHARE_READ,
                               NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        SetWindowTextW(g_hWnd, L"❌ 파일 오류!");
        SetTimer(g_hWnd, 2, 1500, NULL);
        return;
    }
    
    wchar_t buffer[256];
    wsprintfW(buffer, L"point%d=%d,%d\r\n", g_clickCount++, x, y);
    DWORD bytesWritten;
    WriteFile(hFile, buffer, wcslen(buffer) * sizeof(wchar_t), &bytesWritten, NULL);
    CloseHandle(hFile);
    
    SetWindowTextW(g_hWnd, L"✅ 저장됨!");
    SetTimer(g_hWnd, 2, 1000, NULL);
}

// 🖱️ 마우스 훅 프로시저
LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        MSLLHOOKSTRUCT* pStruct = (MSLLHOOKSTRUCT*)lParam;
        POINT pt = pStruct->pt;
        
        if (wParam == WM_LBUTTONDOWN && g_saveOnMouseClick) {
            SaveClickPosition(pt.x, pt.y);
        } else if (wParam == WM_MBUTTONDOWN && g_saveOnMiddleClick) {
            SaveClickPosition(pt.x, pt.y);
        } else if (wParam == WM_MOUSEHWHEEL && g_saveOnWheelTilt) {
            // 휠 틸트(좌우 스크롤) 발생 시 좌표 저장
            SaveClickPosition(pt.x, pt.y);
        }
    }
    return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
}

// ⚙️ 설정 다이얼로그 프로시저
INT_PTR CALLBACK ConfigDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG: {
            g_hConfigDlg = hDlg;
            g_isWaitingForKey = false;
            
            // 현재 단축키 표시
            wchar_t text[256];
            std::wstring modStr;
            if (g_hotkeyModifiers & MOD_CONTROL) modStr += L"Ctrl+";
            if (g_hotkeyModifiers & MOD_SHIFT) modStr += L"Shift+";
            if (g_hotkeyModifiers & MOD_ALT) modStr += L"Alt+";
            if (g_hotkeyModifiers & MOD_WIN) modStr += L"Win+";
            if (!modStr.empty()) modStr.pop_back();
            
            std::wstring keyStr;
            if (g_hotkeyKey >= 'A' && g_hotkeyKey <= 'Z') keyStr = (wchar_t)g_hotkeyKey;
            else if (g_hotkeyKey == VK_SPACE) keyStr = L"Space";
            else if (g_hotkeyKey == VK_TAB) keyStr = L"Tab";
            else if (g_hotkeyKey == VK_RETURN) keyStr = L"Enter";
            else if (g_hotkeyKey == VK_ESCAPE) keyStr = L"Escape";
            else if (g_hotkeyKey >= VK_F1 && g_hotkeyKey <= VK_F12)
                keyStr = L"F" + std::to_wstring(g_hotkeyKey - VK_F1 + 1);
            else keyStr = L"???";
            
            wsprintfW(text, L"현재 단축키: %s + %s", modStr.c_str(), keyStr.c_str());
            SetDlgItemTextW(hDlg, 1001, text);  // ID 1001: 현재 단축키 표시 레이블
            
            SetDlgItemTextW(hDlg, 1002, L"변경할 단축키를 누르세요...");
            EnableWindow(GetDlgItem(hDlg, 1003), FALSE);  // ID 1003: 확인 버튼 비활성화
            return TRUE;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == 1002) {  // 변경 버튼 클릭
                g_isWaitingForKey = true;
                SetDlgItemTextW(hDlg, 1002, L"키를 입력하세요...");
                SetDlgItemTextW(hDlg, 1001, L"단축키를 누르면 자동 등록됩니다");
                EnableWindow(GetDlgItem(hDlg, 1003), FALSE);
                SetFocus(hDlg);
            } else if (LOWORD(wParam) == 1003) {  // 확인 버튼
                // 현재 설정 저장
                SaveConfig();
                // 단축키 재등록
                UnregisterHotKey(g_hWnd, 1);
                RegisterHotKey(g_hWnd, 1, g_hotkeyModifiers, g_hotkeyKey);
                DestroyWindow(hDlg);
                g_hConfigDlg = NULL;
            } else if (LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hDlg);
                g_hConfigDlg = NULL;
            }
            break;
        }
        case WM_KEYDOWN: {
            if (g_isWaitingForKey) {
                // 입력된 키 저장
                int vkCode = (int)wParam;
                int mods = 0;
                if (GetKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
                if (GetKeyState(VK_SHIFT) & 0x8000) mods |= MOD_SHIFT;
                if (GetKeyState(VK_MENU) & 0x8000) mods |= MOD_ALT;
                if (GetKeyState(VK_LWIN) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000) mods |= MOD_WIN;
                
                // 유효한 키인지 확인
                if ((vkCode >= 'A' && vkCode <= 'Z') ||
                    (vkCode >= VK_F1 && vkCode <= VK_F12) ||
                    vkCode == VK_SPACE || vkCode == VK_TAB ||
                    vkCode == VK_RETURN || vkCode == VK_ESCAPE) {
                    
                    g_hotkeyModifiers = mods;
                    g_hotkeyKey = vkCode;
                    g_isWaitingForKey = false;
                    
                    // UI 업데이트
                    std::wstring modStr;
                    if (g_hotkeyModifiers & MOD_CONTROL) modStr += L"Ctrl+";
                    if (g_hotkeyModifiers & MOD_SHIFT) modStr += L"Shift+";
                    if (g_hotkeyModifiers & MOD_ALT) modStr += L"Alt+";
                    if (g_hotkeyModifiers & MOD_WIN) modStr += L"Win+";
                    if (!modStr.empty()) modStr.pop_back();
                    
                    std::wstring keyStr;
                    if (vkCode >= 'A' && vkCode <= 'Z') keyStr = (wchar_t)vkCode;
                    else if (vkCode == VK_SPACE) keyStr = L"Space";
                    else if (vkCode == VK_TAB) keyStr = L"Tab";
                    else if (vkCode == VK_RETURN) keyStr = L"Enter";
                    else if (vkCode == VK_ESCAPE) keyStr = L"Escape";
                    else if (vkCode >= VK_F1 && vkCode <= VK_F12)
                        keyStr = L"F" + std::to_wstring(vkCode - VK_F1 + 1);
                    
                    wchar_t text[256];
                    wsprintfW(text, L"설정 완료: %s + %s", modStr.c_str(), keyStr.c_str());
                    SetDlgItemTextW(hDlg, 1001, text);
                    SetDlgItemTextW(hDlg, 1002, L"변경");
                    EnableWindow(GetDlgItem(hDlg, 1003), TRUE);
                }
                return TRUE;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hDlg);
            g_hConfigDlg = NULL;
            break;
    }
    return FALSE;
}

// 📋 컨텍스트 메뉴 표시
void ShowContextMenu(HWND hWnd, int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, 1001, L"⚙️ 설정");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, 1002, L"🚪 종료");
    
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
                             x, y, 0, hWnd, NULL);
    DestroyMenu(hMenu);
    
    if (cmd == 1001) {  // 설정
        if (g_hConfigDlg == NULL) {
            // 간단한 다이얼로그 생성 (윈도우 클래스 등록)
            HWND hDlg = CreateWindowExW(0, L"#32770", L"단축키 설정",
                                       WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                       CW_USEDEFAULT, CW_USEDEFAULT, 350, 150,
                                       hWnd, NULL, GetModuleHandle(NULL), NULL);
            if (hDlg) {
                // 다이얼로그에 컨트롤 추가 (Static, Button)
                CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
                             20, 20, 300, 25, hDlg, (HMENU)1001, NULL, NULL);
                CreateWindowW(L"BUTTON", L"변경", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                             30, 60, 80, 30, hDlg, (HMENU)1002, NULL, NULL);
                CreateWindowW(L"BUTTON", L"확인", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                             130, 60, 80, 30, hDlg, (HMENU)1003, NULL, NULL);
                CreateWindowW(L"BUTTON", L"취소", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                             230, 60, 80, 30, hDlg, (HMENU)IDCANCEL, NULL, NULL);
                
                // 윈도우 프로시저 설정
                SetWindowLongPtrW(hDlg, GWLP_WNDPROC, (LONG_PTR)ConfigDlgProc);
                // WM_INITDIALOG 대신 WM_CREATE에서 초기화 처리
                SendMessageW(hDlg, WM_INITDIALOG, 0, 0);
                ShowWindow(hDlg, SW_SHOW);
                UpdateWindow(hDlg);
            }
        } else {
            SetForegroundWindow(g_hConfigDlg);
        }
    } else if (cmd == 1002) {  // 종료
        if (g_hMouseHook) {
            UnhookWindowsHookEx(g_hMouseHook);
            g_hMouseHook = NULL;
        }
        PostQuitMessage(0);
    }
}

// 📋 메인 윈도우 프로시저
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rect;
            GetClientRect(hWnd, &rect);
            FillRect(hdc, &rect, (HBRUSH)(COLOR_WINDOW + 1));
            
            POINT pt;
            GetCursorPos(&pt);
            wchar_t buffer[100];
            wsprintfW(buffer, L"X: %d  Y: %d", pt.x, pt.y);
            
            SetBkMode(hdc, TRANSPARENT);
            HFONT hFont = CreateFontW(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                     CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                     DEFAULT_PITCH | FF_DONTCARE, L"맑은 고딕");
            SelectObject(hdc, hFont);
            DrawTextW(hdc, buffer, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DeleteObject(hFont);
            EndPaint(hWnd, &ps);
            break;
        }
        case WM_TIMER: {
            if (wParam == 1) {
                InvalidateRect(hWnd, NULL, TRUE);
            } else if (wParam == 2) {
                SetWindowTextW(hWnd, L"마우스 좌표 트래커");
                KillTimer(hWnd, 2);
            }
            break;
        }
        case WM_HOTKEY: {
            POINT pt;
            GetCursorPos(&pt);
            SaveClickPosition(pt.x, pt.y);
            break;
        }
        case WM_CONTEXTMENU: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            ShowContextMenu(hWnd, x, y);
            break;
        }
        case WM_DESTROY:
            if (g_hMouseHook) {
                UnhookWindowsHookEx(g_hMouseHook);
                g_hMouseHook = NULL;
            }
            UnregisterHotKey(hWnd, 1);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 🚀 프로그램 진입점
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    LoadConfig();
    
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MouseTrackerClass";
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    if (!wc.hIcon) wc.hIcon = LoadIconW(NULL, IDI_INFORMATION);
    RegisterClassW(&wc);

    g_hWnd = CreateWindowW(L"MouseTrackerClass", L"마우스 좌표 트래커",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 120,
        NULL, NULL, hInstance, NULL);

    if (wc.hIcon) {
        SendMessageW(g_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)wc.hIcon);
        SendMessageW(g_hWnd, WM_SETICON, ICON_BIG, (LPARAM)wc.hIcon);
    }

    SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // 마우스 훅 설치
    g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, hInstance, 0);
    if (!g_hMouseHook) {
        SetWindowTextW(g_hWnd, L"❌ 마우스 훅 실패");
        SetTimer(g_hWnd, 2, 1500, NULL);
    }

    // 단축키 등록
    RegisterHotKey(g_hWnd, 1, g_hotkeyModifiers, g_hotkeyKey);
    SetTimer(g_hWnd, 1, 50, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
