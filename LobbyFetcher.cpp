#include "steam/steam_api.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>

using namespace std;

// Strict ASCII Sanitization - removes everything outside printable ASCII
string Sanitize(const string& input) {
    string safe;
    for (char c : input) {
        unsigned char uc = (unsigned char)c;
        // Only allow printable ASCII, but EXCLUDE pipe character to avoid breaking delimiter
        if (uc >= 32 && uc <= 126 && c != '|') {
            safe += c;
        }
    }
    return safe;
}

bool g_Done = false;

class LobbySearcher {
public:
    void FindLobbies() {
        SteamAPICall_t hSteamAPICall = SteamMatchmaking()->RequestLobbyList();
        m_CallResultLobbyMatchList.Set(hSteamAPICall, this, &LobbySearcher::OnLobbyMatchList);
    }

    void OnLobbyMatchList(LobbyMatchList_t* pLobbyMatchList, bool bIOFailure) {
        if (bIOFailure) {
            g_Done = true;
            return;
        }

        uint32 count = pLobbyMatchList->m_nLobbiesMatching;
        ofstream file("out.txt");
        if (!file.is_open()) {
            g_Done = true;
            return;
        }

        // PIPE-DELIMITED FORMAT: ID|Players|Data|Link
        for (uint32 i = 0; i < count; ++i) {
            CSteamID lobbyID = SteamMatchmaking()->GetLobbyByIndex(i);
            
            // 1. ID
            string sID = to_string(lobbyID.ConvertToUint64());
            
            // 2. Players
            int cMembers = SteamMatchmaking()->GetNumLobbyMembers(lobbyID);
            int cLimit = SteamMatchmaking()->GetLobbyMemberLimit(lobbyID);
            string sPlayers = to_string(cMembers) + " / " + (cLimit > 0 ? to_string(cLimit) : "?");

            // 3. Custom Data (flatten to single line, use space separator instead of pipe)
            string sCustomData = "";
            int dataCount = SteamMatchmaking()->GetLobbyDataCount(lobbyID);
            for(int k=0; k<dataCount; ++k) {
                char key[256];
                char val[4096];
                if(SteamMatchmaking()->GetLobbyDataByIndex(lobbyID, k, key, 256, val, 4096)) {
                    string sKey = Sanitize(key);
                    string sVal = Sanitize(val);
                    if (!sKey.empty() && !sVal.empty()) {
                        if (!sCustomData.empty()) sCustomData += " ";
                        sCustomData += sKey + ":" + sVal;
                    }
                }
            }
            if (sCustomData.empty()) sCustomData = "n/a";
            // Truncate if too long to prevent display issues
            if (sCustomData.length() > 500) {
                sCustomData = sCustomData.substr(0, 500) + "...";
            }

            // 4. Link
            string sLink = "steam://joinlobby/480/" + sID + "/" + to_string(SteamUser()->GetSteamID().ConvertToUint64());

            // WRITE PIPE-DELIMITED LINE
            file << sID << "|" << sPlayers << "|" << sCustomData << "|" << sLink << "\n";
        }

        file.close();
        g_Done = true;
    }

private:
    CCallResult<LobbySearcher, LobbyMatchList_t> m_CallResultLobbyMatchList;
};

LobbySearcher g_searcher;

int main() {
    if (!SteamAPI_Init()) {
        return 1;
    }
    
    SteamMatchmaking()->AddRequestLobbyListDistanceFilter(k_ELobbyDistanceFilterWorldwide);
    SteamMatchmaking()->AddRequestLobbyListResultCountFilter(20000);

    g_searcher.FindLobbies();

    while (!g_Done) {
        SteamAPI_RunCallbacks();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    SteamAPI_Shutdown();
    return 0;
}
