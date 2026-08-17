#include "app_window.hpp"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    aerosync_win::AppWindow appWin;
    if (!appWin.initialize(hInstance, nCmdShow)) {
        return -1;
    }
    return appWin.runEventLoop();
}
