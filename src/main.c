// iplookup
// Copyright (c) 2011-2022 Henry++

#include <ws2tcpip.h>
#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <mstcpip.h>

#include "routine.h"

#include "main.h"
#include "rapp.h"

#include "resource.h"

volatile LONG lock_thread = 0;

NTSTATUS NTAPI _app_print (
	_In_ PVOID lparam
)
{
	HWND hwnd;
	WSADATA wsa;
	WCHAR buffer[128];
	PIP_ADAPTER_ADDRESSES adapter_addresses;
	PIP_ADAPTER_ADDRESSES adapter;
	IP_ADAPTER_UNICAST_ADDRESS* address;
	PSOCKADDR_IN ipv4;
	PSOCKADDR_IN6 ipv6;
	ADDRESS_FAMILY af;
	ULONG buffer_length;
	ULONG size;
	ULONG code;
	INT item_id;

	R_DOWNLOAD_INFO download_info;
	HINTERNET hsession;
	PR_STRING url_string;

	hwnd = (HWND)lparam;

	InterlockedIncrement (&lock_thread);

	_r_status_settext (hwnd, IDC_STATUSBAR, 0, L"Loading....");

	_r_listview_deleteallitems (hwnd, IDC_LISTVIEW);

	item_id = 0;

	code = WSAStartup (WINSOCK_VERSION, &wsa);

	if (code == ERROR_SUCCESS)
	{
		size = 0;

		while (TRUE)
		{
			size += 1024;
			adapter_addresses = _r_mem_allocatezero (size);

			code = GetAdaptersAddresses (
				AF_UNSPEC,
				GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME,
				NULL,
				adapter_addresses,
				&size
			);

			if (code == ERROR_SUCCESS)
			{
				break;
			}
			else
			{
				SAFE_DELETE_MEMORY (adapter_addresses);

				if (code == ERROR_BUFFER_OVERFLOW)
					continue;

				break;
			}
		}

		if (adapter_addresses)
		{
			for (adapter = adapter_addresses; adapter != NULL; adapter = adapter->Next)
			{
				if (IF_TYPE_SOFTWARE_LOOPBACK == adapter->IfType)
					continue;

				for (address = adapter->FirstUnicastAddress; address != NULL; address = address->Next)
				{
					af = address->Address.lpSockaddr->sa_family;

					if (af == AF_INET)
					{
						// ipv4
						ipv4 = (PSOCKADDR_IN)address->Address.lpSockaddr;
						buffer_length = RTL_NUMBER_OF (buffer);

						if (NT_SUCCESS (RtlIpv4AddressToStringEx (&(ipv4->sin_addr), 0, buffer, &buffer_length)))
						{
							_r_listview_additem_ex (hwnd, IDC_LISTVIEW, item_id, buffer, I_IMAGENONE, 0, 0);
							_r_listview_setitem (hwnd, IDC_LISTVIEW, item_id, 1, adapter->Description);

							item_id += 1;
						}
					}
					else if (af == AF_INET6)
					{
						// ipv6
						ipv6 = (PSOCKADDR_IN6)address->Address.lpSockaddr;
						buffer_length = RTL_NUMBER_OF (buffer);

						if (NT_SUCCESS (RtlIpv6AddressToStringEx (&(ipv6->sin6_addr), 0, 0, buffer, &buffer_length)))
						{
							_r_listview_additem_ex (hwnd, IDC_LISTVIEW, item_id, buffer, I_IMAGENONE, 1, 0);
							_r_listview_setitem (hwnd, IDC_LISTVIEW, item_id, 1, adapter->Description);

							item_id += 1;
						}
					}
				}
			}

			_r_mem_free (adapter_addresses);
		}

		WSACleanup ();
	}

	if (_r_config_getboolean (L"GetExternalIp", FALSE))
	{
		if (_r_sys_isosversiongreaterorequal (WINDOWS_7))
		{
			url_string = _r_config_getstring (L"ExternalUrl", EXTERNAL_URL);

			if (url_string)
			{
				hsession = _r_inet_createsession (_r_app_getuseragent ());

				if (hsession)
				{
					_r_inet_initializedownload (&download_info);

					if (_r_inet_begindownload (hsession, url_string, &download_info) == ERROR_SUCCESS)
					{
						_r_listview_additem_ex (hwnd, IDC_LISTVIEW, item_id, _r_obj_getstringorempty (download_info.u.string), I_IMAGENONE, 2, 0);
						_r_listview_setitem (hwnd, IDC_LISTVIEW, item_id, 1, url_string->buffer);

						item_id += 1;
					}

					_r_inet_destroydownload (&download_info);

					_r_inet_close (hsession);
				}

				_r_obj_dereference (url_string);
			}
		}
	}

	_r_listview_setcolumn (hwnd, IDC_LISTVIEW, 0, NULL, -40);
	_r_listview_setcolumn (hwnd, IDC_LISTVIEW, 1, NULL, -60);

	_r_status_settextformat (hwnd, IDC_STATUSBAR, 0, _r_locale_getstring (IDS_STATUS), _r_listview_getitemcount (hwnd, IDC_LISTVIEW));

	InterlockedDecrement (&lock_thread);

	return STATUS_SUCCESS;
}

INT_PTR CALLBACK DlgProc (
	_In_ HWND hwnd,
	_In_ UINT msg,
	_In_ WPARAM wparam,
	_In_ LPARAM lparam
)
{
	static R_LAYOUT_MANAGER layout_manager = {0};

	switch (msg)
	{
		case WM_INITDIALOG:
		{
			UINT state_mask;

			// configure listview
			_r_listview_setstyle (hwnd, IDC_LISTVIEW, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP | LVS_EX_LABELTIP, TRUE);

			_r_listview_addcolumn (hwnd, IDC_LISTVIEW, 0, L"", 10, LVCFMT_LEFT);
			_r_listview_addcolumn (hwnd, IDC_LISTVIEW, 1, L"", 10, LVCFMT_LEFT);

			state_mask = 0;

			if (_r_sys_isosversiongreaterorequal (WINDOWS_VISTA))
				state_mask = LVGS_COLLAPSIBLE;

			_r_listview_addgroup (hwnd, IDC_LISTVIEW, 0, L"IPv4", 0, state_mask, state_mask);
			_r_listview_addgroup (hwnd, IDC_LISTVIEW, 1, L"IPv6", 0, state_mask, state_mask);
			_r_listview_addgroup (hwnd, IDC_LISTVIEW, 2, _r_locale_getstring (IDS_GROUP2), 0, state_mask, state_mask);

			_r_layout_initializemanager (&layout_manager, hwnd);

			// refresh list
			PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDM_REFRESH, 0), 0);

			break;
		}

		case RM_INITIALIZE:
		{
			// configure menu
			HMENU hmenu;

			hmenu = GetMenu (hwnd);

			if (hmenu)
			{
				CheckMenuItem (hmenu, IDM_ALWAYSONTOP_CHK, MF_BYCOMMAND | (_r_config_getboolean (L"AlwaysOnTop", FALSE) ? MF_CHECKED : MF_UNCHECKED));
				CheckMenuItem (hmenu, IDM_GETEXTERNALIP_CHK, MF_BYCOMMAND | (_r_config_getboolean (L"GetExternalIp", FALSE) ? MF_CHECKED : MF_UNCHECKED));

				if (!_r_sys_isosversiongreaterorequal (WINDOWS_7))
					_r_menu_enableitem (hmenu, IDM_GETEXTERNALIP_CHK, MF_BYCOMMAND, FALSE);
			}

			break;
		}

		case RM_LOCALIZE:
		{
			HMENU hmenu;

			// configure listview
			_r_listview_setgroup (hwnd, IDC_LISTVIEW, 0, L"IPv4", 0, 0);
			_r_listview_setgroup (hwnd, IDC_LISTVIEW, 1, L"IPv6", 0, 0);
			_r_listview_setgroup (hwnd, IDC_LISTVIEW, 2, _r_locale_getstring (IDS_GROUP2), 0, 0);

			// localize menu
			hmenu = GetMenu (hwnd);

			if (hmenu)
			{
				_r_menu_setitemtext (hmenu, 0, TRUE, _r_locale_getstring (IDS_FILE));
				_r_menu_setitemtext (hmenu, 1, TRUE, _r_locale_getstring (IDS_SETTINGS));
				_r_menu_setitemtext (hmenu, 2, TRUE, _r_locale_getstring (IDS_HELP));
				_r_menu_setitemtextformat (GetSubMenu (hmenu, 1), LANG_MENU, TRUE, L"%s (Language)", _r_locale_getstring (IDS_LANGUAGE));

				_r_menu_setitemtextformat (hmenu, IDM_EXIT, FALSE, L"%s\tEsc", _r_locale_getstring (IDS_EXIT));
				_r_menu_setitemtext (hmenu, IDM_ALWAYSONTOP_CHK, FALSE, _r_locale_getstring (IDS_ALWAYSONTOP_CHK));
				_r_menu_setitemtext (hmenu, IDM_GETEXTERNALIP_CHK, FALSE, _r_locale_getstring (IDS_GETEXTERNALIP_CHK));
				_r_menu_setitemtext (hmenu, IDM_WEBSITE, FALSE, _r_locale_getstring (IDS_WEBSITE));
				_r_menu_setitemtext (hmenu, IDM_ABOUT, FALSE, _r_locale_getstring (IDS_ABOUT));

				_r_locale_enum (GetSubMenu (hmenu, 1), LANG_MENU, IDX_LANGUAGE); // enum localizations
			}

			break;
		}

		case WM_DESTROY:
		{
			PostQuitMessage (0);
			break;
		}

		case WM_SIZE:
		{
			if (!_r_layout_resize (&layout_manager, wparam))
				break;

			_r_listview_setcolumn (hwnd, IDC_LISTVIEW, 0, NULL, -40);
			_r_listview_setcolumn (hwnd, IDC_LISTVIEW, 1, NULL, -60);

			break;
		}

		case WM_GETMINMAXINFO:
		{
			_r_layout_resizeminimumsize (&layout_manager, lparam);
			break;
		}

		case WM_CONTEXTMENU:
		{
			HMENU hmenu;
			HMENU hsubmenu;

			if (GetDlgCtrlID ((HWND)wparam) != IDC_LISTVIEW)
				break;

			hmenu = LoadMenu (NULL, MAKEINTRESOURCE (IDM_LISTVIEW));

			if (!hmenu)
				break;

			hsubmenu = GetSubMenu (hmenu, 0);

			if (hsubmenu)
			{
				// localize
				_r_menu_setitemtextformat (hmenu, IDM_REFRESH, FALSE, L"%s\tF5", _r_locale_getstring (IDS_REFRESH));
				_r_menu_setitemtextformat (hmenu, IDM_COPY, FALSE, L"%s\tCtrl+C", _r_locale_getstring (IDS_COPY));

				if (InterlockedCompareExchange (&lock_thread, 0, 0) != 0)
					_r_menu_enableitem (hsubmenu, IDM_REFRESH, MF_BYCOMMAND, FALSE);

				if (!_r_listview_getselectedcount (hwnd, IDC_LISTVIEW))
					_r_menu_enableitem (hsubmenu, IDM_COPY, MF_BYCOMMAND, FALSE);

				_r_menu_popup (hsubmenu, hwnd, NULL, TRUE);
			}

			DestroyMenu (hmenu);

			break;
		}

		case WM_COMMAND:
		{
			INT ctrl_id = LOWORD (wparam);
			INT notify_code = HIWORD (wparam);

			if (notify_code == 0 && ctrl_id >= IDX_LANGUAGE && ctrl_id <= IDX_LANGUAGE + (INT_PTR)_r_locale_getcount () + 1)
			{
				_r_locale_apply (GetSubMenu (GetSubMenu (GetMenu (hwnd), 1), LANG_MENU), ctrl_id, IDX_LANGUAGE);

				return FALSE;
			}

			switch (ctrl_id)
			{
				case IDCANCEL: // process Esc key
				case IDM_EXIT:
				{
					DestroyWindow (hwnd);
					break;
				}

				case IDM_ALWAYSONTOP_CHK:
				{
					BOOLEAN new_val = !_r_config_getboolean (L"AlwaysOnTop", FALSE);

					CheckMenuItem (GetMenu (hwnd), IDM_ALWAYSONTOP_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					_r_config_setboolean (L"AlwaysOnTop", new_val);

					_r_wnd_top (hwnd, new_val);

					break;
				}

				case IDM_GETEXTERNALIP_CHK:
				{
					BOOLEAN new_val = !_r_config_getboolean (L"GetExternalIp", FALSE);

					CheckMenuItem (GetMenu (hwnd), IDM_GETEXTERNALIP_CHK, MF_BYCOMMAND | (new_val ? MF_CHECKED : MF_UNCHECKED));
					_r_config_setboolean (L"GetExternalIp", new_val);

					PostMessage (hwnd, WM_COMMAND, MAKEWPARAM (IDM_REFRESH, 0), 0);

					break;
				}

				case IDM_WEBSITE:
				{
					ShellExecute (hwnd, NULL, _r_app_getwebsite_url (), NULL, NULL, SW_SHOWDEFAULT);
					break;
				}

				case IDM_ABOUT:
				{
					_r_show_aboutmessage (hwnd);
					break;
				}

				case IDM_REFRESH:
				{
					if (InterlockedCompareExchange (&lock_thread, 0, 0) == 0)
						_r_sys_createthread (&_app_print, hwnd, NULL, NULL, NULL);

					break;
				}

				case IDM_COPY:
				{
					R_STRINGBUILDER buffer;
					PR_STRING string;
					INT item_id;

					item_id = -1;

					_r_obj_initializestringbuilder (&buffer);

					while ((item_id = (INT)SendDlgItemMessage (hwnd, IDC_LISTVIEW, LVM_GETNEXTITEM, item_id, LVNI_SELECTED)) != -1)
					{
						string = _r_listview_getitemtext (hwnd, IDC_LISTVIEW, item_id, 0);

						if (string)
						{
							_r_obj_appendstringbuilder2 (&buffer, string);
							_r_obj_appendstringbuilder (&buffer, L"\r\n");

							_r_obj_dereference (string);
						}
					}

					string = _r_obj_finalstringbuilder (&buffer);

					_r_str_trimstring2 (string, L"\r\n ", 0);

					_r_clipboard_set (hwnd, &string->sr);

					_r_obj_deletestringbuilder (&buffer);

					break;
				}

				case IDM_SELECT_ALL:
				{
					_r_listview_setitemstate (hwnd, IDC_LISTVIEW, -1, LVIS_SELECTED, LVIS_SELECTED);
					break;
				}
			}

			break;
		}
	}

	return FALSE;
}

INT APIENTRY wWinMain (
	_In_ HINSTANCE hinst,
	_In_opt_ HINSTANCE prev_hinst,
	_In_ LPWSTR cmdline,
	_In_ INT show_cmd
)
{
	HWND hwnd;

	if (!_r_app_initialize ())
		return ERROR_APP_INIT_FAILURE;

	hwnd = _r_app_createwindow (hinst, MAKEINTRESOURCE (IDD_MAIN), MAKEINTRESOURCE (IDI_MAIN), &DlgProc);

	if (!hwnd)
		return ERROR_APP_INIT_FAILURE;

	return _r_wnd_message_callback (hwnd, MAKEINTRESOURCE (IDA_MAIN));
}
