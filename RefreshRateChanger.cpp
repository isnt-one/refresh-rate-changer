#include <stdio.h>
#include <iostream>
#include <Windows.h>
#include <string>
#include <strsafe.h>

#define throw_err(str) throw std::runtime_error(std::string(str).c_str());
#define MIN_ALL 419

HANDLE hStartEvent = NULL;
bool isRunning = true;
bool useNotification = false;

enum {
    CLOSE_KEYID = 0,
    CHANGE_RESOLUTION_KEYID = 1,
    TOGGLE_WINDOW_KEYID = 2,
    LOCK_WORKSTATION_KEYID = 3,
    PRINT_INSTRUCTIONS_KEYID = 4,
    TOGGLE_NOTIFICATION_KEYID = 5
};

void ToggleWindow()
{
    HWND hConsole = GetConsoleWindow();
    int isVisible = IsWindowVisible(hConsole);
    ShowWindow(hConsole, isVisible ? SW_HIDE : SW_NORMAL);
}

void PrintInstructions()
{
    static bool bRan = false;

    if (!bRan) std::cout << "Running refresh rate changer...\n\n";

    std::cout << "Ctrl + F10    = Close application\n"
        << "Ctrl + F12    = Change refresh rate\n"
        << "Ctrl + Home   = Hide window\n"
        << "Ctrl + Insert = Print instructions\n"
        << "Ctrl + End    = Toggle notifications\n"
        << "Shft + Escape = Minimize everything and lock computer\n\n";

    bRan = true;
}

double SwitchRefreshRate(bool poll = false)
{
    DEVMODE dm;
    dm.dmSize = sizeof(DEVMODE);

    if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm) != 0)
    {
        if (poll) return dm.dmDisplayFrequency;

        double oldFrequency = dm.dmDisplayFrequency;
        double newFrequency = dm.dmDisplayFrequency == 60 ? 120 : 60;

        dm.dmDisplayFrequency = newFrequency;
        dm.dmFields = DM_DISPLAYFREQUENCY;

        int iRet = ChangeDisplaySettings(&dm, CDS_TEST);
        if (iRet == DISP_CHANGE_FAILED)
        {
            std::cout << "Unable to change display settings\n";
            return oldFrequency;
        }
        else
        {
            iRet = ChangeDisplaySettings(&dm, CDS_UPDATEREGISTRY);
            switch (iRet)
            {
                case DISP_CHANGE_SUCCESSFUL:
                {
                    std::cout << "Refresh rate change successful\n";
                    return newFrequency;
                }
                case DISP_CHANGE_RESTART:
                {
                    std::cout << "Reboot required for change\n";
                    return oldFrequency;
                }
                default:
                {
                    std::cout << "Could not change settings\n";
                    return oldFrequency;
                }
            }
        }
    }

    return 0;
}

bool EnsureSingleInstance()
{
    hStartEvent = CreateEventW(NULL, true, false, L"RefreshRateChanger");

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(hStartEvent);
        hStartEvent = NULL;
        return false;
    }

    return true;
}

void MinimizeAll()
{
    HWND hWnd = FindWindow(L"Shell_TrayWnd", NULL);
    SendMessage(hWnd, WM_COMMAND, MIN_ALL, 0);
}

void SetTitle(DWORD frequency)
{
    std::string title = "Refresh Rate Changer - " + std::to_string(frequency) + "hz";
    SetConsoleTitleA(title.c_str());
}

void NotifySettingsChange(DWORD frequency)
{
    if (useNotification)
    {
        NOTIFYICONDATA nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd = GetConsoleWindow();
        nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_INFO;
        nid.dwInfoFlags = NIIF_INFO;

        std::wstring msg = L"Refresh rate changed to " + std::to_wstring(frequency) + L"hz";

        StringCchCopy(nid.szTip, ARRAYSIZE(nid.szTip), L"Refresh Rate Changer");
        StringCchCopy(nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle), L"Display settings updated");
        StringCchCopy(nid.szInfo, ARRAYSIZE(nid.szInfo), msg.c_str());

        Shell_NotifyIcon(NIM_ADD, &nid);
        Shell_NotifyIcon(NIM_MODIFY, &nid);
        Shell_NotifyIcon(NIM_SETVERSION, &nid);
    }

    SetTitle(frequency);
    std::cout << "Current refresh rate: " << frequency << "hz\n";
    Beep(frequency + 200, 300);
}

void HotkeyHandler(DWORD dwKeyId)
{
    switch (dwKeyId)
    {
        case CLOSE_KEYID:
        {
            std::cout << "Exiting!\n";
            isRunning = false;

            Beep(420, 200);
            Beep(420, 200);
            Beep(420, 200);

            break;
        }
        case CHANGE_RESOLUTION_KEYID:
        {
            double refreshRate = SwitchRefreshRate();
            NotifySettingsChange(refreshRate);
            break;
        }
        case TOGGLE_WINDOW_KEYID:
            ToggleWindow();
            break;
        case LOCK_WORKSTATION_KEYID:
            MinimizeAll();
            LockWorkStation();
            break;
        case PRINT_INSTRUCTIONS_KEYID:
            PrintInstructions();
            break;
        case TOGGLE_NOTIFICATION_KEYID:
        {
            useNotification = !useNotification;
            std::cout << "Notifications are now " << ((useNotification == true) ? "active" : "inactive") << "\n";
            break;
        }
        default:
            break;
    }
}

void RegisterHotKeys()
{
    if (!RegisterHotKey(NULL, CLOSE_KEYID, MOD_NOREPEAT | MOD_CONTROL, VK_F10))
    {
        throw_err("Could not register hotkey to exit application");
    }

    if (!RegisterHotKey(NULL, CHANGE_RESOLUTION_KEYID, MOD_NOREPEAT | MOD_CONTROL, VK_F12))
    {
        throw_err("Could not register hotkey to change refresh rate");
    }

    if (!RegisterHotKey(NULL, TOGGLE_WINDOW_KEYID, MOD_NOREPEAT | MOD_CONTROL, VK_HOME))
    {
        throw_err("Could not register hotkey to toggle window visibility");
    }

    if (!RegisterHotKey(NULL, LOCK_WORKSTATION_KEYID, MOD_NOREPEAT | MOD_SHIFT, VK_ESCAPE))
    {
        throw_err("Could not register hotkey to lock workstation");
    }

    if (!RegisterHotKey(NULL, PRINT_INSTRUCTIONS_KEYID, MOD_NOREPEAT | MOD_CONTROL, VK_INSERT))
    {
        throw_err("Could not register hotkey to print instructions");
    }

    if (!RegisterHotKey(NULL, TOGGLE_NOTIFICATION_KEYID, MOD_NOREPEAT | MOD_CONTROL, VK_END))
    {
        throw_err("Could not register hotkey to toggle notifications");
    }
}

void RemoveHotKeys()
{
    UnregisterHotKey(NULL, CLOSE_KEYID);
    UnregisterHotKey(NULL, CHANGE_RESOLUTION_KEYID);
    UnregisterHotKey(NULL, LOCK_WORKSTATION_KEYID);
    UnregisterHotKey(NULL, PRINT_INSTRUCTIONS_KEYID);
    UnregisterHotKey(NULL, TOGGLE_NOTIFICATION_KEYID);
}

int main()
{
    try {
        SetConsoleTitle(L"Refresh Rate Changer");
        PrintInstructions();

        if (!EnsureSingleInstance())
        {
            throw_err("Instance already running");
        }

        RegisterHotKeys();

        double refreshRateAtLaunch = SwitchRefreshRate(true);
        std::cout << "Refresh rate at launch: " << refreshRateAtLaunch << "hz\n\n";

        if (refreshRateAtLaunch == 60)
        {
            double newRefreshRate = SwitchRefreshRate();
            NotifySettingsChange(newRefreshRate);
        }
        else
        {
            SetTitle(refreshRateAtLaunch);
        }

        MSG msg;
        while (isRunning && GetMessage(&msg, NULL, 0, 0))
        {
            PeekMessage(&msg, 0, 0, 0, 0x0001);

            switch (msg.message)
            {
                case WM_HOTKEY:
                    HotkeyHandler(msg.wParam);
                    break;
                default:
                    break;
            }
        }
    }
    catch (std::exception& e)
    {
        std::cout << "Exception: " << e.what() << "\n";

        Beep(420, 200);
        Beep(420, 200);
        Beep(420, 200);
    }

    RemoveHotKeys();
    CloseHandle(hStartEvent);
    
    return 0;
}