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

// 📖 설정 파일 읽기 (UTF-16 LE)
void LoadConfig() {
    wchar_t configPath[MAX_PATH];
    GetModuleFileNameW(NULL, configPath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(configPath, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    wcscat_s(configPath, MAX_PATH, L"config.ini");
    
    std::wifstream file(configPath, std::ios::binary);
    if (!file.is_open()) return;
    
    // UTF-16 BOM 확인 (0xFFFE)
    unsigned short bom;
    file.read((char*)&bom, 2);
    if (bom != 0xFEFF) {
        file.close();
        // BOM 없으면 UTF-8로 시도 (예전 버전 호환)
        std::ifstream file8(configPath);
        if (!file8.is_open()) return;
        std::string line;
        std::wstring modifiers, key, mouseClick, middleClick, wheelTilt;
        while (std::getline(file8, line)) {
            if (line.empty() || line[0] == '#') continue;
            size_t eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;
            std::string keyName = line.substr(0, eqPos);
            std::string value = line.substr(eqPos + 1);
            keyName.erase(0, keyName.find_first_not_of(" \t\r\n"));
            keyName.erase(keyName.find_last_not_of(" \t\r\n") + 1);
            value.erase(0, value.find_first_not_of(" \t\r\n"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);
            
            std::wstring wKey = std::wstring(keyName.begin(), keyName.end());
            std::wstring wVal = std::wstring(value.begin(), value.end());
            if (wKey == L"Modifiers") modifiers = wVal;
            else if (wKey == L"Key") key = wVal;
            else if (wKey == L"SaveOnMouseClick") mouseClick = wVal;
            else if (wKey == L"SaveOnMiddleClick") middleClick = wVal;
            else if (wKey == L"SaveOnWheelTilt") wheelTilt = wVal;
        }
        // 파싱은 아래에서 공통으로 처리
    } else {
        // UTF-16 읽기
        std::wstring line;
        std::wstring modifiers, key, mouseClick, middleClick, wheelTilt;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == L'#') continue;
            size_t eqPos = line.find(L'=');
            if (eqPos == std::wstring::npos) continue;
            std::wstring keyName = line.substr(0, eqPos);
            std::wstring value = line.substr(eqPos + 1);
            keyName.erase(0, keyName.find_first_not_of(L" \t\r\n"));
            keyName.erase(keyName.find_last_not_of(L" \t\r\n") + 1);
            value.erase(0, value.find_first_not_of(L" \t\r\n"));
            value.erase(value.find_last_not_of(L" \t\r\n") + 1);
            
            if (keyName == L"Modifiers") modifiers = value;
            else if (keyName == L"Key") key = value;
            else if (keyName == L"SaveOnMouseClick") mouseClick = value;
            else if (keyName == L"SaveOnMiddleClick") middleClick = value;
            else if (keyName == L"SaveOnWheelTilt") wheelTilt = value;
        }
        // 파싱
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
        if (!key.empty()) {
            if (key == L"Space") g_hotkeyKey = VK_SPACE;
            else if (key == L"Tab") g_hotkeyKey = VK_TAB;
            else if (key == L"Enter") g_hotkeyKey = VK_RETURN;
            else if (key == L"Escape") g_hotkeyKey = VK_ESCAPE;
            else if (key.length() == 1 && iswalpha(key[0])) g_hotkeyKey = toupper(key[0]);
            else if (key.find(L"F") == 0 && key.length() <= 3) {
                int num = _wtoi(key.substr(1).c_str());
                if (num >= 1 && num <= 12) g_hotkeyKey = VK_F1 + num - 1;
            }
        }
        auto parseBool = [](const std::wstring& str) -> bool {
            return (str == L"Yes" || str == L"yes" || str == L"True" || str == L"true");
        };
        if (!mouseClick.empty()) g_saveOnMouseClick = parseBool(mouseClick);
        if (!middleClick.empty()) g_saveOnMiddleClick = parseBool(middleClick);
        if (!wheelTilt.empty()) g_saveOnWheelTilt = parseBool(wheelTilt);
    }
}

// 💾 설정 파일 저장 (UTF-16 LE with BOM)
void SaveConfig() {
    wchar_t configPath[MAX_PATH];
    GetModuleFileNameW(NULL, configPath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(configPath, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    wcscat_s(configPath, MAX_PATH, L"config.ini");
    
    std::wofstream file(configPath, std::ios::binary);
    if (!file.is_open()) return;
    
    // UTF-16 BOM 쓰기
    unsigned short bom = 0xFEFF;
    file.write((char*)&bom, 2);
    
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
    else if (g_hotkeyKey >= VK_F1 && g_hotkeyKey <= VK_F12) keyStr = L"F" + std::to_wstring(g_hotkeyKey - VK_F1 + 1);
    else keyStr = L"???";
    
    file << L"[Hotkey]\n";
    file << L"Modifiers=" << modStr << L"\n";
    file << L"Key=" << keyStr << L"\n\n";
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
            
            // 현재 단축키 표시 (ID 1001)
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
            SetDlgItemTextW(hDlg, 1001, text);
            SetDlgItemTextW(hDlg, 1002, L"변경");
            EnableWindow(GetDlgItem(hDlg, 1003), TRUE);
            
            // 체크박스 상태 설정 (ID 101, 102, 103)
            SendDlgItemMessageW(hDlg, 101, BM_SETCHECK, g_saveOnMouseClick ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessageW(hDlg, 102, BM_SETCHECK, g_saveOnMiddleClick ? BST_CHECKED : BST_UNCHECKED, 0);
            SendDlgItemMessageW(hDlg, 103, BM_SETCHECK, g_saveOnWheelTilt ? BST_CHECKED : BST_UNCHECKED, 0);
            return TRUE;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            
            if (id == 1002) {  // 변경 버튼
                g_isWaitingForKey = true;
                SetDlgItemTextW(hDlg, 1002, L"키 입력 중...");
                SetDlgItemTextW(hDlg, 1001, L"원하는 단축키를 눌러주세요");
                EnableWindow(GetDlgItem(hDlg, 1003), FALSE);
                SetFocus(hDlg);
            } else if (id == 1003) {  // 확인 버튼
                // 체크박스 상태 저장
                g_saveOnMouseClick = (SendDlgItemMessageW(hDlg, 101, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_saveOnMiddleClick = (SendDlgItemMessageW(hDlg, 102, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_saveOnWheelTilt = (SendDlgItemMessageW(hDlg, 103, BM_GETCHECK, 0, 0) == BST_CHECKED);
                
                SaveConfig();
                UnregisterHotKey(g_hWnd, 1);
                RegisterHotKey(g_hWnd, 1, g_hotkeyModifiers, g_hotkeyKey);
                DestroyWindow(hDlg);
                g_hConfigDlg = NULL;
            } else if (id == IDCANCEL) {  // 취소 버튼
                DestroyWindow(hDlg);
                g_hConfigDlg = NULL;
            } else if (id >= 101 && id <= 103) {
                // 체크박스 클릭 시 바로 변수 반영 (선택사항)
                // 실제 저장은 확인 버튼에서 처리
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
                
                if ((vkCode >= 'A' && vkCode <= 'Z') ||
                    (vkCode >= VK_F1 && vkCode <= VK_F12) ||
                    vkCode == VK_SPACE || vkCode == VK_TAB ||
                    vkCode == VK_RETURN || vkCode == VK_ESCAPE) {
                    
                    g_hotkeyModifiers = mods;
                    g_hotkeyKey = vkCode;
                    g_isWaitingForKey = false;
                    
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
    
    if (cmd == 1001) {
        if (g_hConfigDlg == NULL) {
            HWND hDlg = CreateWindowExW(0, L"#32770", L"설정",
                                       WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                       CW_USEDEFAULT, CW_USEDEFAULT, 450, 260,
                                       hWnd, NULL, GetModuleHandle(NULL), NULL);
            if (hDlg) {
                // 단축키 표시 (ID 1001)
                CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
                             20, 15, 410, 25, hDlg, (HMENU)1001, NULL, NULL);
                
                // 변경 버튼 (ID 1002) - 중앙 정렬
                CreateWindowW(L"BUTTON", L"변경", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                             175, 50, 100, 28, hDlg, (HMENU)1002, NULL, NULL);
                
                // 구분선
                CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
                             20, 95, 410, 2, hDlg, NULL, NULL, NULL);
                
                // 마우스 설정 레이블
                CreateWindowW(L"STATIC", L"🖱️ 마우스 입력 저장 활성화", WS_CHILD | WS_VISIBLE | SS_CENTER,
                             20, 108, 410, 20, hDlg, NULL, NULL, NULL);
                
                // 체크박스: 왼쪽 클릭 (ID 101)
                CreateWindowW(L"BUTTON", L"왼쪽 클릭", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                             40, 138, 110, 22, hDlg, (HMENU)101, NULL, NULL);
                
                // 체크박스: 가운데 클릭 (ID 102)
                CreateWindowW(L"BUTTON", L"가운데 클릭 (휠)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                             170, 138, 130, 22, hDlg, (HMENU)102, NULL, NULL);
                
                // 체크박스: 휠 틸트 (ID 103)
                CreateWindowW(L"BUTTON", L"휠 틸트 (좌우)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                             310, 138, 110, 22, hDlg, (HMENU)103, NULL, NULL);
                
                // 확인/취소 버튼 (하단 중앙 정렬)
                CreateWindowW(L"BUTTON", L"확인", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                             130, 185, 80, 30, hDlg, (HMENU)1003, NULL, NULL);
                CreateWindowW(L"BUTTON", L"취소", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                             240, 185, 80, 30, hDlg, (HMENU)IDCANCEL, NULL, NULL);
                
                // 하단 설명
                CreateWindowW(L"STATIC", L"※ 확인 버튼을 눌러야 설정이 저장됩니다", 
                             WS_CHILD | WS_VISIBLE | SS_CENTER,
                             20, 230, 410, 18, hDlg, NULL, NULL, NULL);
                
                SetWindowLongPtrW(hDlg, GWLP_WNDPROC, (LONG_PTR)ConfigDlgProc);
                SendMessageW(hDlg, WM_INITDIALOG, 0, 0);
                ShowWindow(hDlg, SW_SHOW);
                UpdateWindow(hDlg);
            }
        } else {
            SetForegroundWindow(g_hConfigDlg);
        }
    } else if (cmd == 1002) {
        if (g_hMouseHook) {
            UnhookWindowsHookEx(g_hMouseHook);
            g_hMouseHook = NULL;
        }
        PostQuitMessage(0);
    }
}

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
