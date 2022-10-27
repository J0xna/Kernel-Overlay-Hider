#include <iostream>
#include <Windows.h>

BOOL CALLBACK EnumWindowsProc(HWND hWnd, long lParam) {
    char buff[255];

    if (IsWindowVisible(hWnd)) {
        GetWindowTextA(hWnd, buff, 64);
        printf("%s\n", buff);
    }
    return TRUE;
}

int main() {
    EnumWindows((WNDENUMPROC)EnumWindowsProc, 0);
    std::cin.get( );
    return 0;
}