#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <string>
#include <ctime>

HWND g_hWnd;
HHOOK g_hMouseHook = NULL;  // 전역 마우스 훅 핸들
int g_clickCount = 1;       // 클릭 횟수 (전역에서 공유)

// 📝 바탕화면에 좌표를 저장하는 함수
void SaveClickPosition(int x, int y) {
    wchar_t filePath[MAX_PATH];
    
    // 바탕화면 경로 가져오기
    HRESULT hr = SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, 0, filePath);
    if (FAILED(hr)) {
        GetTempPathW(MAX_PATH, filePath);
    }
    wcscat_s(filePath, MAX_PATH, L"\\click_coordinates.txt");
    
    // 파일 열기 (없으면 생성, 있으면 뒤에 이어쓰기)
    HANDLE hFile = CreateFileW(
        filePath,
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        SetWindowTextW(g_hWnd, L"❌ 파일 오류!");
        SetTimer(g_hWnd, 2, 1500, NULL);
        return;
    }
    
    // 저장 텍스트: "point1=100,200\r\n"
    wchar_t buffer[256];
    wsprintfW(buffer, L"point%d=%d,%d\r\n", g_clickCount++, x, y);
    
    // 파일에 쓰기
    DWORD bytesWritten;
    WriteFile(hFile, buffer, wcslen(buffer) * sizeof(wchar_t), &bytesWritten, NULL);
    CloseHandle(hFile);
    
    // 성공 알림
    SetWindowTextW(g_hWnd, L"✅ 저장됨!");
    SetTimer(g_hWnd, 2, 1000, NULL);
}

// 🖱️ 전역 마우스 훅 프로시저 (모든 창에서 마우스 이벤트 감지)
LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_LBUTTONDOWN) {
        // 마우스 왼쪽 버튼 클릭 감지
        MSLLHOOKSTRUCT* pMouseStruct = (MSLLHOOKSTRUCT*)lParam;
        if (pMouseStruct) {
            // 클릭한 위치의 화면 좌표 저장
            SaveClickPosition(pMouseStruct->pt.x, pMouseStruct->pt.y);
        }
    }
    // 다음 훅으로 이벤트 전달 (중요: 시스템 동작을 방해하지 않음)
    return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
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
        case WM_DESTROY:
            // 프로그램 종료 시 훅 해제
            if (g_hMouseHook) {
                UnhookWindowsHookEx(g_hMouseHook);
                g_hMouseHook = NULL;
            }
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. 윈도우 클래스 등록
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MouseTrackerClass";
    wc.hIcon = LoadIcon(NULL, IDI_INFORMATION);
    RegisterClassW(&wc);

    // 2. 윈도우 생성
    g_hWnd = CreateWindowW(
        L"MouseTrackerClass", L"마우스 좌표 트래커",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 120,
        NULL, NULL, hInstance, NULL
    );

    SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // 3. 전역 마우스 훅 설치 (WH_MOUSE_LL)
    g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, hInstance, 0);
    if (g_hMouseHook == NULL) {
        // 훅 설치 실패 시 메시지 표시
        SetWindowTextW(g_hWnd, L"❌ 훅 설치 실패!");
        // 그래도 프로그램은 계속 실행 (창 내부 클릭은 동작함)
    } else {
        SetWindowTextW(g_hWnd, L"🔄 전역 감지 활성화");
        SetTimer(g_hWnd, 2, 1500, NULL);
    }

    // 4. 좌표 갱신 타이머
    SetTimer(g_hWnd, 1, 50, NULL);

    // 5. 메시지 루프
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
