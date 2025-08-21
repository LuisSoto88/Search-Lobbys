#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "Comctl32.lib")

#define ID_UPDATE_BUTTON 1001
#define ID_JOIN_BUTTON 1002
#define ID_LISTVIEW 1003
#define ID_GAMEID_EDIT 1004 // ID para el nuevo campo de texto

// Estructura para almacenar los datos de un lobby
struct LobbyData {
    std::string id;
    std::string name;
    std::string players;
    std::string customData;
    std::string link;
};

// Variables globales para los controles de la ventana
HWND g_hListView;
HWND g_hUpdateButton;
HWND g_hJoinButton;
HWND g_hGameIDEdit; // Handle del nuevo campo de texto

// Prototipos de funciones
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateControls(HWND hwnd);
void PopulateListView(const std::vector<LobbyData>& lobbies);
std::vector<LobbyData> ReadLobbyDataFromFile(const std::string& filename);
void HandleItemSelection(HWND hwnd);
void UpdateAppIDFile(); // Nueva función para actualizar el archivo
std::wstring string_to_wstring(const std::string& str);

// Función auxiliar para convertir std::string a std::wstring
std::wstring string_to_wstring(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    if (size_needed <= 0) {
        return L"";
    }
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
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
        MessageBox(NULL, L"Window Registration Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    HWND hwnd = CreateWindowEx(
        0,
        L"LobbySearchApp",
        L"Steam Lobby Search",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!hwnd) {
        MessageBox(NULL, L"Window Creation Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
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
            MoveWindow(g_hListView, 10, 50, width - 30, height - 100, TRUE);
            MoveWindow(g_hGameIDEdit, 10, 10, 100, 30, TRUE);
            MoveWindow(g_hUpdateButton, 120, 10, 100, 30, TRUE);
            MoveWindow(g_hJoinButton, 230, 10, 100, 30, TRUE);
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_UPDATE_BUTTON) {
            EnableWindow(g_hUpdateButton, FALSE);
            EnableWindow(g_hJoinButton, FALSE);

            UpdateAppIDFile(); // Llama a la nueva función

            ShellExecute(NULL, L"open", L"SteamSearchlobby.exe", NULL, NULL, SW_SHOWNORMAL);
            SetTimer(hwnd, 1, 5000, NULL);
        }
        else if (LOWORD(wParam) == ID_JOIN_BUTTON) {
            int selectedRow = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            if (selectedRow != -1) {
                wchar_t joinLink[256];
                LVITEM item = { 0 };
                item.iItem = selectedRow;
                item.iSubItem = 4;
                item.mask = LVIF_TEXT;
                item.pszText = joinLink;
                item.cchTextMax = sizeof(joinLink) / sizeof(wchar_t);
                ListView_GetItem(g_hListView, &item);

                if (wcslen(joinLink) > 0) {
                    ShellExecute(NULL, L"open", joinLink, NULL, NULL, SW_SHOWNORMAL);
                }
            }
        }
        break;

    case WM_TIMER:
        if (wParam == 1) {
            KillTimer(hwnd, 1);
            std::vector<LobbyData> lobbies = ReadLobbyDataFromFile("out.txt");
            PopulateListView(lobbies);
            EnableWindow(g_hUpdateButton, TRUE);
            HandleItemSelection(hwnd);
        }
        break;

    case WM_NOTIFY:
        if ((((LPNMHDR)lParam)->hwndFrom == g_hListView) &&
            (((LPNMHDR)lParam)->code == LVN_ITEMCHANGED))
        {
            HandleItemSelection(hwnd);
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
    // Campo de texto para el AppID
    g_hGameIDEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE, L"EDIT", L"480", // Valor predeterminado
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        10, 10, 100, 30, hwnd, (HMENU)ID_GAMEID_EDIT,
        GetModuleHandle(NULL), NULL);

    // Botón de Actualizar
    g_hUpdateButton = CreateWindowEx(
        0, L"BUTTON", L"Actualizar",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        120, 10, 100, 30, hwnd, (HMENU)ID_UPDATE_BUTTON,
        GetModuleHandle(NULL), NULL);

    // Botón de Unirse (deshabilitado por defecto)
    g_hJoinButton = CreateWindowEx(
        0, L"BUTTON", L"Unirse",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        230, 10, 100, 30, hwnd, (HMENU)ID_JOIN_BUTTON,
        GetModuleHandle(NULL), NULL);
    EnableWindow(g_hJoinButton, FALSE);

    // Control ListView (la tabla)
    g_hListView = CreateWindowEx(
        WS_EX_CLIENTEDGE, WC_LISTVIEW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        10, 50, 760, 500, hwnd, (HMENU)ID_LISTVIEW,
        GetModuleHandle(NULL), NULL);

    LVCOLUMN lvc;
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;

    lvc.iSubItem = 0;
    lvc.cx = 150;
    lvc.pszText = const_cast<LPWSTR>(L"ID del Lobby");
    ListView_InsertColumn(g_hListView, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.cx = 200;
    lvc.pszText = const_cast<LPWSTR>(L"Nombre");
    ListView_InsertColumn(g_hListView, 1, &lvc);

    lvc.iSubItem = 2;
    lvc.cx = 100;
    lvc.pszText = const_cast<LPWSTR>(L"Jugadores");
    ListView_InsertColumn(g_hListView, 2, &lvc);

    lvc.iSubItem = 3;
    lvc.cx = 250;
    lvc.pszText = const_cast<LPWSTR>(L"Datos Personalizados");
    ListView_InsertColumn(g_hListView, 3, &lvc);

    lvc.iSubItem = 4;
    lvc.cx = 0;
    lvc.pszText = const_cast<LPWSTR>(L"Enlace");
    ListView_InsertColumn(g_hListView, 4, &lvc);
}

// Nueva función para actualizar el archivo steam_appid.txt
void UpdateAppIDFile() {
    wchar_t appIDBuffer[256];
    GetWindowText(g_hGameIDEdit, appIDBuffer, 256);
    std::wstring appIDWStr(appIDBuffer);
    std::string appIDStr(appIDWStr.begin(), appIDWStr.end());

    std::ofstream file("steam_appid.txt");
    if (file.is_open()) {
        file << appIDStr;
        file.close();
    }
    else {
        MessageBox(NULL, L"No se pudo escribir en steam_appid.txt", L"Error", MB_ICONERROR | MB_OK);
    }
}

void PopulateListView(const std::vector<LobbyData>& lobbies) {
    ListView_DeleteAllItems(g_hListView);

    for (size_t i = 0; i < lobbies.size(); ++i) {
        std::wstring w_id = string_to_wstring(lobbies[i].id);
        std::wstring w_name = string_to_wstring(lobbies[i].name);
        std::wstring w_players = string_to_wstring(lobbies[i].players);
        std::wstring w_customData = string_to_wstring(lobbies[i].customData);
        std::wstring w_link = string_to_wstring(lobbies[i].link);

        LVITEM item = { 0 };
        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.iSubItem = 0;
        item.pszText = const_cast<LPWSTR>(w_id.c_str());
        ListView_InsertItem(g_hListView, &item);

        ListView_SetItemText(g_hListView, i, 1, const_cast<LPWSTR>(w_name.c_str()));
        ListView_SetItemText(g_hListView, i, 2, const_cast<LPWSTR>(w_players.c_str()));
        ListView_SetItemText(g_hListView, i, 3, const_cast<LPWSTR>(w_customData.c_str()));
        ListView_SetItemText(g_hListView, i, 4, const_cast<LPWSTR>(w_link.c_str()));
    }
}

std::vector<LobbyData> ReadLobbyDataFromFile(const std::string& filename) {
    std::vector<LobbyData> lobbies;
    std::ifstream file(filename);

    if (!file.is_open()) {
        MessageBox(NULL, L"No se pudo abrir el archivo out.txt. Asegúrese de que existe.", L"Error", MB_ICONERROR | MB_OK);
        return lobbies;
    }

    std::string line;
    LobbyData currentLobby;
    bool inLobbyBlock = false;
    bool inCustomDataBlock = false;

    currentLobby.id = "None";
    currentLobby.name = "None";
    currentLobby.players = "None";
    currentLobby.customData = "None";
    currentLobby.link = "None";

    while (getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        line.erase(line.find_last_not_of(" \t\n\r") + 1);

        if (line.find("=== Lobby [") != std::string::npos) {
            if (inLobbyBlock) {
                lobbies.push_back(currentLobby);
            }
            currentLobby = {};
            inLobbyBlock = true;
            inCustomDataBlock = false;

            currentLobby.id = "None";
            currentLobby.name = "None";
            currentLobby.players = "None";
            currentLobby.customData = "None";
            currentLobby.link = "None";
        }
        else if (inLobbyBlock) {
            if (line.find("ID:") != std::string::npos) {
                currentLobby.id = line.substr(line.find("ID:") + 4);
                inCustomDataBlock = false;
            }
            else if (line.find("Nombre:") != std::string::npos) {
                currentLobby.name = line.substr(line.find("Nombre:") + 8);
                inCustomDataBlock = false;
            }
            else if (line.find("Jugadores:") != std::string::npos) {
                currentLobby.players = line.substr(line.find("Jugadores:") + 11);
                inCustomDataBlock = false;
            }
            else if (line.find("Datos personalizados:") != std::string::npos) {
                inCustomDataBlock = true;
                currentLobby.customData.clear();
            }
            else if (line.find("Enlace:") != std::string::npos) {
                currentLobby.link = line.substr(line.find("Enlace:") + 8);
                inCustomDataBlock = false;
                lobbies.push_back(currentLobby);
                inLobbyBlock = false;
            }
            else if (inCustomDataBlock && !line.empty()) {
                if (currentLobby.customData == "None") {
                    currentLobby.customData = line;
                }
                else {
                    currentLobby.customData += "; " + line;
                }
            }
        }
    }

    if (inLobbyBlock) {
        lobbies.push_back(currentLobby);
    }

    file.close();
    return lobbies;
}

void HandleItemSelection(HWND hwnd) {
    int selectedCount = ListView_GetSelectedCount(g_hListView);
    EnableWindow(g_hJoinButton, selectedCount > 0);
}