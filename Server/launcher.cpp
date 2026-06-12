#include <windows.h>

#define ID_START_BUTTON 1001

HFONT hFont, hFontTitle, hFontMono;
HBRUSH hBgBrush, hBtnBrush, hBtnHoverBrush;
COLORREF clrBg        = RGB(6, 8, 8);     // near-black charcoal
COLORREF clrAccent    = RGB(220, 30, 30); // imperial red
COLORREF clrAccentDim = RGB(90, 15, 15);  // dim red for lines
COLORREF clrText      = RGB(200, 210, 210);
COLORREF clrTextDim   = RGB(110, 120, 120);
COLORREF clrBtn       = RGB(16, 20, 20);
COLORREF clrBtnHov    = RGB(35, 14, 14);

bool g_hover = false;

void DrawCornerBrackets(HDC hdc, RECT r, int len, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HPEN old = (HPEN)SelectObject(hdc, pen);

    // top-left
    MoveToEx(hdc, r.left, r.top + len, NULL);
    LineTo(hdc, r.left, r.top);
    LineTo(hdc, r.left + len, r.top);

    // top-right
    MoveToEx(hdc, r.right - len, r.top, NULL);
    LineTo(hdc, r.right, r.top);
    LineTo(hdc, r.right, r.top + len);

    // bottom-left
    MoveToEx(hdc, r.left, r.bottom - len, NULL);
    LineTo(hdc, r.left, r.bottom);
    LineTo(hdc, r.left + len, r.bottom);

    // bottom-right
    MoveToEx(hdc, r.right - len, r.bottom, NULL);
    LineTo(hdc, r.right, r.bottom);
    LineTo(hdc, r.right, r.bottom - len);

    SelectObject(hdc, old);
    DeleteObject(pen);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            hFont = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, FF_DONTCARE, L"Consolas");

            hFontTitle = CreateFontW(26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, FF_DONTCARE, L"Consolas");

            hFontMono = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, FF_DONTCARE, L"Consolas");

            HWND hBtn = CreateWindowW(L"BUTTON", L"INITIATE",
                WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                210, 240, 200, 56,
                hwnd, (HMENU)ID_START_BUTTON, NULL, NULL);

            SendMessage(hBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

            hBgBrush = CreateSolidBrush(clrBg);
            hBtnBrush = CreateSolidBrush(clrBtn);
            hBtnHoverBrush = CreateSolidBrush(clrBtnHov);
            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, clrText);
            SetBkColor(hdc, clrBg);
            return (LRESULT)hBgBrush;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
            if (dis->CtlID == ID_START_BUTTON) {
                bool pressed = (dis->itemState & ODS_SELECTED);
                bool hover = g_hover;

                HBRUSH bg = hover ? hBtnHoverBrush : hBtnBrush;
                FillRect(dis->hDC, &dis->rcItem, bg);

                // angular notched border (chamfered corners)
                COLORREF lineColor = hover ? clrAccent : clrAccentDim;
                HPEN pen = CreatePen(PS_SOLID, 2, lineColor);
                HPEN oldPen = (HPEN)SelectObject(dis->hDC, pen);

                int l = dis->rcItem.left, t = dis->rcItem.top;
                int r = dis->rcItem.right - 1, b = dis->rcItem.bottom - 1;
                int notch = 10;

                POINT pts[] = {
                    { l + notch, t },
                    { r - notch, t },
                    { r, t + notch },
                    { r, b - notch },
                    { r - notch, b },
                    { l + notch, b },
                    { l, b - notch },
                    { l, t + notch },
                    { l + notch, t }
                };
                Polyline(dis->hDC, pts, 9);

                SelectObject(dis->hDC, oldPen);
                DeleteObject(pen);

                SetTextColor(dis->hDC, pressed ? RGB(255,255,255) : clrAccent);
                SetBkMode(dis->hDC, TRANSPARENT);
                SelectObject(dis->hDC, hFont);

                RECT rt = dis->rcItem;
                DrawTextW(dis->hDC, L"I N I T I A T E", -1, &rt,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            return TRUE;
        }

        case WM_MOUSEMOVE: {
            RECT btnRect;
            HWND hBtn = GetDlgItem(hwnd, ID_START_BUTTON);
            GetWindowRect(hBtn, &btnRect);
            POINT pt;
            GetCursorPos(&pt);
            bool now = PtInRect(&btnRect, pt);
            if (now != g_hover) {
                g_hover = now;
                InvalidateRect(hBtn, NULL, TRUE);
            }

            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            break;
        }

        case WM_MOUSELEAVE: {
            g_hover = false;
            InvalidateRect(GetDlgItem(hwnd, ID_START_BUTTON), NULL, TRUE);
            break;
        }

        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, hBgBrush);
            return 1;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT client;
            GetClientRect(hwnd, &client);

            SetBkMode(hdc, TRANSPARENT);

            // Outer frame
            RECT outer = { 14, 14, client.right - 14, client.bottom - 14 };
            HPEN framePen = CreatePen(PS_SOLID, 1, clrAccentDim);
            HPEN oldPen = (HPEN)SelectObject(hdc, framePen);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, outer.left, outer.top, outer.right, outer.bottom);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(framePen);

            // Corner brackets (targeting reticle style)
            DrawCornerBrackets(hdc, outer, 28, clrAccent);

            // Thin horizontal divider lines (panel seams)
            HPEN seamPen = CreatePen(PS_SOLID, 1, RGB(40, 40, 40));
            HPEN oldSeam = (HPEN)SelectObject(hdc, seamPen);
            MoveToEx(hdc, outer.left + 10, 100, NULL);
            LineTo(hdc, outer.right - 10, 100);
            MoveToEx(hdc, outer.left + 10, outer.bottom - 50, NULL);
            LineTo(hdc, outer.right - 10, outer.bottom - 50);
            SelectObject(hdc, oldSeam);
            DeleteObject(seamPen);

            // Title block
            SetTextColor(hdc, clrAccent);
            SelectObject(hdc, hFontTitle);
            RECT title = {0, 36, client.right, 70};
            DrawTextW(hdc, L"IMPERIAL TERMINAL", -1, &title,
                      DT_CENTER | DT_SINGLELINE);

            // Status line
            SetTextColor(hdc, clrTextDim);
            SelectObject(hdc, hFontMono);
            RECT sub = {0, 72, client.right, 96};
            DrawTextW(hdc, L"CLEARANCE LEVEL: COMMANDER  //  SECTOR LINK: ACTIVE", -1, &sub,
                      DT_CENTER | DT_SINGLELINE);

            // Side readouts (left)
            SetTextColor(hdc, clrTextDim);
            RECT left1 = { 36, 130, 300, 154 };
            DrawTextW(hdc, L"> SYSTEM .......... STANDBY", -1, &left1, DT_LEFT | DT_SINGLELINE);
            RECT left2 = { 36, 158, 300, 182 };
            DrawTextW(hdc, L"> POWER CORE ...... NOMINAL", -1, &left2, DT_LEFT | DT_SINGLELINE);
            RECT left3 = { 36, 186, 300, 210 };
            DrawTextW(hdc, L"> UPLINK .......... ENCRYPTED", -1, &left3, DT_LEFT | DT_SINGLELINE);

            // Side readouts (right)
            RECT right1 = { client.right - 300, 130, client.right - 36, 154 };
            DrawTextW(hdc, L"AWAITING COMMAND <", -1, &right1, DT_RIGHT | DT_SINGLELINE);
            RECT right2 = { client.right - 300, 158, client.right - 36, 182 };
            DrawTextW(hdc, L"NODE 7-66 <", -1, &right2, DT_RIGHT | DT_SINGLELINE);
            RECT right3 = { client.right - 300, 186, client.right - 36, 210 };
            DrawTextW(hdc, L"SIGNAL: STRONG <", -1, &right3, DT_RIGHT | DT_SINGLELINE);

            // Bottom status bar
            SetTextColor(hdc, clrAccentDim);
            RECT bottom = {0, client.bottom - 42, client.right, client.bottom - 18};
            DrawTextW(hdc, L"PRESS INITIATE TO ESTABLISH SERVER CONNECTION", -1, &bottom,
                      DT_CENTER | DT_SINGLELINE);

            EndPaint(hwnd, &ps);
            break;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_START_BUTTON) {
                // Open a terminal window and run "bun run server.ts"
                STARTUPINFOW si = { sizeof(si) };
                PROCESS_INFORMATION pi;

                wchar_t cmdBuf[] = L"cmd.exe /K \"bun run server.ts\"";

                CreateProcessW(
                    NULL,
                    cmdBuf,
                    NULL, NULL, FALSE,
                    CREATE_NEW_CONSOLE,
                    NULL, NULL,
                    &si, &pi
                );

                if (pi.hProcess) CloseHandle(pi.hProcess);
                if (pi.hThread) CloseHandle(pi.hThread);

                // Give the server a moment, then open localhost:3000
                Sleep(1500);
                ShellExecuteW(NULL, L"open", L"http://localhost:3000",
                               NULL, NULL, SW_SHOWNORMAL);
            }
            break;

        case WM_DESTROY:
            DeleteObject(hBgBrush);
            DeleteObject(hBtnBrush);
            DeleteObject(hBtnHoverBrush);
            DeleteObject(hFont);
            DeleteObject(hFontTitle);
            DeleteObject(hFontMono);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"ImperialLauncherClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Phantom // Imperial Launcher",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 380,
        NULL, NULL, hInstance, NULL
    );

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}