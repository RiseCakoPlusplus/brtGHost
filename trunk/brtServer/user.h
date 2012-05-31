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

#ifndef USERS_H
#define USERS_H

enum { CLIENT_NOT_AUTH = 0, CLIENT_AUTH = 1 };

class CChannel
{
    public:
        string m_name;
        unsigned char m_type;

        string m_bot_ip;  // if room type is WAR3 game
        uint16_t m_gameport; // if room type is WAR3 game

        CChannel( string nName, unsigned char nType, string nBot_ip = string(), uint16_t nGamePort = 0 )
        {
            m_name = nName;
            m_bot_ip = nBot_ip;
            m_gameport = nGamePort;
            m_type = nType;
        }
};

class CUser
{
    private:
        string name;
        string lower_name;
        string hash_password;
        string key;

        int status;
        bool client_version_checked;

        CTCPSocket* m_socket;

    public:
        uint32_t last_ping_send;
        uint32_t last_pong_recv;
        uint32_t m_last_chat_to_game_send;

        vector<CChannel> m_ChannelList;

        CUser(CTCPSocket* socket);
        ~CUser();

        void SetName(string new_name)
        {
            name = new_name;
            lower_name.resize( new_name.size() );
            transform( new_name.begin(), new_name.end(), lower_name.begin( ), (int(*)(int))tolower );
        }

        string GetName() { return name; }
        string GetLowerName() { return lower_name; }

        bool isVersionChecked() { return client_version_checked; }
        void setVersionChecked(bool checked) { client_version_checked = checked; }

        void SetHashPassword(string hash_pass) { hash_password = hash_pass; }
        string GetHashPassword() { return hash_password; }

        void SetStatus( int new_status ) { status = new_status; }
        int GetStatus() { return status; }

        void SetKey(string new_key) { key = new_key; }
        string GetKey() { return key; }

        CTCPSocket* GetSocket() { return m_socket; }
};

#endif // CUSERS_H
