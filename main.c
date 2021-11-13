/*
 * hide-volume-osd-c is a language port of HideVolumeOSD project, written for
 * fair use.
 *
 * HideVolumeOSD Copyright (C) 2016-2021 UnlimitedStack and contributors
 *
 * This file is a part of hide-volume-osd-c project
 * Copyright (c) 2021 Philosoph228 <philosoph228@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <strsafe.h>
#include <assert.h>
#include <uxtheme.h>
#include "resource.h"

#define LR_FAILED(x) (x != ERROR_SUCCESS)
#define LR_SUCCEEDED(x) (x == ERROR_SUCCESS)

#define HVWM_TRAYICON (WM_APP + 100)

const WCHAR szAppName[] = L"HideVolumeOSD-C";
const WCHAR szAppMutex[] = L"{F95D0266-5DA1-4701-8188-78F17E2F98F}";

HWND hWndInject;
HWND hMsgWindow;

enum {
  STATUS_UNKNOWN,
  STATUS_HIDDEN,
  STATUS_SHOWN,
};

int g_status;

enum {
  ICON_THEME_AUTO,
  ICON_THEME_LIGHT,
  ICON_THEME_DARK
};

typedef enum {
  AUTORUN_NOT_SET,
  AUTORUN_SET,
  AUTORUN_DIFFER_LOCATION,
} AUTORUNSTATE, *PAUTORUNSTATE;

typedef struct _tagSETTINGS {
  BOOL bShowTrayIcon;
  int iIconTheme;
} SETTINGS, * PSETTINGS;

SETTINGS g_settings;

BOOL Init();
void HideOSD();
void ShowOSD();
HWND FindOSDWindow();
BOOL CheckInstance(LPCWSTR);
BOOL SystemUsesLightTheme(PBOOL);

void ShowOSD()
{
  if (!IsWindow(hWndInject))
  {
    if (!Init()) return;
  }

  ShowWindow(hWndInject, SW_RESTORE);

  /* Show window on the screen */
  keybd_event(VK_VOLUME_UP, 0, 0, 0);
  keybd_event(VK_VOLUME_DOWN, 0, 0, 0);
}

void HideOSD()
{
  if (!IsWindow(hWndInject))
  {
    if (!Init()) return;
  }

  ShowWindow(hWndInject, SW_MINIMIZE);
}

BOOL CheckInstance(LPCWSTR szMutexName)
{
  if (CreateMutex(NULL, FALSE, szMutexName) == NULL ||
      GetLastError() == ERROR_ALREADY_EXISTS ||
      GetLastError() == ERROR_ACCESS_DENIED)
  {
    return TRUE;
  }

  return FALSE;
}

BOOL Init()
{
  hWndInject = FindOSDWindow();
  int count = 1;

  while (hWndInject == NULL && count < 9)
  {
    keybd_event(VK_VOLUME_UP, 0, 0, 0);
    keybd_event(VK_VOLUME_DOWN, 0, 0, 0);

    hWndInject = FindOSDWindow();

    /* Quadratic backoff if the window is not found */
    Sleep(1000 * (count * count));
    count++;
  }

  /* Final try */
  hWndInject = FindOSDWindow();

  if (hWndInject == NULL)
  {
    return FALSE;
  }

  return TRUE;
}

extern inline HRESULT QuoteString(LPWSTR pszDest, DWORD dwDestSize,
    LPWSTR pszInput)
{
  return StringCchPrintf(pszDest, dwDestSize, L"\"%s\"", pszInput);
}

void HideTrayIcon()
{
  NOTIFYICONDATA nid = { 0 };
  ZeroMemory(&nid, sizeof(nid));

  nid.cbSize = sizeof(nid);
  nid.hWnd = hMsgWindow;
  nid.uID = 1;

  Shell_NotifyIcon(NIM_DELETE, &nid);
}

HICON GetStatusIcon(int nStatus)
{
  int nIconID;

  switch (g_settings.iIconTheme)
  {
    case ICON_THEME_AUTO:
      {
        BOOL bLightTaskbar;

        SystemUsesLightTheme(&bLightTaskbar);

        if (bLightTaskbar)
        {
          switch (nStatus)
          {
            case STATUS_HIDDEN:
              nIconID = IDI_TRAY_HIDDEN_DARK;
              break;
            case STATUS_SHOWN:
              nIconID = IDI_TRAY_SHOWN_DARK;
              break;
            default:
              nIconID = IDI_TRAY_DEFAULT_DARK;
              break;
          }
        }
        else {
          switch (nStatus)
          {
            case STATUS_HIDDEN:
              nIconID = IDI_TRAY_HIDDEN_LIGHT;
              break;
            case STATUS_SHOWN:
              nIconID = IDI_TRAY_SHOWN_LIGHT;
              break;
            default:
              nIconID = IDI_TRAY_DEFAULT_LIGHT;
              break;
          }
        }
      }
      break;

    case ICON_THEME_DARK:
      switch (nStatus)
      {
        case STATUS_HIDDEN:
          nIconID = IDI_TRAY_HIDDEN_DARK;
          break;
        case STATUS_SHOWN:
          nIconID = IDI_TRAY_SHOWN_DARK;
          break;
        default:
          nIconID = IDI_TRAY_DEFAULT_DARK;
          break;
      }
      break;

    case ICON_THEME_LIGHT:
      switch (nStatus)
      {
        case STATUS_HIDDEN:
          nIconID = IDI_TRAY_HIDDEN_LIGHT;
          break;
        case STATUS_SHOWN:
          nIconID = IDI_TRAY_SHOWN_LIGHT;
          break;
        default:
          nIconID = IDI_TRAY_DEFAULT_LIGHT;
          break;
      }
      break;
  }

  return LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(nIconID));
}

void ShowTrayIcon()
{
  NOTIFYICONDATA nid;
  HICON hIcon;

  hIcon = GetStatusIcon(g_status);

  ZeroMemory(&nid, sizeof(nid));

  nid.cbSize = sizeof(nid);
  nid.hWnd = hMsgWindow;
  nid.uID = 1;
  nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
  nid.uCallbackMessage = HVWM_TRAYICON;
  nid.uVersion = NOTIFYICON_VERSION_4;
  nid.hIcon = hIcon;
  StringCchCopy(nid.szTip, 128, szAppName);

  Shell_NotifyIcon(NIM_ADD, &nid);
  Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

void SetTrayStatusIcon(int nStatus)
{
  NOTIFYICONDATA nid;

  ZeroMemory(&nid, sizeof(nid));
  nid.cbSize = sizeof(nid);
  nid.hWnd = hMsgWindow;
  nid.uID = 1;
  nid.uFlags = NIF_ICON;
  nid.hIcon = GetStatusIcon(nStatus);

  Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void ApplyIconTheme()
{
  SetTrayStatusIcon(g_status);
}

extern inline LRESULT OpenAutorunKey(HKEY* phKey)
{
  return RegCreateKey(HKEY_CURRENT_USER,
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", phKey);
}

extern inline LRESULT OpenSoftwareKey(HKEY* phKey)
{
  return RegCreateKey(HKEY_CURRENT_USER,
      L"Software\\Aragajaga\\HideVolumeOSDC", phKey);
}

BOOL SystemUsesLightTheme(PBOOL pBool)
{
  BOOL bResult;
  LRESULT lResult;
  HKEY hKey;
  DWORD dwType;
  DWORD dwValue;
  DWORD dwLength;

  bResult = FALSE;

  lResult = RegOpenKeyEx(HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      0, KEY_READ, &hKey);

  assert(LR_SUCCEEDED(lResult));
  if (LR_FAILED(lResult))
    return FALSE; 

  dwLength = sizeof(dwValue);

  lResult = RegQueryValueEx(hKey, L"SystemUsesLightTheme", NULL, &dwType,
      (PBYTE) &dwValue, &dwLength);

  assert(LR_SUCCEEDED(lResult));
  if (LR_FAILED(lResult))
    goto finish;

  assert(dwType == REG_DWORD);
  if (dwType != REG_DWORD)
    goto finish; 

  *pBool = dwValue ? TRUE : FALSE;
  bResult = TRUE;

finish:
  RegCloseKey(hKey);
  return bResult;
}

BOOL GetAutorunPath(LPWSTR lpszPath, DWORD dwLength, PDWORD dwWritten)
{
  BOOL bReturn;
  LRESULT lResult;
  HKEY hKey;
  DWORD dwType;

  bReturn = FALSE;

  lResult = OpenAutorunKey(&hKey);

  assert(LR_SUCCEEDED(lResult));
  if (LR_FAILED(lResult))
    return FALSE;

  lResult = RegQueryValueEx(hKey, szAppName, NULL, &dwType, (LPBYTE) lpszPath,
      &dwLength);

  assert(LR_SUCCEEDED(lResult));
  if (LR_FAILED(lResult))
    goto finish;

  switch (dwType)
  {
    case REG_SZ:
    case REG_MULTI_SZ:
    case REG_EXPAND_SZ:
      break;
    default:
      goto finish;
  }

  *dwWritten = dwLength;
  bReturn = TRUE;

finish:
  RegCloseKey(hKey);
  return bReturn;
}

BOOL IsAutorunSet(PAUTORUNSTATE pARState)
{
  BOOL bResult;
  LRESULT lResult;
  HRESULT hResult;
  HKEY hKey;
  DWORD dwType;
  WCHAR szPath[MAX_PATH];
  WCHAR szCurrentImage[MAX_PATH];
  WCHAR szCurrentImageQuoted[MAX_PATH];
  DWORD dwWritten;

  assert(pARState);
  if (!pARState)
    return FALSE;

  bResult = FALSE;
  ZeroMemory(szPath, sizeof(szPath));

  lResult = OpenAutorunKey(&hKey);
  assert(LR_SUCCEEDED(lResult));
  if (LR_FAILED(lResult))
    return FALSE;

  dwWritten = MAX_PATH;

  lResult = RegQueryValueEx(hKey, szAppName, NULL, &dwType, (LPBYTE) szPath,
      &dwWritten);
  
  assert(LR_SUCCEEDED(lResult) || lResult == ERROR_FILE_NOT_FOUND);
  if (LR_FAILED(lResult))
  {
    if (lResult == ERROR_FILE_NOT_FOUND)
    {
      *pARState = AUTORUN_NOT_SET;
      bResult = TRUE;
    }

    goto finish;
  }

  switch (dwType)
  {
    case REG_SZ:
    case REG_MULTI_SZ:
    case REG_EXPAND_SZ:
      break;

    default:
      goto finish;
  }

  if (!dwWritten)
    goto finish;

  dwWritten = GetModuleFileName(NULL, szCurrentImage, MAX_PATH);

  assert(dwWritten);
  if (!dwWritten)
    goto finish;

  hResult = QuoteString(szCurrentImageQuoted, MAX_PATH, szCurrentImage);

  assert(SUCCEEDED(hResult));
  if (FAILED(hResult))
    goto finish;

  *pARState = wcscmp(szCurrentImageQuoted, szPath) == 0
    ? AUTORUN_SET
    : AUTORUN_DIFFER_LOCATION;

  bResult = TRUE;

finish:
  RegCloseKey(hKey);
  return bResult;
}

void InitDefaultSettings(PSETTINGS pSettings)
{
  pSettings->bShowTrayIcon = TRUE;
  pSettings->iIconTheme = 0;
}

BOOL WriteSettingsToRegistry(HKEY hKey, PSETTINGS pSettings)
{
  LRESULT lResult;
  DWORD dwShowIcon;
  DWORD dwIconTheme;

  dwShowIcon = pSettings->bShowTrayIcon;
  dwIconTheme = pSettings->iIconTheme;

  lResult = RegSetValueEx(hKey, L"ShowTrayIcon", 0, REG_DWORD,
      (BYTE *)&dwShowIcon, sizeof(dwShowIcon));

  assert(LR_SUCCEEDED(lResult));
  if (LR_FAILED(lResult))
    return FALSE;

  lResult = RegSetValueEx(hKey, L"IconTheme", 0, REG_DWORD,
      (BYTE *)&dwIconTheme, sizeof(dwIconTheme));

  assert(LR_SUCCEEDED(lResult));
  if (LR_FAILED(lResult))
    return FALSE;

  return TRUE;
}

BOOL WriteSettingsToRegistryDummy(PSETTINGS pSettings)
{
  BOOL bResult;
  LRESULT lResult;
  HKEY hKey;

  lResult = OpenSoftwareKey(&hKey);

  assert(LR_SUCCEEDED(lResult));
  if (LR_FAILED(lResult))
    return FALSE;

  bResult = WriteSettingsToRegistry(hKey, pSettings);

  RegCloseKey(hKey);
  return bResult;
}

BOOL PreloadSettings()
{
  LRESULT lResult;
  HKEY hKey;
  SETTINGS settings;
  
  ZeroMemory(&settings, sizeof(settings));

  lResult = OpenSoftwareKey(&hKey);

  assert(LR_SUCCEEDED(lResult));
  if (LR_FAILED(lResult))
    return FALSE;

  return TRUE;
}

BOOL WriteDefaultSettings(HKEY hKey, PSETTINGS pSettings)
{
  InitDefaultSettings(pSettings);
  return WriteSettingsToRegistry(hKey, pSettings);
}

BOOL ReadSettingsFromRegistry(PSETTINGS pSettings)
{
  BOOL bReturn;
  LRESULT lResult;
  HKEY hKey;
  DWORD dwType;
  DWORD dwValue;
  DWORD dwBytes;
  SETTINGS settings;

  bReturn = FALSE;

  lResult = OpenSoftwareKey(&hKey);

  assert(LR_SUCCEEDED(lResult));
  if (LR_FAILED(lResult))
    return FALSE;

  dwBytes = sizeof(dwValue);

  lResult = RegQueryValueEx(hKey, L"ShowTrayIcon", NULL, &dwType,
      (PBYTE) &dwValue, &dwBytes);

  assert(LR_SUCCEEDED(lResult) || lResult == ERROR_FILE_NOT_FOUND);
  if (lResult == ERROR_FILE_NOT_FOUND)
  {
    if (WriteDefaultSettings(hKey, pSettings))
    {
      goto loaded;
    }
    else {
      goto finish;
    }
  }
  else if (LR_FAILED(lResult)) {
    goto finish;
  }
  
  if (dwType != REG_DWORD)
    goto finish;

  settings.bShowTrayIcon = dwValue ? TRUE : FALSE;

  dwBytes = sizeof(dwValue);

  lResult = RegQueryValueEx(hKey, L"IconTheme", NULL, &dwType, (PBYTE) &dwValue,
      &dwBytes);

  assert(LR_SUCCEEDED(lResult) || lResult == ERROR_FILE_NOT_FOUND);
  if (lResult == ERROR_FILE_NOT_FOUND)
  {
    if (WriteDefaultSettings(hKey, &settings))
    {
      goto loaded;
    }
    else {
      goto finish;
    }
  }
  else if (LR_FAILED(lResult)) {
    goto finish;
  }

  if (dwType != REG_DWORD)
    goto finish;

  settings.iIconTheme = dwValue;

loaded:
  memcpy_s(pSettings, sizeof(SETTINGS), &settings, sizeof(SETTINGS));

  bReturn = TRUE;

finish:
  RegCloseKey(hKey);
  return bReturn;
}

BOOL AddAutorunEntry()
{
  BOOL bReturn;
  LRESULT lResult;
  HRESULT hResult;
  HKEY hKey;
  WCHAR szPath[MAX_PATH];
  WCHAR szPathQuoted[MAX_PATH];
  DWORD dwWritten;

  bReturn = FALSE;

  dwWritten = GetModuleFileName(NULL, szPath, MAX_PATH);
  assert(dwWritten);
  if (!dwWritten)
    return FALSE;

  hResult = QuoteString(szPathQuoted, MAX_PATH, szPath);

  assert(SUCCEEDED(hResult));
  if (FAILED(hResult))
    goto finish;


  lResult = OpenAutorunKey(&hKey);

  assert(LR_SUCCEEDED(lResult));
  if (LR_FAILED(lResult))
    goto finish;

  lResult = RegSetValueEx(hKey, szAppName, 0, REG_SZ, (BYTE *)szPathQuoted,
      (wcslen(szPathQuoted)+1)*2);
  assert(LR_SUCCEEDED(lResult));
  if (LR_FAILED(lResult))
    goto finish;  

  bReturn = TRUE;

finish:
  RegCloseKey(hKey);
  return bReturn;
}

BOOL RemoveAutorunEntry()
{
  BOOL bResult;
  LRESULT lResult;
  HKEY hKey;

  bResult = FALSE;

  lResult = OpenAutorunKey(&hKey);
  assert(LR_SUCCEEDED(lResult));
  if (LR_FAILED(lResult))
    return FALSE;

  lResult = RegDeleteValue(hKey, szAppName);
  assert(LR_SUCCEEDED(lResult));
  if (LR_FAILED(lResult))
    goto finish;

  bResult = TRUE;

finish:
  RegCloseKey(hKey);
  return bResult;
}

SETTINGS g_settings;

void SaveSettings(PSETTINGS pSettings)
{
  WriteSettingsToRegistryDummy(pSettings);
}

INT_PTR CALLBACK SettingsDlgProc(HWND hWnd, UINT message, WPARAM wParam,
   LPARAM lParam)
{
  UNREFERENCED_PARAMETER(lParam)

  switch (message)
  {
    case WM_INITDIALOG:
      {
        AUTORUNSTATE ars;
        IsAutorunSet(&ars);

        switch (ars)
        {
          case AUTORUN_SET:
            Button_SetCheck(GetDlgItem(hWnd, IDB_AUTORUN), BST_CHECKED);
            break;

          case AUTORUN_DIFFER_LOCATION:
            Button_SetCheck(GetDlgItem(hWnd, IDB_AUTORUN), BST_INDETERMINATE);
            break;

          case AUTORUN_NOT_SET:
          default:
            break;
        }

        ReadSettingsFromRegistry(&g_settings);

        if (g_settings.bShowTrayIcon)
          Button_SetCheck(GetDlgItem(hWnd, IDB_SHOWTRAYICON), BST_CHECKED);

        SendDlgItemMessage(hWnd, IDCB_THEME, CB_ADDSTRING, 0, (LPARAM) L"Auto select");
        SendDlgItemMessage(hWnd, IDCB_THEME, CB_ADDSTRING, 0, (LPARAM) L"Light");
        SendDlgItemMessage(hWnd, IDCB_THEME, CB_ADDSTRING, 0, (LPARAM) L"Dark");

        SendDlgItemMessage(hWnd, IDCB_THEME, CB_SETCURSEL, g_settings.iIconTheme, 0);
      }
      break;

    case WM_COMMAND:
      switch (LOWORD(wParam))
      {
        case IDOK:
          EndDialog(hWnd, IDOK);
          break;

        case IDCANCEL:
          EndDialog(hWnd, IDCANCEL);
          break;

        case IDCB_THEME:
          switch (HIWORD(wParam))
          {
            case CBN_SELCHANGE:
              {
                int iTheme = -1;
                int nSel = SendDlgItemMessage(hWnd, IDCB_THEME, CB_GETCURSEL, 0, 0);

                switch (nSel)
                {
                  case 0:
                    iTheme = ICON_THEME_AUTO;
                    break;
                  case 1:
                    iTheme = ICON_THEME_LIGHT;
                    break;
                  case 2:
                    iTheme = ICON_THEME_DARK;
                    break;
                }

                g_settings.iIconTheme = iTheme;
                WriteSettingsToRegistryDummy(&g_settings);
                ApplyIconTheme();
              }

              break;
          }
          break;

        case IDB_SHOWTRAYICON:
          if (HIWORD(wParam) == BN_CLICKED)
          {
            BOOL bChecked = Button_GetCheck(GetDlgItem(hWnd, IDB_SHOWTRAYICON));

            if (bChecked)
            {
              ShowTrayIcon();
              g_settings.bShowTrayIcon = TRUE;
              WriteSettingsToRegistryDummy(&g_settings);
            }
            else {
              HideTrayIcon();    
              g_settings.bShowTrayIcon = FALSE;
              WriteSettingsToRegistryDummy(&g_settings);
            }
          }
          break;

        case IDB_AUTORUN:
          if (HIWORD(wParam) == BN_CLICKED)
          {
            int iCheck;
            iCheck = Button_GetCheck(GetDlgItem(hWnd, IDB_AUTORUN));

            switch (iCheck)
            {
              case BST_INDETERMINATE:
                {
                  WCHAR szMessage[1024] = { 0 };
                  WCHAR szAutorunPath[MAX_PATH] = { 0 };
                  int iSelect;
                  DWORD dwWritten;

                  GetAutorunPath(szAutorunPath, MAX_PATH, &dwWritten);
                  StringCchPrintf(szMessage, 1024,
                      L"It seems to be application being run from another"
                      L" place. There is already existing autorun entry: %s\n\n"
                      L"What would you like to do?\n"
                      L"Yes - Overwrite autorun entry with this executable\n"
                      L"No - Remove autorun entry\n"
                      L"Cancel - Do nothing", szAutorunPath);

                  iSelect = MessageBox(NULL, szMessage,
                      L"Executable path changed",
                      MB_YESNOCANCEL | MB_ICONWARNING);

                  switch (iSelect)
                  {
                    case IDYES:
                      AddAutorunEntry();
                      Button_SetCheck(GetDlgItem(hWnd, IDB_AUTORUN), BST_CHECKED);
                      break;
                    case IDNO:
                      RemoveAutorunEntry();
                      Button_SetCheck(GetDlgItem(hWnd, IDB_AUTORUN), BST_UNCHECKED);
                      break;
                  }
                }
                break;
              case BST_CHECKED:
                {
                  BOOL bResult;

                  bResult = RemoveAutorunEntry();
                  if (bResult)
                  {
                    Button_SetCheck((HWND) lParam, BST_UNCHECKED);
                  }
                  else {
                    MessageBox(NULL, L"Failed to create autorun entry.", NULL,
                        MB_OK | MB_ICONERROR);
                  }
                }
                break;
              case BST_UNCHECKED:
                {
                  BOOL bResult = AddAutorunEntry();
                  if (bResult)
                  {
                    Button_SetCheck((HWND) lParam, BST_CHECKED);
                  }
                  else {
                    MessageBox(NULL, L"Failed to remove autorun entry.", NULL,
                        MB_OK | MB_ICONERROR);
                  }
                }
                break;
            }
          }
          break;
      }
      break;
    
    case WM_NOTIFY:
      switch (((LPNMHDR)lParam)->code)
      {
        case NM_CLICK:
        case NM_RETURN:
          {
            PNMLINK pNMLink = (PNMLINK)lParam;
            LITEM item = pNMLink->item;  

            ShellExecute(NULL, L"open",
                item.szUrl,
                NULL, NULL, SW_SHOW);
          }
          break;

      }
      break;

    default:
      return FALSE; 
      break;
  }

  return TRUE;
} 

extern inline void OpenSettingsDialog()
{
  HWND hDlg;

  /* Mutex? */
  hDlg = FindWindow(0, L"HideVolumeOSD-C Settings");

  if (hDlg)
    SetActiveWindow(hDlg);
  else
    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SETTINGS),
        hMsgWindow, (DLGPROC) SettingsDlgProc);      
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case HVWM_TRAYICON:
      {
        if (LOWORD(lParam) == WM_CONTEXTMENU)
        {
          HMENU hMenu = LoadMenu(GetModuleHandle(NULL),
              MAKEINTRESOURCE(IDR_TRAYPOPUP));
          hMenu = GetSubMenu(hMenu, 0);

          SetForegroundWindow(hWnd);
          TrackPopupMenu(hMenu, TPM_TOPALIGN | TPM_LEFTALIGN,
              GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam), 0, hWnd, NULL);
        }
      }
      return 0;
      break;
    case WM_COMMAND:
      if (HIWORD(wParam) == 0)
      {
        if (LOWORD(wParam) == IDM_EXIT)
        {
          NOTIFYICONDATA nid = { 0 };
          ZeroMemory(&nid, sizeof(nid));

          nid.cbSize = sizeof(nid);
          nid.hWnd = hMsgWindow;
          nid.uID = 1;

          Shell_NotifyIcon(NIM_DELETE, &nid);

          PostQuitMessage(0);
        }
        else if (LOWORD(wParam) == IDM_SETTINGS)
        {
          OpenSettingsDialog();
        }
        else if (LOWORD(wParam) == IDM_SHOW)
        {
          ShowOSD();
          g_status = STATUS_SHOWN;
          ApplyIconTheme();
        }
        else if (LOWORD(wParam) == IDM_HIDE)
        {
          HideOSD();
          g_status = STATUS_HIDDEN;
          ApplyIconTheme();
        }
      }
      return 0;
      break;
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPWSTR lpCmdLine, int nCmdShow)
{
  UNREFERENCED_PARAMETER(hInstance)
  UNREFERENCED_PARAMETER(hPrevInstance)
  UNREFERENCED_PARAMETER(nCmdShow)

  MSG msg;
  BOOL bResult;
  WNDCLASSEX wcex;
  int nArgs;
  LPWSTR *szArgList;

  if (CheckInstance(szAppMutex))
  {
    MessageBox(NULL, L"Another instance of application is already running.",
        szAppName, MB_OK);
    return -1;
  }

  INITCOMMONCONTROLSEX iccex = { 0 };
  iccex.dwSize = sizeof(iccex);
  iccex.dwICC = ICC_LINK_CLASS;
  InitCommonControlsEx(&iccex);

  szArgList = CommandLineToArgvW(lpCmdLine, &nArgs);

  if (nArgs > 0 && (wcscmp(szArgList[0], L"-show") == 0))
  {
    ShowOSD();
  }
  else {
    HideOSD();
  }

  bResult = ReadSettingsFromRegistry(&g_settings);
  
  assert(bResult);
  if (!bResult)
  {
    MessageBox(NULL, L"Couldn't load settings", NULL, MB_OK | MB_ICONERROR);
    PostQuitMessage(0);
  }

  ZeroMemory(&wcex, sizeof(wcex));
  wcex.cbSize = sizeof(wcex);
  wcex.lpfnWndProc = (WNDPROC) WndProc;
  wcex.hInstance = hInstance;
  wcex.lpszClassName = L"HideVol";

  bResult = RegisterClassEx(&wcex);

  assert(bResult);
  if (!bResult)
  {
    MessageBox(NULL, L"Class registration failed", NULL, MB_OK | MB_ICONERROR);
    PostQuitMessage(0);
  }

  hMsgWindow = CreateWindowEx(0, L"HideVol", L"HideVol Window",
      0, 0, 0, 0, 0,
      HWND_MESSAGE, NULL, hInstance, NULL);
  if (g_settings.bShowTrayIcon)
    ShowTrayIcon();

  ZeroMemory(&msg, sizeof(msg));
  while (GetMessage(&msg, NULL, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return (int) msg.wParam;
}

HWND FindOSDWindow()
{
  HWND hWndRet = NULL;
  HWND hWndHost = NULL;
  int pairCount = 0;

  /* Search for all windows with class "NativeHWNDHost" */
  while ((hWndHost = FindWindowEx(NULL, hWndHost, L"NativeHWNDHost", L""))
      != NULL)
  {
    /*
     * If this window has a child with class "DirectUIHWND" it might be the
     * volume OSD
     */
    if (FindWindowEx(hWndHost, NULL, L"DirectUIHWND", L"") != NULL)
    {
      /* If this is the only pair we are sure */
      if (pairCount == 0)
      {
        hWndRet = hWndHost;
      }
    }

    pairCount++;

    /* If there are more pairs creteria has failed... */
    if (pairCount > 1)
    {
      MessageBox(NULL, L"Severe error: Multiple pairs found!", szAppName,
          MB_OK);
      return NULL;
    }
  }

  /* If no window found yet, there is no OSD window at all */
  if (hWndRet == NULL)
  {
    MessageBox(NULL, L"Severe error: OSD window not found!", szAppName, MB_OK);
  }

  return hWndRet;
}

