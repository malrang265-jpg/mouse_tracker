#include <windows.h>
#include <windowsx.h>
#include <string>
#include <fstream>
#include <sstream>
#include <cctype>

#pragma comment(lib, "user32.lib")

// ============================================================
// 전역 상태
// ============================================================
HWND g_hWnd = NULL;
HWND g_hConfigDlg = NULL;
HHOOK g_hMouseHook = NULL;
HHOOK g_hKeyboardHook = NULL;

int g_clickCount = 1;

// 단축키:
// 0 = 키보드, 1 = 마우스 왼쪽, 2 = 마우스 가운데, 3 = 마우스 오른쪽,
// 4 = 마우스 뒤로(XBUTTON1), 5 = 마우스 앞으로(XBUTTON2)
int g_hotkeyType = 0;
int g_hotkeyModifiers = MOD_CONTROL | MOD_SHIFT;
UINT g_hotkeyKey = 'S';
bool g_saveOnMouseClick = true;
bool g_saveOnMiddleClick = true;
bool g_saveOnWheelTilt = true;

bool g_isWaitingForKey = false;

// 설정창 컨트롤 ID
#define IDC_STATIC_HOTKEY  1001
#define IDC_BTN_CHANGE     1002
#define IDC_BTN_OK         1003
#define IDC_BTN_CANCEL     1004
#define IDC_CHK_LEFT       101
#define IDC_CHK_MIDDLE     102
#define IDC_CHK_WHEEL      103

// ============================================================
// 단축키 문자열
// ============================================================
std::wstring ModifierString(int mods)
{
    std::wstring s;
    if (mods & MOD_CONTROL) s += L"Ctrl+";
    if (mods & MOD_SHIFT)   s += L"Shift+";
    if (mods & MOD_ALT)     s += L"Alt+";
    if (mods & MOD_WIN)     s += L"Win+";
    if (!s.empty()) s.pop_back();
    return s;
}

std::wstring HotkeyString()
{
    if (g_hotkeyType == 1) return L"마우스 왼쪽 버튼";
    if (g_hotkeyType == 2) return L"마우스 가운데 버튼";
    if (g_hotkeyType == 3) return L"마우스 오른쪽 버튼";
    if (g_hotkeyType == 4) return L"마우스 뒤로 버튼";
    if (g_hotkeyType == 5) return L"마우스 앞으로 버튼";

    std::wstring s = ModifierString(g_hotkeyModifiers);
    if (!s.empty()) s += L"+";

    if (g_hotkeyKey >= 'A' && g_hotkeyKey <= 'Z')
        s += (wchar_t)g_hotkeyKey;
    else if (g_hotkeyKey >= '0' && g_hotkeyKey <= '9')
        s += (wchar_t)g_hotkeyKey;
    else if (g_hotkeyKey >= VK_F1 && g_hotkeyKey <= VK_F24)
        s += L"F" + std::to_wstring(g_hotkeyKey - VK_F1 + 1);
    else if (g_hotkeyKey == VK_SPACE)   s += L"Space";
    else if (g_hotkeyKey == VK_TAB)     s += L"Tab";
    else if (g_hotkeyKey == VK_RETURN)  s += L"Enter";
    else if (g_hotkeyKey == VK_ESCAPE)  s += L"Escape";
    else if (g_hotkeyKey == VK_BACK)    s += L"Backspace";
    else if (g_hotkeyKey == VK_DELETE)  s += L"Delete";
    else if (g_hotkeyKey == VK_INSERT)  s += L"Insert";
    else if (g_hotkeyKey == VK_HOME)    s += L"Home";
    else if (g_hotkeyKey == VK_END)     s += L"End";
    else if (g_hotkeyKey == VK_PRIOR)   s += L"PageUp";
    else if (g_hotkeyKey == VK_NEXT)    s += L"PageDown";
    else s += L"???";

    return s;
}

void UpdateHotkeyDisplay(HWND hDlg, const wchar_t* prefix = L"현재 단축키: ")
{
    std::wstring text = prefix;
    text += HotkeyString();
    SetDlgItemTextW(hDlg, IDC_STATIC_HOTKEY, text.c_str());
}

// ============================================================
// 설정 파일
// ============================================================
std::wstring GetConfigPath()
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(NULL, path, MAX_PATH);

    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) *(slash + 1) = L'\0';

    wcscat_s(path, MAX_PATH, L"config.ini");
    return path;
}

void LoadConfig()
{
    std::ifstream file(GetConfigPath());
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        auto trim = [](std::string& s)
        {
            size_t a = s.find_first_not_of(" \t\r\n");
            size_t b = s.find_last_not_of(" \t\r\n");
            if (a == std::string::npos) s.clear();
            else s = s.substr(a, b - a + 1);
        };

        trim(key);
        trim(value);

        if (key == "HotkeyType")
        {
            g_hotkeyType = std::stoi(value);
        }
        else if (key == "Modifiers")
        {
            int mod = 0;
            std::istringstream ss(value);
            std::string token;

            while (std::getline(ss, token, '+'))
            {
                trim(token);
                if (token == "Ctrl")  mod |= MOD_CONTROL;
                if (token == "Shift") mod |= MOD_SHIFT;
                if (token == "Alt")   mod |= MOD_ALT;
                if (token == "Win")   mod |= MOD_WIN;
            }

            if (mod != 0) g_hotkeyModifiers = mod;
        }
        else if (key == "Key")
        {
            if (value.size() == 1)
            {
                unsigned char c = (unsigned char)value[0];
                if (std::isalnum(c)) g_hotkeyKey = (UINT)std::toupper(c);
            }
            else if (value.size() > 1 && value[0] == 'F')
            {
                int n = std::stoi(value.substr(1));
                if (n >= 1 && n <= 24)
                    g_hotkeyKey = VK_F1 + n - 1;
            }
            else if (value == "Space")    g_hotkeyKey = VK_SPACE;
            else if (value == "Tab")      g_hotkeyKey = VK_TAB;
            else if (value == "Enter")    g_hotkeyKey = VK_RETURN;
            else if (value == "Escape")   g_hotkeyKey = VK_ESCAPE;
            else if (value == "Backspace")g_hotkeyKey = VK_BACK;
            else if (value == "Delete")   g_hotkeyKey = VK_DELETE;
            else if (value == "Insert")   g_hotkeyKey = VK_INSERT;
            else if (value == "Home")     g_hotkeyKey = VK_HOME;
            else if (value == "End")      g_hotkeyKey = VK_END;
            else if (value == "PageUp")   g_hotkeyKey = VK_PRIOR;
            else if (value == "PageDown") g_hotkeyKey = VK_NEXT;
        }
        else if (key == "SaveOnMouseClick")
            g_saveOnMouseClick = (value == "Yes" || value == "yes" || value == "True" || value == "true");
        else if (key == "SaveOnMiddleClick")
            g_saveOnMiddleClick = (value == "Yes" || value == "yes" || value == "True" || value == "true");
        else if (key == "SaveOnWheelTilt")
            g_saveOnWheelTilt = (value == "Yes" || value == "yes" || value == "True" || value == "true");
    }
}

void SaveConfig()
{
    std::ofstream file(GetConfigPath());
    if (!file.is_open()) return;

    std::string modStr;
    if (g_hotkeyModifiers & MOD_CONTROL) modStr += "Ctrl+";
    if (g_hotkeyModifiers & MOD_SHIFT)   modStr += "Shift+";
    if (g_hotkeyModifiers & MOD_ALT)     modStr += "Alt+";
    if (g_hotkeyModifiers & MOD_WIN)     modStr += "Win+";
    if (!modStr.empty()) modStr.pop_back();

    std::string keyStr;

    if (g_hotkeyKey >= 'A' && g_hotkeyKey <= 'Z')
        keyStr = std::string(1, (char)g_hotkeyKey);
    else if (g_hotkeyKey >= '0' && g_hotkeyKey <= '9')
        keyStr = std::string(1, (char)g_hotkeyKey);
    else if (g_hotkeyKey >= VK_F1 && g_hotkeyKey <= VK_F24)
        keyStr = "F" + std::to_string(g_hotkeyKey - VK_F1 + 1);
    else if (g_hotkeyKey == VK_SPACE)    keyStr = "Space";
    else if (g_hotkeyKey == VK_TAB)      keyStr = "Tab";
    else if (g_hotkeyKey == VK_RETURN)   keyStr = "Enter";
    else if (g_hotkeyKey == VK_ESCAPE)   keyStr = "Escape";
    else if (g_hotkeyKey == VK_BACK)     keyStr = "Backspace";
    else if (g_hotkeyKey == VK_DELETE)   keyStr = "Delete";
    else if (g_hotkeyKey == VK_INSERT)   keyStr = "Insert";
    else if (g_hotkeyKey == VK_HOME)     keyStr = "Home";
    else if (g_hotkeyKey == VK_END)      keyStr = "End";
    else if (g_hotkeyKey == VK_PRIOR)    keyStr = "PageUp";
    else if (g_hotkeyKey == VK_NEXT)     keyStr = "PageDown";
    else keyStr = "???";

    file << "[Hotkey]\n";
    file << "HotkeyType=" << g_hotkeyType << "\n";
    file << "Modifiers=" << modStr << "\n";
    file << "Key=" << keyStr << "\n\n";
    file << "SaveOnMouseClick=" << (g_saveOnMouseClick ? "Yes" : "No") << "\n";
    file << "SaveOnMiddleClick=" << (g_saveOnMiddleClick ? "Yes" : "No") << "\n";
    file << "SaveOnWheelTilt=" << (g_saveOnWheelTilt ? "Yes" : "No") << "\n";
}

// ============================================================
// 좌표 저장
// ============================================================
void SaveClickPosition(int x, int y)
{
    wchar_t filePath[MAX_PATH] = {};
    GetModuleFileNameW(NULL, filePath, MAX_PATH);

    wchar_t* lastSlash = wcsrchr(filePath, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';

    wcscat_s(filePath, MAX_PATH, L"click_coordinates.txt");

    HANDLE hFile = CreateFileW(
        filePath,
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE)
    {
        SetWindowTextW(g_hWnd, L"파일 오류!");
        SetTimer(g_hWnd, 2, 1500, NULL);
        return;
    }

    wchar_t buffer[256];
    wsprintfW(buffer, L"point%d=%d,%d\r\n", g_clickCount++, x, y);

    DWORD bytesWritten = 0;
    WriteFile(hFile, buffer, (DWORD)(wcslen(buffer) * sizeof(wchar_t)), &bytesWritten, NULL);
    CloseHandle(hFile);

    SetWindowTextW(g_hWnd, L"저장됨!");
    SetTimer(g_hWnd, 2, 1000, NULL);
}

// ============================================================
// 설정창
// 핵심 수정:
// 기존 코드는 일반 CreateWindowEx로 만든 #32770 창에
// WM_INITDIALOG/INT_PTR CALLBACK를 억지로 붙여서 키 입력 전달이
// 불안정했습니다. 이제 일반 WNDPROC 기반 설정창으로 처리합니다.
// ============================================================
void FinishHotkeyCapture(HWND hDlg, int type, int mods, UINT key)
{
    g_hotkeyType = type;
    g_hotkeyModifiers = mods;
    g_hotkeyKey = key;
    g_isWaitingForKey = false;

    SetDlgItemTextW(hDlg, IDC_BTN_CHANGE, L"단축키 변경");
    UpdateHotkeyDisplay(hDlg, L"설정 완료: ");
    EnableWindow(GetDlgItem(hDlg, IDC_BTN_OK), TRUE);
    SetFocus(GetDlgItem(hDlg, IDC_BTN_CHANGE));
}

bool GetCurrentModifiers()
{
    return false;
}

void CaptureKeyboardHotkey(HWND hDlg, UINT vkCode)
{
    // Ctrl/Shift/Alt/Win 자체를 단독 키로 받지 않도록 함.
    if (vkCode == VK_CONTROL || vkCode == VK_SHIFT ||
        vkCode == VK_MENU || vkCode == VK_LWIN || vkCode == VK_RWIN)
        return;

    int mods = 0;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
    if (GetAsyncKeyState(VK_SHIFT)   & 0x8000) mods |= MOD_SHIFT;
    if (GetAsyncKeyState(VK_MENU)    & 0x8000) mods |= MOD_ALT;
    if ((GetAsyncKeyState(VK_LWIN) & 0x8000) ||
        (GetAsyncKeyState(VK_RWIN) & 0x8000))
        mods |= MOD_WIN;

    FinishHotkeyCapture(hDlg, 0, mods, vkCode);
}

void CaptureMouseHotkey(HWND hDlg, int mouseType)
{
    FinishHotkeyCapture(hDlg, mouseType, 0, 0);
}

LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && g_isWaitingForKey && g_hConfigDlg)
    {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
        {
            // ESC는 캡처 취소
            if (p->vkCode == VK_ESCAPE)
            {
                g_isWaitingForKey = false;
                SetDlgItemTextW(g_hConfigDlg, IDC_BTN_CHANGE, L"단축키 변경");
                UpdateHotkeyDisplay(g_hConfigDlg);
                EnableWindow(GetDlgItem(g_hConfigDlg, IDC_BTN_OK), TRUE);
                return 1;
            }

            CaptureKeyboardHotkey(g_hConfigDlg, p->vkCode);
            return 1;
        }
    }

    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0)
    {
        MSLLHOOKSTRUCT* p = (MSLLHOOKSTRUCT*)lParam;

        // 설정창에서 "단축키 변경" 상태면 마우스 버튼도 직접 캡처
        if (g_isWaitingForKey && g_hConfigDlg)
        {
            if (wParam == WM_LBUTTONDOWN)
            {
                CaptureMouseHotkey(g_hConfigDlg, 1);
                return 1;
            }
            if (wParam == WM_MBUTTONDOWN)
            {
                CaptureMouseHotkey(g_hConfigDlg, 2);
                return 1;
            }
            if (wParam == WM_RBUTTONDOWN)
            {
                CaptureMouseHotkey(g_hConfigDlg, 3);
                return 1;
            }
            if (wParam == WM_XBUTTONDOWN)
            {
                WORD button = HIWORD(p->mouseData);
                CaptureMouseHotkey(g_hConfigDlg, button == XBUTTON1 ? 4 : 5);
                return 1;
            }
        }

        // 일반 마우스 좌표 저장
        if (wParam == WM_LBUTTONDOWN && g_saveOnMouseClick)
            SaveClickPosition(p->pt.x, p->pt.y);
        else if (wParam == WM_MBUTTONDOWN && g_saveOnMiddleClick)
            SaveClickPosition(p->pt.x, p->pt.y);
        else if (wParam == WM_MOUSEHWHEEL && g_saveOnWheelTilt)
            SaveClickPosition(p->pt.x, p->pt.y);

        // 마우스 단축키
        if (g_hotkeyType == 1 && wParam == WM_LBUTTONDOWN)
        {
            if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
                SaveClickPosition(p->pt.x, p->pt.y);
        }
        else if (g_hotkeyType == 2 && wParam == WM_MBUTTONDOWN)
        {
            SaveClickPosition(p->pt.x, p->pt.y);
        }
        else if (g_hotkeyType == 3 && wParam == WM_RBUTTONDOWN)
        {
            SaveClickPosition(p->pt.x, p->pt.y);
        }
        else if (g_hotkeyType == 4 && wParam == WM_XBUTTONDOWN &&
                 HIWORD(p->mouseData) == XBUTTON1)
        {
            SaveClickPosition(p->pt.x, p->pt.y);
        }
        else if (g_hotkeyType == 5 && wParam == WM_XBUTTONDOWN &&
                 HIWORD(p->mouseData) == XBUTTON2)
        {
            SaveClickPosition(p->pt.x, p->pt.y);
        }
    }

    return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
}

// ------------------------------------------------------------
// 설정창 WndProc
// ------------------------------------------------------------
LRESULT CALLBACK ConfigWndProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        g_hConfigDlg = hDlg;

        CheckDlgButton(hDlg, IDC_CHK_LEFT,
                       g_saveOnMouseClick ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHK_MIDDLE,
                       g_saveOnMiddleClick ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CHK_WHEEL,
                       g_saveOnWheelTilt ? BST_CHECKED : BST_UNCHECKED);

        UpdateHotkeyDisplay(hDlg);
        return 0;

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);

        if (id == IDC_BTN_CHANGE)
        {
            g_isWaitingForKey = true;

            SetDlgItemTextW(hDlg, IDC_BTN_CHANGE, L"입력 대기 중... (ESC 취소)");
            SetDlgItemTextW(hDlg, IDC_STATIC_HOTKEY,
                            L"키보드 또는 마우스 버튼을 눌러주세요");
            EnableWindow(GetDlgItem(hDlg, IDC_BTN_OK), FALSE);

            SetForegroundWindow(hDlg);

            // 전역 저수준 훅을 사용하므로 자식 컨트롤 포커스와 관계없이
            // 키보드/마우스 입력을 받을 수 있습니다.
            return 0;
        }

        if (id == IDC_BTN_OK)
        {
            g_saveOnMouseClick =
                (IsDlgButtonChecked(hDlg, IDC_CHK_LEFT) == BST_CHECKED);
            g_saveOnMiddleClick =
                (IsDlgButtonChecked(hDlg, IDC_CHK_MIDDLE) == BST_CHECKED);
            g_saveOnWheelTilt =
                (IsDlgButtonChecked(hDlg, IDC_CHK_WHEEL) == BST_CHECKED);

            SaveConfig();

            // 키보드 전역 단축키는 RegisterHotKey 사용.
            UnregisterHotKey(g_hWnd, 1);
            if (g_hotkeyType == 0)
            {
                RegisterHotKey(g_hWnd, 1,
                               g_hotkeyModifiers, g_hotkeyKey);
            }

            DestroyWindow(hDlg);
            return 0;
        }

        if (id == IDC_BTN_CANCEL)
        {
            DestroyWindow(hDlg);
            return 0;
        }

        if (id == IDC_CHK_LEFT)
        {
            g_saveOnMouseClick =
                (IsDlgButtonChecked(hDlg, IDC_CHK_LEFT) == BST_CHECKED);
            return 0;
        }

        if (id == IDC_CHK_MIDDLE)
        {
            g_saveOnMiddleClick =
                (IsDlgButtonChecked(hDlg, IDC_CHK_MIDDLE) == BST_CHECKED);
            return 0;
        }

        if (id == IDC_CHK_WHEEL)
        {
            g_saveOnWheelTilt =
                (IsDlgButtonChecked(hDlg, IDC_CHK_WHEEL) == BST_CHECKED);
            return 0;
        }

        break;
    }

    case WM_CLOSE:
        DestroyWindow(hDlg);
        return 0;

    case WM_DESTROY:
        g_isWaitingForKey = false;
        g_hConfigDlg = NULL;
        return 0;
    }

    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

// ------------------------------------------------------------
// 설정창 생성
// ------------------------------------------------------------
void ShowConfigDialog(HWND owner)
{
    if (g_hConfigDlg)
    {
        SetForegroundWindow(g_hConfigDlg);
        return;
    }

    const wchar_t* CLASS_NAME = L"MouseTrackerConfigWindow";

    static bool registered = false;
    if (!registered)
    {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = ConfigWndProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = CLASS_NAME;
        wc.hIcon = (HICON)LoadImageW(
            GetModuleHandleW(NULL), MAKEINTRESOURCEW(1),
            IMAGE_ICON, 0, 0, LR_DEFAULTSIZE
        );

        RegisterClassW(&wc);
        registered = true;
    }

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        CLASS_NAME,
        L"마우스 좌표 트래커 - 설정",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 330,
        owner, NULL, GetModuleHandleW(NULL), NULL
    );

    if (!hDlg) return;

    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    // 제목
    HWND title = CreateWindowW(
        L"STATIC", L"단축키 설정",
        WS_CHILD | WS_VISIBLE,
        30, 22, 440, 24, hDlg,
        NULL, GetModuleHandleW(NULL), NULL
    );
    SendMessageW(title, WM_SETFONT, (WPARAM)hFont, TRUE);

    // 현재 단축키 표시
    HWND hotkey = CreateWindowW(
        L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE |
        WS_BORDER,
        30, 55, 440, 45, hDlg,
        (HMENU)IDC_STATIC_HOTKEY,
        GetModuleHandleW(NULL), NULL
    );
    SendMessageW(hotkey, WM_SETFONT, (WPARAM)hFont, TRUE);

    // 변경 버튼
    HWND change = CreateWindowW(
        L"BUTTON", L"단축키 변경",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        175, 112, 150, 34, hDlg,
        (HMENU)IDC_BTN_CHANGE,
        GetModuleHandleW(NULL), NULL
    );
    SendMessageW(change, WM_SETFONT, (WPARAM)hFont, TRUE);

    // 설명
    HWND desc = CreateWindowW(
        L"STATIC",
        L"※ 키보드 키뿐 아니라 마우스 왼쪽/가운데/오른쪽/뒤로/앞으로 버튼도 지정할 수 있습니다.",
        WS_CHILD | WS_VISIBLE,
        30, 154, 440, 38, hDlg,
        NULL, GetModuleHandleW(NULL), NULL
    );
    SendMessageW(desc, WM_SETFONT, (WPARAM)hFont, TRUE);

    // 구분선
    CreateWindowW(
        L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        30, 198, 440, 2, hDlg,
        NULL, GetModuleHandleW(NULL), NULL
    );

    HWND mouseTitle = CreateWindowW(
        L"STATIC", L"마우스 입력 저장",
        WS_CHILD | WS_VISIBLE,
        30, 211, 440, 22, hDlg,
        NULL, GetModuleHandleW(NULL), NULL
    );
    SendMessageW(mouseTitle, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND left = CreateWindowW(
        L"BUTTON", L"왼쪽 클릭",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        30, 240, 125, 25, hDlg,
        (HMENU)IDC_CHK_LEFT,
        GetModuleHandleW(NULL), NULL
    );
    SendMessageW(left, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND middle = CreateWindowW(
        L"BUTTON", L"가운데 클릭 (휠)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        165, 240, 150, 25, hDlg,
        (HMENU)IDC_CHK_MIDDLE,
        GetModuleHandleW(NULL), NULL
    );
    SendMessageW(middle, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND wheel = CreateWindowW(
        L"BUTTON", L"휠 틸트 (좌우)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        325, 240, 145, 25, hDlg,
        (HMENU)IDC_CHK_WHEEL,
        GetModuleHandleW(NULL), NULL
    );
    SendMessageW(wheel, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND ok = CreateWindowW(
        L"BUTTON", L"확인",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
        160, 275, 80, 30, hDlg,
        (HMENU)IDC_BTN_OK,
        GetModuleHandleW(NULL), NULL
    );
    SendMessageW(ok, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND cancel = CreateWindowW(
        L"BUTTON", L"취소",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        260, 275, 80, 30, hDlg,
        (HMENU)IDC_BTN_CANCEL,
        GetModuleHandleW(NULL), NULL
    );
    SendMessageW(cancel, WM_SETFONT, (WPARAM)hFont, TRUE);

    UpdateHotkeyDisplay(hDlg);

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
    SetForegroundWindow(hDlg);
}

// ============================================================
// 컨텍스트 메뉴
// ============================================================
void ShowContextMenu(HWND hWnd, int x, int y)
{
    HMENU hMenu = CreatePopupMenu();

    AppendMenuW(hMenu, MF_STRING, 1001, L"설정");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, 1002, L"종료");

    int cmd = TrackPopupMenu(
        hMenu,
        TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
        x, y, 0, hWnd, NULL
    );

    DestroyMenu(hMenu);

    if (cmd == 1001)
        ShowConfigDialog(hWnd);
    else if (cmd == 1002)
        PostMessageW(hWnd, WM_CLOSE, 0, 0);
}

// ============================================================
// 메인 윈도우
// ============================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
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

        HFONT hFont = CreateFontW(
            48, 0, 0, 0, FW_BOLD,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"맑은 고딕"
        );

        HFONT old = (HFONT)SelectObject(hdc, hFont);
        DrawTextW(hdc, buffer, -1, &rect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old);
        DeleteObject(hFont);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_TIMER:
        if (wParam == 1)
        {
            InvalidateRect(hWnd, NULL, TRUE);
        }
        else if (wParam == 2)
        {
            SetWindowTextW(hWnd, L"마우스 좌표 트래커");
            KillTimer(hWnd, 2);
        }
        return 0;

    case WM_HOTKEY:
        if (wParam == 1 && g_hotkeyType == 0)
        {
            POINT pt;
            GetCursorPos(&pt);
            SaveClickPosition(pt.x, pt.y);
        }
        return 0;

    case WM_CONTEXTMENU:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        if (x == -1 && y == -1)
        {
            RECT r;
            GetWindowRect(hWnd, &r);
            x = r.left + 20;
            y = r.bottom;
        }

        ShowContextMenu(hWnd, x, y);
        return 0;
    }

    case WM_DESTROY:
        if (g_hMouseHook)
        {
            UnhookWindowsHookEx(g_hMouseHook);
            g_hMouseHook = NULL;
        }

        if (g_hKeyboardHook)
        {
            UnhookWindowsHookEx(g_hKeyboardHook);
            g_hKeyboardHook = NULL;
        }

        UnregisterHotKey(hWnd, 1);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

// ============================================================
// 진입점
// ============================================================
int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    LPSTR,
    int nCmdShow)
{
    LoadConfig();

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MouseTrackerClass";

    wc.hIcon = (HICON)LoadImageW(
        hInstance, MAKEINTRESOURCEW(1),
        IMAGE_ICON, 0, 0, LR_DEFAULTSIZE
    );

    if (!wc.hIcon)
        wc.hIcon = LoadIconW(NULL, IDI_INFORMATION);

    RegisterClassW(&wc);

    g_hWnd = CreateWindowW(
        L"MouseTrackerClass",
        L"마우스 좌표 트래커",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        400, 120,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hWnd) return 0;

    SendMessageW(g_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)wc.hIcon);
    SendMessageW(g_hWnd, WM_SETICON, ICON_BIG, (LPARAM)wc.hIcon);

    SetWindowPos(
        g_hWnd, HWND_TOPMOST,
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE
    );

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // 마우스/키보드 캡처 훅
    g_hMouseHook = SetWindowsHookExW(
        WH_MOUSE_LL,
        MouseHookProc,
        hInstance,
        0
    );

    g_hKeyboardHook = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        KeyboardHookProc,
        hInstance,
        0
    );

    if (!g_hMouseHook || !g_hKeyboardHook)
    {
        SetWindowTextW(g_hWnd, L"훅 설치 실패");
        SetTimer(g_hWnd, 2, 1500, NULL);
    }

    // 키보드 단축키만 RegisterHotKey로 등록
    if (g_hotkeyType == 0)
    {
        if (!RegisterHotKey(
                g_hWnd, 1,
                g_hotkeyModifiers,
                g_hotkeyKey))
        {
            SetWindowTextW(g_hWnd, L"단축키 등록 실패");
            SetTimer(g_hWnd, 2, 1500, NULL);
        }
    }

    SetTimer(g_hWnd, 1, 50, NULL);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
