#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>     // ✅ SHGetFolderPathW 사용을 위해 추가
#include <string>
#include <ctime>

HWND g_hWnd;

// 📝 바탕화면에 좌표를 저장하는 함수
void SaveClickPosition(int x, int y) {
    wchar_t filePath[MAX_PATH];
    
    // 1. 바탕화면 경로 가져오기 (항상 쓰기 가능)
    HRESULT hr = SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, 0, filePath);
    if (FAILED(hr)) {
        // 바탕화면 경로를 못 가져오면 임시 폴더로 대체
        GetTempPathW(MAX_PATH, filePath);
    }
    
    // 2. 파일명 추가: "C:\Users\사용자\Desktop\click_coordinates.txt"
    wcscat_s(filePath, MAX_PATH, L"\\click_coordinates.txt");
    
    // 3. 파일 열기 (없으면 생성, 있으면 뒤에 이어쓰기)
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
    
    // 4. 클릭 횟수 (프로그램 실행 중 유지)
    static int clickCount = 1;
    
    // 5. 저장 텍스트: "point1=100,200\r\n"
    wchar_t buffer[256];
    wsprintfW(buffer, L"point%d=%d,%d\r\n", clickCount++, x, y);
    
    // 6. 파일에 쓰기
    DWORD bytesWritten;
    WriteFile(hFile, buffer, wcslen(buffer) * sizeof(wchar_t), &bytesWritten, NULL);
    
    // 7. 파일 닫기
    CloseHandle(hFile);
    
    // 8. 성공 알림 (제목 표시줄에 표시)
    SetWindowTextW(g_hWnd, L"✅ 바탕화면에 저장됨!");
    SetTimer(g_hWnd, 2, 1500, NULL);
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
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            
            POINT pt = { x, y };
            ClientToScreen(hWnd, &pt);
            
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
