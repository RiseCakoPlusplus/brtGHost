/*
    Copyright [2011] [Igor Bygaev] [http://brt.org.ua]
	
	email: avon.dn.ua@gmail.com
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#ifndef SERVER_H
#define SERVER_H

class CPUBProtocol;
class CCommandPacket;
class CTCPClient;
class CTCPServer;
class CTCPSocket;
class CUser;
class CUpdater;

uint32_t GetTime( );


class CGameMsg
{
    private:
        string m_From;
        string m_Msg;

    public:
        CGameMsg(string nFrom, string nMsg) { m_From = nFrom; m_Msg = nMsg;}

        string GetFrom() { return m_From; }
        string GetMsg() { return m_Msg; }
};

class CGamePlayer
{
    private:
        string m_PlayerName;
        unsigned char m_Team;

    public:
        CGamePlayer(string nPlayerName, unsigned char nTeam) { m_PlayerName = nPlayerName; m_Team = nTeam; }

    string GetName() { return m_PlayerName; }
    unsigned char GetTeam() { return m_Team; }
};

class CGame
{
    private:
        string m_GameName;
        uint32_t m_HostCounter;

        bool m_Started;

        uint32_t m_CreatedTime;
        uint32_t m_StartedTime;

    public:
        CGame(string nGameName, uint32_t nHostCounter) { m_GameName = nGameName; m_HostCounter = nHostCounter; m_CreatedTime = GetTime(); m_Started = false; }
        ~CGame() {};

        bool isStarted() { return m_Started; }
        uint32_t getCreatedTime() { return m_CreatedTime; }
        uint32_t getStartedTime() { return m_StartedTime; }
        uint32_t getHostCounter() { return m_HostCounter; }
        string getName()          { return m_GameName;    }

        void setStarted(bool nStarted) { m_Started = nStarted; m_StartedTime = GetTime(); }

        vector<CGamePlayer> m_GamePlayers;
        vector<CGameMsg> m_GameMsg;
};

class CBotData
{
    private:
        CTCPClient* m_socket;

    public:
        string bot_ip;
        string bot_channel;
        uint16_t bot_gameport;
        uint16_t bot_command_port;
        bool can_create_game;

        vector<CGame*> m_GamesList;

        CTCPClient* GetSocket() { return m_socket; }
        void SetSocket(CTCPClient* new_socket) { m_socket = new_socket; }

        CBotData() { m_socket = NULL; }
};

class CServer
{
    private:

        CTCPServer* m_MainServer;
        CPUBProtocol* m_PUBProtocol;

        CUpdater* m_Updater;

        queue<CCommandPacket *> m_Packets; // Recieve packets from PUB client
        queue<CCommandPacket *> m_BotsPackets; // Recieve packets from bots client
        queue<BYTEARRAY> m_toAllPackets;
        vector<string> m_AllreadySendVersionInvalid;

        int portin;
        int portout;

        uint32_t last_bot_update_time;

        string m_ClientVersion; // Client version must be

        string GetUniqKey(string login);
        CUser* GetRealUser(string key);

        void SendLobbyPlayers(CUser* usr, const string& gamename);
        void UpdateRunningGamesList();

    public:
        deque<CBotData> BotList;

        CServer(int port_in, int port_out, CUpdater* nUpdater, string nClientVersion );
        virtual ~CServer();

        bool Update( long usecBlock );
        bool UpdateBotStatus();

        bool ProcessClientPackets(CUser* usr);
        bool ProcessBotsPackets(CBotData* bot);

        bool ExtractClientPackets(CTCPSocket* socket);
        bool ExtractBotsPackets(CTCPSocket* socket);

        void EventSendAuthAccept(CTCPSocket* socket, string& login, string& pass);
        void EventSendAuthFailed(CTCPSocket* socket);

    protected:

};

#endif // SERVER_H
