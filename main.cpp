#include <windows.h>
#include <string>
#include <fstream>
#include <ctime>

HWND g_hWnd;

// 📝 클릭 좌표를 파일에 기록하는 함수
void SaveClickPosition(int x, int y) {
    // 실행 파일과 같은 폴더에 "click_coordinates.txt" 파일 생성
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    
    // 실행 파일 경로에서 디렉토리 경로만 추출
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) {
        *(lastSlash + 1) = L'\0';  // 파일명 제거하고 디렉토리 경로만 남김
    }
    
    // 전체 파일 경로 생성: 디렉토리 + "click_coordinates.txt"
    std::wstring filePath = std::wstring(exePath) + L"click_coordinates.txt";
    
    // 파일을 Append 모드로 열기 (없으면 새로 생성)
    std::wofstream file(filePath, std::ios::app);
    if (file.is_open()) {
        // 현재 시간 가져오기
        time_t now = time(NULL);
        struct tm timeInfo;
        localtime_s(&timeInfo, &now);
        
        // [시간] X: 좌표  Y: 좌표 형식으로 기록
        file << L"["
             << (timeInfo.tm_year + 1900) << L"-"
             << (timeInfo.tm_mon + 1) << L"-"
             << timeInfo.tm_mday << L" "
             << timeInfo.tm_hour << L":"
             << timeInfo.tm_min << L":"
             << timeInfo.tm_sec << L"] "
             << L"X: " << x << L"  Y: " << y << std::endl;
        
        file.close();
        
        // 📢 디버그용: 파일 저장 성공을 알림 (선택사항)
        // 창 제목에 잠시 표시해도 좋음
        SetWindowTextW(g_hWnd, L"✅ 좌표 저장됨!");
        SetTimer(g_hWnd, 2, 1000, NULL);  // 1초 후 원래 제목으로 복원
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
                // 50ms마다 좌표 갱신
                InvalidateRect(hWnd, NULL, TRUE);
            } else if (wParam == 2) {
                // 1초 후 제목 복원
                SetWindowTextW(hWnd, L"마우스 좌표 트래커");
                KillTimer(hWnd, 2);
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            // 🖱️ 마우스 왼쪽 버튼 클릭 이벤트
            // lParam의 하위 16비트: X 좌표, 상위 16비트: Y 좌표
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            
            // 클라이언트 좌표를 화면 좌표로 변환 (절대 좌표로 저장)
            POINT pt = { x, y };
            ClientToScreen(hWnd, &pt);
            
            // 파일에 저장
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
