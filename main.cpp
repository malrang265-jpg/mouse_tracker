#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <string>
#include <fstream>
#include <sstream>
#include <cctype>

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

// 설정 다이얼로그 컨트롤 ID
#define IDC_STATIC_HOTKEY    1001
#define IDC_BTN_CHANGE       1002
#define IDC_BTN_OK           1003
#define IDC_BTN_CANCEL       1004
#define IDC_CHK_LEFT         101
#define IDC_CHK_MIDDLE       102
#define IDC_CHK_WHEEL        103

// 📖 설정 파일 읽기
void LoadConfig() {
    wchar_t configPath[MAX_PATH];
    GetModuleFileNameW(NULL, configPath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(configPath, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    wcscat_s(configPath, MAX_PATH, L"config.ini");
    
    std::ifstream file(configPath);
    if (!file.is_open()) return;
    
    std::string line;
    std::string modifiers, key, mouseClick, middleClick, wheelTilt;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;
        
        std::string keyName = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);
        keyName.erase(0, keyName.find_first_not_of(" \t\r\n"));
        keyName.erase(keyName.find_last_not_of(" \t\r\n") + 1);
        value.erase(0, value.find_first_not_of(" \t\r\n"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);
        
        if (keyName == "Modifiers") modifiers = value;
        else if (keyName == "Key") key = value;
        else if (keyName == "SaveOnMouseClick") mouseClick = value;
        else if (keyName == "SaveOnMiddleClick") middleClick = value;
        else if (keyName == "SaveOnWheelTilt") wheelTilt = value;
    }
    
    if (!modifiers.empty()) {
        int mod = 0;
        std::istringstream ss(modifiers);
        std::string token;
        while (std::getline(ss, token, '+')) {
            token.erase(0, token.find_first_not_of(" \t"));
            token.erase(token.find_last_not_of(" \t") + 1);
            if (token == "Ctrl") mod |= MOD_CONTROL;
            else if (token == "Shift") mod |= MOD_SHIFT;
            else if (token == "Alt") mod |= MOD_ALT;
            else if (token == "Win") mod |= MOD_WIN;
        }
        if (mod != 0) g_hotkeyModifiers = mod;
    }
    
    if (!key.empty()) {
        if (key == "Space") g_hotkeyKey = VK_SPACE;
        else if (key == "Tab") g_hotkeyKey = VK_TAB;
        else if (key == "Enter") g_hotkeyKey = VK_RETURN;
        else if (key == "Escape") g_hotkeyKey = VK_ESCAPE;
        else if (key.length() == 1 && isalpha(key[0])) g_hotkeyKey = toupper(key[0]);
        else if (key.find("F") == 0 && key.length() <= 3) {
            int num = std::stoi(key.substr(1));
            if (num >= 1 && num <= 12) g_hotkeyKey = VK_F1 + num - 1;
        }
    }
    
    auto parseBool = [](const std::string& str) -> bool {
        return (str == "Yes" || str == "yes" || str == "True" || str == "true");
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
    
    std::ofstream file(configPath);
    if (!file.is_open()) return;
    
    std::string modStr;
    if (g_hotkeyModifiers & MOD_CONTROL) modStr += "Ctrl+";
    if (g_hotkeyModifiers & MOD_SHIFT) modStr += "Shift+";
    if (g_hotkeyModifiers & MOD_ALT) modStr += "Alt+";
    if (g_hotkeyModifiers & MOD_WIN) modStr += "Win+";
    if (!modStr.empty()) modStr.pop_back();
    
    std::string keyStr;
    if (g_hotkeyKey >= 'A' && g_hotkeyKey <= 'Z') keyStr = (char)g_hotkeyKey;
    else if (g_hotkeyKey == VK_SPACE) keyStr = "Space";
    else if (g_hotkeyKey == VK_TAB) keyStr = "Tab";
    else if (g_hotkeyKey == VK_RETURN) keyStr = "Enter";
    else if (g_hotkeyKey == VK_ESCAPE) keyStr = "Escape";
    else if (g_hotkeyKey >= VK_F1 && g_hotkeyKey <= VK_F12) keyStr = "F" + std::to_string(g_hotkeyKey - VK_F1 + 1);
    else keyStr = "???";
    
    file << "[Hotkey]\n";
    file << "Modifiers=" << modStr << "\n";
    file << "Key=" << keyStr << "\n\n";
    file << "# 마우스 입력 설정 (Yes/No)\n";
    file << "SaveOnMouseClick=" << (g_saveOnMouseClick ? "Yes" : "No") << "\n";
    file << "SaveOnMiddleClick=" << (g_saveOnMiddleClick ? "Yes" : "No") << "\n";
    file << "SaveOnWheelTilt=" << (g_saveOnWheelTilt ? "Yes" : "No") << "\n";
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
            
            // 단축키 표시 업데이트
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
            SetDlgItemTextW(hDlg, IDC_STATIC_HOTKEY, text);
            
            // 체크박스 상태 설정
            SendDlgItemMessageW(hDlg, IDC_CHK_LEFT, BM_SETCHECK, g_saveOnMouseClick ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessageW(hDlg, IDC_CHK_MIDDLE, BM_SETCHECK, g_saveOnMiddleClick ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessageW(hDlg, IDC_CHK_WHEEL, BM_SETCHECK, g_saveOnWheelTilt ? BST_CHECKED : BST_UNCHECKED, 0);
            
            return TRUE;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);
            
            if (id == IDC_BTN_CHANGE) {
                g_isWaitingForKey = true;
                SetDlgItemTextW(hDlg, IDC_BTN_CHANGE, L"키 입력 중...");
                SetDlgItemTextW(hDlg, IDC_STATIC_HOTKEY, L"원하는 단축키를 눌러주세요");
                EnableWindow(GetDlgItem(hDlg, IDC_BTN_OK), FALSE);
                SetFocus(hDlg);
            } else if (id == IDC_BTN_OK) {
                // 체크박스 상태 저장
                g_saveOnMouseClick = (SendDlgItemMessageW(hDlg, IDC_CHK_LEFT, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_saveOnMiddleClick = (SendDlgItemMessageW(hDlg, IDC_CHK_MIDDLE, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_saveOnWheelTilt = (SendDlgItemMessageW(hDlg, IDC_CHK_WHEEL, BM_GETCHECK, 0, 0) == BST_CHECKED);
                
                SaveConfig();
                UnregisterHotKey(g_hWnd, 1);
                RegisterHotKey(g_hWnd, 1, g_hotkeyModifiers, g_hotkeyKey);
                DestroyWindow(hDlg);
                g_hConfigDlg = NULL;
            } else if (id == IDC_BTN_CANCEL) {
                DestroyWindow(hDlg);
                g_hConfigDlg = NULL;
            } else if (id >= IDC_CHK_LEFT && id <= IDC_CHK_WHEEL) {
                // 체크박스 클릭 시 즉시 반영 (UI 피드백)
                // 실제 저장은 OK 버튼에서 처리
                if (id == IDC_CHK_LEFT) {
                    g_saveOnMouseClick = (SendDlgItemMessageW(hDlg, IDC_CHK_LEFT, BM_GETCHECK, 0, 0) == BST_CHECKED);
                } else if (id == IDC_CHK_MIDDLE) {
                    g_saveOnMiddleClick = (SendDlgItemMessageW(hDlg, IDC_CHK_MIDDLE, BM_GETCHECK, 0, 0) == BST_CHECKED);
                } else if (id == IDC_CHK_WHEEL) {
                    g_saveOnWheelTilt = (SendDlgItemMessageW(hDlg, IDC_CHK_WHEEL, BM_GETCHECK, 0, 0) == BST_CHECKED);
                }
                // 제목 표시줄에 실시간 상태 표시 (선택사항)
            }
            break;
        }
        case WM_KEYDOWN: {
            if (g_isWaitingForKey) {
                int vkCode = (int)wParam;
                int mods = 0;
                if (GetKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
                if (GetKeyState(VK_SHIFT) & 0x8000) mods |= MOD_SHIFT;
                if (GetKeyState(VK_MENU) & 0x8000) mods |= MOD_ALT;
                if (GetKeyState(VK_LWIN) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000) mods |= MOD_WIN;
                
                // 유효한 단축키만 허용
                if ((vkCode >= 'A' && vkCode <= 'Z') ||
                    (vkCode >= VK_F1 && vkCode <= VK_F12) ||
                    vkCode == VK_SPACE || vkCode == VK_TAB ||
                    vkCode == VK_RETURN || vkCode == VK_ESCAPE) {
                    
                    g_hotkeyModifiers = mods;
                    g_hotkeyKey = vkCode;
                    g_isWaitingForKey = false;
                    
                    // 표시 업데이트
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
                    SetDlgItemTextW(hDlg, IDC_STATIC_HOTKEY, text);
                    SetDlgItemTextW(hDlg, IDC_BTN_CHANGE, L"변경");
                    EnableWindow(GetDlgItem(hDlg, IDC_BTN_OK), TRUE);
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

// 📋 설정 다이얼로그 생성
void ShowConfigDialog(HWND hWnd) {
    if (g_hConfigDlg != NULL) {
        SetForegroundWindow(g_hConfigDlg);
        return;
    }
    
    // 다이얼로그 생성
    HWND hDlg = CreateWindowExW(
        0, L"#32770", L"설정",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 260,
        hWnd, NULL, GetModuleHandle(NULL), NULL
    );
    if (!hDlg) return;
    
    // --- 컨트롤 생성 ---
    // 1. 단축키 표시 (IDC_STATIC_HOTKEY, 1001)
    CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
                  20, 18, 380, 25, hDlg, (HMENU)IDC_STATIC_HOTKEY, NULL, NULL);
    
    // 2. 변경 버튼 (IDC_BTN_CHANGE, 1002)
    CreateWindowW(L"BUTTON", L"변경", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  160, 52, 100, 28, hDlg, (HMENU)IDC_BTN_CHANGE, NULL, NULL);
    
    // 3. 구분선
    CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
                  20, 95, 380, 2, hDlg, NULL, NULL, NULL);
    
    // 4. 마우스 설정 레이블
    CreateWindowW(L"STATIC", L"🖱️ 마우스 입력 저장", WS_CHILD | WS_VISIBLE | SS_CENTER,
                  20, 108, 380, 20, hDlg, NULL, NULL, NULL);
    
    // 5. 왼쪽 클릭 체크박스 (IDC_CHK_LEFT, 101)
    CreateWindowW(L"BUTTON", L"왼쪽 클릭", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                  40, 138, 100, 22, hDlg, (HMENU)IDC_CHK_LEFT, NULL, NULL);
    
    // 6. 가운데 클릭 체크박스 (IDC_CHK_MIDDLE, 102)
    CreateWindowW(L"BUTTON", L"가운데 클릭 (휠)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                  160, 138, 120, 22, hDlg, (HMENU)IDC_CHK_MIDDLE, NULL, NULL);
    
    // 7. 휠 틸트 체크박스 (IDC_CHK_WHEEL, 103)
    CreateWindowW(L"BUTTON", L"휠 틸트 (좌우)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                  300, 138, 100, 22, hDlg, (HMENU)IDC_CHK_WHEEL, NULL, NULL);
    
    // 8. 확인 버튼 (IDC_BTN_OK, 1003)
    CreateWindowW(L"BUTTON", L"확인", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  110, 185, 80, 30, hDlg, (HMENU)IDC_BTN_OK, NULL, NULL);
    
    // 9. 취소 버튼 (IDC_BTN_CANCEL, 1004)
    CreateWindowW(L"BUTTON", L"취소", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  220, 185, 80, 30, hDlg, (HMENU)IDC_BTN_CANCEL, NULL, NULL);
    
    // 10. 하단 설명
    CreateWindowW(L"STATIC", L"※ 확인 버튼을 눌러야 설정이 저장됩니다",
                  WS_CHILD | WS_VISIBLE | SS_CENTER,
                  20, 228, 380, 18, hDlg, NULL, NULL, NULL);
    
    // 다이얼로그 프로시저 설정
    SetWindowLongPtrW(hDlg, GWLP_WNDPROC, (LONG_PTR)ConfigDlgProc);
    
    // WM_INITDIALOG 전송
    SendMessageW(hDlg, WM_INITDIALOG, 0, 0);
    
    // 화면에 표시
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
}

// 📋 컨텍스트 메뉴
void ShowContextMenu(HWND hWnd, int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, 1001, L"⚙️ 설정");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, 1002, L"🚪 종료");
    
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
                             x, y, 0, hWnd, NULL);
    DestroyMenu(hMenu);
    
    if (cmd == 1001) {
        ShowConfigDialog(hWnd);
    } else if (cmd == 1002) {
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
    if (!wc.hIcon) wc.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_INFORMATION);
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

    g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, hInstance, 0);
    if (!g_hMouseHook) {
        SetWindowTextW(g_hWnd, L"❌ 마우스 훅 실패");
        SetTimer(g_hWnd, 2, 1500, NULL);
    }

    RegisterHotKey(g_hWnd, 1, g_hotkeyModifiers, g_hotkeyKey);
    SetTimer(g_hWnd, 1, 50, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
