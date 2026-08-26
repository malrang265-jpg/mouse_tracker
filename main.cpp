#include <windows.h>
#include <windowsx.h>   // ✅ GET_X_LPARAM, GET_Y_LPARAM 사용을 위해 추가
#include <string>
#include <fstream>
#include <ctime>

HWND g_hWnd;

// 📝 클릭 좌표를 파일에 기록하는 함수 (point1, point2, ... 순서대로)
void SaveClickPosition(int x, int y) {
    // 실행 파일과 같은 폴더에 "click_coordinates.txt" 파일 생성
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) {
        *(lastSlash + 1) = L'\0';
    }
    
    std::wstring filePath = std::wstring(exePath) + L"click_coordinates.txt";
    
    // ✅ MinGW 호환을 위해 c_str()로 변환하여 전달
    std::wofstream file(filePath.c_str(), std::ios::app);
    if (file.is_open()) {
        // ✅ 클릭 횟수를 세는 정적 변수 (프로그램 실행 중 계속 유지)
        static int clickCount = 1;
        
        // ✅ point1=100,200  형식으로 저장
        file << L"point" << clickCount++ << L"=" << x << L"," << y << std::endl;
        
        file.close();
        
        // 제목 표시줄에 저장 성공 알림
        SetWindowTextW(g_hWnd, L"✅ 좌표 저장됨!");
        SetTimer(g_hWnd, 2, 1000, NULL);
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
        case WM_LBUTTONDOWN: {
            // 🖱️ 마우스 왼쪽 버튼 클릭
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            
            // 클라이언트 좌표를 화면 절대 좌표로 변환
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
