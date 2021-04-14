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

const WCHAR szAppName[] = L"HideVolumeOSD-C";
const WCHAR szAppMutex[] = L"{F95D0266-5DA1-4701-8188-78F17E2F98F}";

HWND hWndInject;

void Init();
void HideOSD();
void ShowOSD();
HWND FindOSDWindow();
BOOL CheckInstance(LPCWSTR);

void ShowOSD()
{
  if (!IsWindow(hWndInject))
  {
    Init();
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
    Init();
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

void Init()
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
    ExitProcess(-1);
    return;
  }
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPWSTR lpCmdLine, int nCmdShow)
{
  UNREFERENCED_PARAMETER(hInstance)
  UNREFERENCED_PARAMETER(hPrevInstance)
  UNREFERENCED_PARAMETER(nCmdShow)

  if (CheckInstance(szAppMutex))
  {
    MessageBox(NULL, L"Another instance of application is already running.",
        szAppName, MB_OK);
    return -1;
  }

  int nArgs;
  LPWSTR *szArgList = CommandLineToArgvW(lpCmdLine, &nArgs);

  if (nArgs > 0 && (wcscmp(szArgList[0], L"-show") == 0))
  {
    ShowOSD();
  }
  else {
    HideOSD();
  }

  return 0;
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

