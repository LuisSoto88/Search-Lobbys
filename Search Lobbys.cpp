#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "Comctl32.lib")

#define ID_UPDATE_BUTTON 1001
#define ID_JOIN_BUTTON 1002
#define ID_LISTVIEW 1003
#define ID_GAMEID_EDIT 1004

struct LobbyData {
    std::string id;
    std::string players;
    std::string customData;
    std::string link;
};

HWND g_hListView;
HWND g_hUpdateButton;
HWND g_hJoinButton;
HWND g_hGameIDEdit;

std::vector<LobbyData> g_lobbies;
int g_SortColumn = -1;
bool g_SortAscending = true;

std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Force clean conversion - only allow printable ASCII
std::wstring string_to_wstring(const std::string& str) {
    if (str.empty()) return L"";
    
    // Create clean string with only printable ASCII
    std::string clean;
    for (char c : str) {
        unsigned char uc = (unsigned char)c;
        if (uc >= 32 && uc <= 126) {
            clean += c;
        }
    }
    
    if (clean.empty()) return L"n/a";
    
    int size_needed = MultiByteToWideChar(CP_ACP, 0, clean.c_str(), (int)clean.size(), NULL, 0);
    if (size_needed > 0) {
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_ACP, 0, clean.c_str(), (int)clean.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }
    return L"Error";
}

// Parse pipe-delimited format: ID|Players|Data|Link
void LoadLobbiesFromFile() {
    g_lobbies.clear();
    std::ifstream file("out.txt", std::ios::binary); // Read as binary to avoid encoding issues
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty()) continue;

        // Split by pipe
        std::vector<std::string> parts;
        std::stringstream ss(line);
        std::string part;
        while (std::getline(ss, part, '|')) {
            parts.push_back(Trim(part));
        }

        if (parts.size() >= 4) {
            LobbyData lobby;
            lobby.id = parts[0];
            lobby.players = parts[1];
            lobby.customData = parts[2];
            lobby.link = parts[3];
            g_lobbies.push_back(lobby);
        }
    }
    file.close();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateControls(HWND hwnd);
void PopulateListView();
void HandleItemSelection(HWND hwnd);
void UpdateAppIDFile();

int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
    if (lParam1 >= (LPARAM)g_lobbies.size() || lParam2 >= (LPARAM)g_lobbies.size()) return 0;
    const LobbyData& d1 = g_lobbies[(size_t)lParam1];
    const LobbyData& d2 = g_lobbies[(size_t)lParam2];
    std::string s1, s2;
    switch(lParamSort) {
        case 0: s1 = d1.id; s2 = d2.id; break;
        case 1: s1 = d1.players; s2 = d2.players; break;
        case 2: s1 = d1.customData; s2 = d2.customData; break;
        case 3: s1 = d1.link; s2 = d2.link; break;
        default: return 0;
    }
    int res = _stricmp(s1.c_str(), s2.c_str());
    return g_SortAscending ? res : -res;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdCmdLine, int nCmdShow) {
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"LobbySearchApp";

    if (!RegisterClassEx(&wc)) {
        return 0;
    }

    HWND hwnd = CreateWindowEx(
        0, L"LobbySearchApp", L"Steam Lobby Search",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        CreateControls(hwnd);
        break;
    case WM_SIZE:
        if (g_hListView) {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            MoveWindow(g_hListView, 10, 50, width - 20, height - 70, TRUE);
            MoveWindow(g_hGameIDEdit, 10, 10, 100, 30, TRUE);
            MoveWindow(g_hUpdateButton, 120, 10, 100, 30, TRUE);
            MoveWindow(g_hJoinButton, 230, 10, 100, 30, TRUE);
        }
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_UPDATE_BUTTON) {
            UpdateAppIDFile();

            EnableWindow(g_hUpdateButton, FALSE);
            
            SHELLEXECUTEINFO sei = {0};
            sei.cbSize = sizeof(SHELLEXECUTEINFO);
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.hwnd = hwnd;
            sei.lpVerb = L"open";
            sei.lpFile = L"LobbyFetcher.exe";
            sei.nShow = SW_SHOWNORMAL; 

            if (ShellExecuteEx(&sei)) {
                WaitForSingleObject(sei.hProcess, INFINITE);
                CloseHandle(sei.hProcess);
                
                LoadLobbiesFromFile();
                PopulateListView();
            } else {
                MessageBox(hwnd, L"Error: LobbyFetcher.exe not found or failed.", L"Error", MB_ICONERROR | MB_OK);
            }

            EnableWindow(g_hUpdateButton, TRUE);
        }
        else if (LOWORD(wParam) == ID_JOIN_BUTTON) {
            int selectedRow = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            if (selectedRow != -1) {
                wchar_t joinLink[1024];
                ListView_GetItemText(g_hListView, selectedRow, 3, joinLink, 1024);
                if (wcslen(joinLink) > 0 && wcscmp(joinLink, L"n/a") != 0 && wcscmp(joinLink, L"Error") != 0) {
                    ShellExecute(NULL, L"open", joinLink, NULL, NULL, SW_SHOWNORMAL);
                }
            }
        }
        break;
    case WM_NOTIFY:
        {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            if (pnmh->hwndFrom == g_hListView) {
                if (pnmh->code == LVN_ITEMCHANGED) {
                    HandleItemSelection(hwnd);
                }
                else if (pnmh->code == LVN_COLUMNCLICK) {
                    LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                    if (g_SortColumn == pnmv->iSubItem) {
                        g_SortAscending = !g_SortAscending;
                    } else {
                        g_SortColumn = pnmv->iSubItem;
                        g_SortAscending = true;
                    }
                    ListView_SortItems(g_hListView, CompareFunc, (LPARAM)g_SortColumn);
                }
            }
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void CreateControls(HWND hwnd) {
    g_hGameIDEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"480", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 10, 10, 100, 30, hwnd, (HMENU)ID_GAMEID_EDIT, GetModuleHandle(NULL), NULL);
    g_hUpdateButton = CreateWindowEx(0, L"BUTTON", L"Update", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 120, 10, 100, 30, hwnd, (HMENU)ID_UPDATE_BUTTON, GetModuleHandle(NULL), NULL);
    g_hJoinButton = CreateWindowEx(0, L"BUTTON", L"Join", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 230, 10, 100, 30, hwnd, (HMENU)ID_JOIN_BUTTON, GetModuleHandle(NULL), NULL);
    EnableWindow(g_hJoinButton, FALSE);

    g_hListView = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL, 10, 50, 960, 600, hwnd, (HMENU)ID_LISTVIEW, GetModuleHandle(NULL), NULL);
    SendMessage(g_hListView, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    LVCOLUMN lvc;
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

    const wchar_t* headers[] = { L"ID", L"Players", L"Lobby Data", L"Join Link" };
    int colWidths[] = { 150, 80, 500, 200 };
    
    for (int i = 0; i < 4; i++) {
        lvc.iSubItem = i;
        lvc.pszText = (LPWSTR)headers[i];
        lvc.fmt = (i == 1) ? LVCFMT_CENTER : LVCFMT_LEFT;
        lvc.cx = colWidths[i]; 
        ListView_InsertColumn(g_hListView, i, &lvc);
    }
}

void UpdateAppIDFile() {
    wchar_t appIDBuffer[256];
    GetWindowText(g_hGameIDEdit, appIDBuffer, 256);
    
    std::wstring wAppID(appIDBuffer);
    std::string sAppID;
    if (!wAppID.empty()) {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wAppID[0], (int)wAppID.size(), NULL, 0, NULL, NULL);
        sAppID.assign(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wAppID[0], (int)wAppID.size(), &sAppID[0], size_needed, NULL, NULL);
    }

    std::ofstream file("steam_appid.txt");
    if (file.is_open()) {
        file << sAppID;
        file.close();
    }
}

void PopulateListView() {
    ListView_DeleteAllItems(g_hListView);
    for (size_t i = 0; i < g_lobbies.size(); ++i) {
        LVITEM item = { 0 };
        item.mask = LVIF_TEXT | LVIF_PARAM; 
        item.iItem = (int)i;
        item.iSubItem = 0;
        item.lParam = (LPARAM)i;
        
        std::wstring w_id = string_to_wstring(g_lobbies[i].id);
        item.pszText = (LPWSTR)w_id.c_str();
        ListView_InsertItem(g_hListView, &item);
        
        // Use direct wstring conversion for each column
        std::wstring w_players = string_to_wstring(g_lobbies[i].players);
        std::wstring w_data = string_to_wstring(g_lobbies[i].customData);
        std::wstring w_link = string_to_wstring(g_lobbies[i].link);
        
        ListView_SetItemText(g_hListView, i, 1, (LPWSTR)w_players.c_str());
        ListView_SetItemText(g_hListView, i, 2, (LPWSTR)w_data.c_str());
        ListView_SetItemText(g_hListView, i, 3, (LPWSTR)w_link.c_str());
    }

    for (int col = 0; col < 4; col++) {
        ListView_SetColumnWidth(g_hListView, col, LVSCW_AUTOSIZE);
        int contentWidth = ListView_GetColumnWidth(g_hListView, col);
        ListView_SetColumnWidth(g_hListView, col, LVSCW_AUTOSIZE_USEHEADER);
        int headerWidth = ListView_GetColumnWidth(g_hListView, col);
        ListView_SetColumnWidth(g_hListView, col, (contentWidth > headerWidth) ? contentWidth : headerWidth);
    }
}

void HandleItemSelection(HWND hwnd) {
    int selectedCount = ListView_GetSelectedCount(g_hListView);
    EnableWindow(g_hJoinButton, selectedCount > 0);
}
