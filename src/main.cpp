// Ip Lookup
// Copyright (c) 2011-2018 Henry++

#include <ws2tcpip.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>

#include "main.hpp"
#include "rapp.hpp"
#include "routine.hpp"

#include "resource.hpp"

rapp app (APP_NAME, APP_NAME_SHORT, APP_VERSION, APP_COPYRIGHT);

_R_FASTLOCK lock;

UINT WINAPI _app_print (LPVOID lparam)
{
	HWND hwnd = (HWND)lparam;

	_r_fastlock_acquireexclusive (&lock);

	SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_DELETEALLITEMS, 0, 0);

	WSADATA wsa = {0};
	if (WSAStartup (MAKEWORD (2, 2), &wsa) == ERROR_SUCCESS)
	{
		PIP_ADAPTER_ADDRESSES adapter_addresses = nullptr;
		PIP_ADAPTER_ADDRESSES adapter = nullptr;

		ULONG size = 0;

		rstring buffer;

		while (true)
		{
			size += _R_BUFFER_LENGTH;

			adapter_addresses = new IP_ADAPTER_ADDRESSES[size];

			const DWORD error = GetAdaptersAddresses (AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME, nullptr, adapter_addresses, &size);

			if (error == ERROR_SUCCESS)
			{
				break;
			}
			else
			{
				delete[] adapter_addresses;
				adapter_addresses = nullptr;

				if (error == ERROR_BUFFER_OVERFLOW)
					continue;

				break;
			}
		}

		if (adapter_addresses)
		{
			for (adapter = adapter_addresses; adapter != nullptr; adapter = adapter->Next)
			{
				if (IF_TYPE_SOFTWARE_LOOPBACK == adapter->IfType)
				{
					continue;
				}

				for (IP_ADAPTER_UNICAST_ADDRESS* address = adapter->FirstUnicastAddress; address != nullptr; address = address->Next)
				{
					auto family = address->Address.lpSockaddr->sa_family;

					if (family == AF_INET)
					{
						// ipv4
						PSOCKADDR_IN ipv4 = (PSOCKADDR_IN)(address->Address.lpSockaddr);

						InetNtop (AF_INET, &(ipv4->sin_addr), buffer.GetBuffer (INET_ADDRSTRLEN), INET_ADDRSTRLEN);
						buffer.ReleaseBuffer ();

						_r_listview_additem (hwnd, IDC_LISTVIEW, LAST_VALUE, 0, buffer, LAST_VALUE, 0);
					}
					else if (family == AF_INET6)
					{
						// ipv6
						PSOCKADDR_IN6 ipv6 = (PSOCKADDR_IN6)(address->Address.lpSockaddr);

						InetNtop (AF_INET6, &(ipv6->sin6_addr), buffer.GetBuffer (INET6_ADDRSTRLEN), INET6_ADDRSTRLEN);
						buffer.ReleaseBuffer ();

						_r_listview_additem (hwnd, IDC_LISTVIEW, LAST_VALUE, 0, buffer, LAST_VALUE, 0);
					}
					else
					{
						continue;
					}
				}
			}

			delete[] adapter_addresses;
			adapter_addresses = nullptr;
		}

		WSACleanup ();
	}

	if (app.ConfigGet (L"GetExternalIp", false).AsBool ())
	{
		HINTERNET hsession = _r_inet_createsession (app.GetUserAgent ());

		if (hsession)
		{
			HINTERNET hconnect = nullptr;
			HINTERNET hrequest = nullptr;

			if (_r_inet_openurl (hsession, app.ConfigGet (L"ExternalUrl", EXTERNAL_URL), &hconnect, &hrequest, nullptr))
			{
				CHAR buffera[1024] = {0};
				DWORD total_length = 0;
				rstring bufferw;

				while (true)
				{
					if (!_r_inet_readrequest (hrequest, buffera, _countof (buffera) - 1, &total_length))
						break;

					bufferw.Append (buffera);
				}

				bufferw.Trim (L" \r\n");

				_r_listview_additem (hwnd, IDC_LISTVIEW, LAST_VALUE, 0, bufferw, LAST_VALUE, 1);
			}

			if (hrequest)
				_r_inet_close (hrequest);

			if (hconnect)
				_r_inet_close (hconnect);

			_r_inet_close (hsession);
		}
	}

	_r_status_settext (hwnd, IDC_STATUSBAR, 0, _r_fmt (I18N (&app, IDS_STATUS, 0), _r_listview_getitemcount (hwnd, IDC_LISTVIEW)));

	_r_fastlock_releaseexclusive (&lock);

	return ERROR_SUCCESS;
}

BOOL initializer_callback (HWND hwnd, DWORD msg, LPVOID, LPVOID)
{
	switch (msg)
	{
		case _RM_LOCALIZE:
		{
			// configure listview
			_r_listview_deleteallgroups (hwnd, IDC_LISTVIEW);

			_r_listview_addgroup (hwnd, IDC_LISTVIEW, 0, I18N (&app, IDS_GROUP1, 0), 0, 0);
			_r_listview_addgroup (hwnd, IDC_LISTVIEW, 1, I18N (&app, IDS_GROUP2, 0), 0, 0);

			// localize
			HMENU menu = GetMenu (hwnd);

			app.LocaleMenu (menu, I18N (&app, IDS_FILE, 0), 0, true, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_EXIT, 0), IDM_EXIT, false, L"\tEsc");
			app.LocaleMenu (menu, I18N (&app, IDS_SETTINGS, 0), 1, true, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_ALWAYSONTOP_CHK, 0), IDM_ALWAYSONTOP_CHK, false, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_CHECKUPDATES_CHK, 0), IDM_CHECKUPDATES_CHK, false, nullptr);
			app.LocaleMenu (GetSubMenu (menu, 1), I18N (&app, IDS_LANGUAGE, 0), 5, true, L" (Language)");
			app.LocaleMenu (menu, I18N (&app, IDS_GETEXTERNALIP_CHK, 0), IDM_GETEXTERNALIP_CHK, false, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_HELP, 0), 2, true, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_WEBSITE, 0), IDM_WEBSITE, false, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_CHECKUPDATES, 0), IDM_CHECKUPDATES, false, nullptr);
			app.LocaleMenu (menu, I18N (&app, IDS_ABOUT, 0), IDM_ABOUT, false, nullptr);

			DrawMenuBar (hwnd); // redraw menu

			// refresh list
			SendMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDM_REFRESH, 0), 0);

			app.LocaleEnum ((HWND)GetSubMenu (menu, 1), 5, true, IDM_DEFAULT); // enum localizations

			break;
		}

		case _RM_UNINITIALIZE:
		{
			break;
		}
	}

	return FALSE;
}

void ResizeWindow (HWND hwnd, INT, INT)
{
	RECT rc = {0};

	GetClientRect (GetDlgItem (hwnd, IDC_STATUSBAR), &rc);
	const INT statusbar_height = _R_RECT_HEIGHT (&rc);

	GetClientRect (hwnd, &rc);
	SetWindowPos (GetDlgItem (hwnd, IDC_LISTVIEW), nullptr, 0, 0, _R_RECT_WIDTH (&rc), _R_RECT_HEIGHT (&rc) - statusbar_height, SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);

	_r_listview_setcolumn (hwnd, IDC_LISTVIEW, 0, nullptr, _R_RECT_WIDTH (&rc));

	SendDlgItemMessage (hwnd, IDC_STATUSBAR, WM_SIZE, 0, 0);
}

INT_PTR CALLBACK DlgProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			// configure listview
			_r_listview_setstyle (hwnd, IDC_LISTVIEW, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP);

			_r_listview_addcolumn (hwnd, IDC_LISTVIEW, 0, nullptr, 95, LVCFMT_LEFT);

			// configure menu
			CheckMenuItem (GetMenu (hwnd), IDM_ALWAYSONTOP_CHK, MF_BYCOMMAND | (app.ConfigGet (L"AlwaysOnTop", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (GetMenu (hwnd), IDM_CHECKUPDATES_CHK, MF_BYCOMMAND | (app.ConfigGet (L"CheckUpdates", true).AsBool () ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem (GetMenu (hwnd), IDM_GETEXTERNALIP_CHK, MF_BYCOMMAND | (app.ConfigGet (L"GetExternalIp", false).AsBool () ? MF_CHECKED : MF_UNCHECKED));

			break;
		}

		case WM_DESTROY:
		{
			PostQuitMessage (0);
			break;
		}

		case WM_QUERYENDSESSION:
		{
			SetWindowLongPtr (hwnd, DWLP_MSGRESULT, TRUE);
			return TRUE;
		}

		case WM_SIZE:
		{
			ResizeWindow (hwnd, LOWORD (lparam), HIWORD (lparam));
			RedrawWindow (hwnd, nullptr, nullptr, RDW_ALLCHILDREN | RDW_ERASE | RDW_INVALIDATE);

			break;
		}

		case WM_CONTEXTMENU:
		{
			if (GetDlgCtrlID ((HWND)wparam) == IDC_LISTVIEW)
			{
				const HMENU menu = LoadMenu (nullptr, MAKEINTRESOURCE (IDM_LISTVIEW));
				const HMENU submenu = GetSubMenu (menu, 0);

				// localize
				app.LocaleMenu (submenu, I18N (&app, IDS_REFRESH, 0), IDM_REFRESH, false, L"\tF5");
				app.LocaleMenu (submenu, I18N (&app, IDS_COPY, 0), IDM_COPY, false, L"\tCtrl+C");

				if (_r_fastlock_islocked (&lock))
					EnableMenuItem (submenu, IDM_REFRESH, MF_BYCOMMAND | MF_DISABLED);

				if (!SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETSELECTEDCOUNT, 0, 0))
					EnableMenuItem (submenu, IDM_COPY, MF_BYCOMMAND | MF_DISABLED);

				POINT pt = {0};
				GetCursorPos (&pt);

				TrackPopupMenuEx (submenu, TPM_RIGHTBUTTON | TPM_LEFTBUTTON, pt.x, pt.y, hwnd, nullptr);

				DestroyMenu (menu);
			}

			break;
		}

		case WM_NOTIFY:
		{
			LPNMHDR nmlp = (LPNMHDR)lparam;

			switch (nmlp->code)
			{
				case LVN_DELETEALLITEMS:
				case LVN_INSERTITEM:
				case LVN_DELETEITEM:
				{
					RECT rc = {0};
					GetWindowRect (GetDlgItem (hwnd, IDC_LISTVIEW), &rc);

					_r_listview_setcolumn (hwnd, IDC_LISTVIEW, 0, nullptr, _R_RECT_WIDTH (&rc));

					break;
				}
			}

			break;
		}

		case WM_COMMAND:
		{
			if (HIWORD (wparam) == 0 && LOWORD (wparam) >= IDM_DEFAULT && LOWORD (wparam) <= IDM_DEFAULT + app.LocaleGetCount ())
			{
				app.LocaleApplyFromMenu (GetSubMenu (GetSubMenu (GetMenu (hwnd), 1), 5), LOWORD (wparam), IDM_DEFAULT);

				return FALSE;
			}

			switch (LOWORD (wparam))
			{
				case IDCANCEL: // process Esc key
				case IDM_EXIT:
				{
					DestroyWindow (hwnd);
					break;
				}

				case IDM_ALWAYSONTOP_CHK:
				{
					bool val = app.ConfigGet (L"AlwaysOnTop", false).AsBool ();

					CheckMenuItem (GetMenu (hwnd), IDM_ALWAYSONTOP_CHK, MF_BYCOMMAND | (!val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"AlwaysOnTop", !val);

					_r_wnd_top (hwnd, !val);

					break;
				}

				case IDM_CHECKUPDATES_CHK:
				{
					bool val = app.ConfigGet (L"CheckUpdates", true).AsBool ();

					CheckMenuItem (GetMenu (hwnd), IDM_CHECKUPDATES_CHK, MF_BYCOMMAND | (!val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"CheckUpdates", !val);

					break;
				}

				case IDM_GETEXTERNALIP_CHK:
				{
					bool val = app.ConfigGet (L"GetExternalIp", false).AsBool ();

					CheckMenuItem (GetMenu (hwnd), IDM_GETEXTERNALIP_CHK, MF_BYCOMMAND | (!val ? MF_CHECKED : MF_UNCHECKED));
					app.ConfigSet (L"GetExternalIp", !val);

					SendMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDM_REFRESH, 0), 0);

					break;
				}

				case IDM_WEBSITE:
				{
					ShellExecute (hwnd, nullptr, _APP_WEBSITE_URL, nullptr, nullptr, SW_SHOWDEFAULT);
					break;
				}

				case IDM_CHECKUPDATES:
				{
					app.CheckForUpdates (false);
					break;
				}

				case IDM_ABOUT:
				{
					app.CreateAboutWindow (hwnd, I18N (&app, IDS_DONATE, 0));
					break;
				}

				case IDM_REFRESH:
				{
					_beginthreadex (nullptr, 0, &_app_print, hwnd, 0, nullptr);
					break;
				}

				case IDM_COPY:
				{
					rstring buffer;

					INT item = -1;

					while ((item = (INT)SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETNEXTITEM, item, LVNI_SELECTED)) != -1)
					{
						buffer.Append (_r_listview_getitemtext (hwnd, IDC_LISTVIEW, item, 0));
						buffer.Append (L"\r\n");
					}

					if (!buffer.IsEmpty ())
					{
						buffer.Trim (L"\r\n");

						_r_clipboard_set (hwnd, buffer, buffer.GetLength ());
					}

					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

INT APIENTRY wWinMain (HINSTANCE, HINSTANCE, LPWSTR, INT)
{
	MSG msg = {0};

	if (app.CreateMainWindow (&DlgProc, &initializer_callback))
	{
		const HACCEL haccel = LoadAccelerators (app.GetHINSTANCE (), MAKEINTRESOURCE (IDA_MAIN));

		while (GetMessage (&msg, nullptr, 0, 0) > 0)
		{
			if (haccel)
				TranslateAccelerator (app.GetHWND (), haccel, &msg);

			if (!IsDialogMessage (app.GetHWND (), &msg))
			{
				TranslateMessage (&msg);
				DispatchMessage (&msg);
			}
		}

		if (haccel)
			DestroyAcceleratorTable (haccel);
	}

	return (INT)msg.wParam;
}
