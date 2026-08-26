#include <windows.h>
#include <windowsx.h>
#include <string>
#include <ctime>

HWND g_hWnd;

// 📝 Windows API를 사용하여 좌표를 파일에 저장 (로케일 문제 없음)
void SaveClickPosition(int x, int y) {
    // 1. 실행 파일 경로 가져오기
    wchar_t filePath[MAX_PATH];
    GetModuleFileNameW(NULL, filePath, MAX_PATH);
    
    // 2. 파일명 제거하고 디렉토리 경로만 남기기
    wchar_t* lastSlash = wcsrchr(filePath, L'\\');
    if (lastSlash) {
        *(lastSlash + 1) = L'\0';
    }
    // 3. 파일명 추가
    wcscat_s(filePath, MAX_PATH, L"click_coordinates.txt");
    
    // 4. 파일 열기 (없으면 생성, 있으면 뒤에 이어쓰기)
    HANDLE hFile = CreateFileW(
        filePath,
        FILE_APPEND_DATA,          // 뒤에 이어쓰기 모드
        FILE_SHARE_READ,           // 다른 프로세스가 읽을 수 있음
        NULL,
        OPEN_ALWAYS,               // 없으면 생성, 있으면 열기
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        // 파일 열기 실패 시 제목에 표시
        SetWindowTextW(g_hWnd, L"❌ 파일 오류!");
        SetTimer(g_hWnd, 2, 1500, NULL);
        return;
    }
    
    // 5. 클릭 횟수 (프로그램 실행 중 유지)
    static int clickCount = 1;
    
    // 6. 저장할 텍스트 생성: "point1=100,200\r\n"
    wchar_t buffer[256];
    wsprintfW(buffer, L"point%d=%d,%d\r\n", clickCount++, x, y);
    
    // 7. 파일에 쓰기
    DWORD bytesWritten;
    WriteFile(hFile, buffer, wcslen(buffer) * sizeof(wchar_t), &bytesWritten, NULL);
    
    // 8. 파일 닫기
    CloseHandle(hFile);
    
    // 9. 성공 알림
    SetWindowTextW(g_hWnd, L"✅ 저장됨!");
    SetTimer(g_hWnd, 2, 1000, NULL);
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
                // 제목 복원
                SetWindowTextW(hWnd, L"마우스 좌표 트래커");
                KillTimer(hWnd, 2);
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            // 마우스 클릭 이벤트
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            
            // 클라이언트 좌표 → 화면 절대 좌표로 변환
            POINT pt = { x, y };
            ClientToScreen(hWnd, &pt);
            
            // 파일에 저장 (point1, point2, ...)
            SaveClickPosition(pt.x, pt.y);
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MouseTrackerClass";
    wc.hIcon = LoadIcon(NULL, IDI_INFORMATION);
    RegisterClassW(&wc);

    g_hWnd = CreateWindowW(
        L"MouseTrackerClass", L"마우스 좌표 트래커",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 120,
        NULL, NULL, hInstance, NULL
    );

    SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    SetTimer(g_hWnd, 1, 50, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
