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

#ifndef PUBPROTOCOL_H
#define PUBPROTOCOL_H

#define PUB_HEADER_CONSTANT 0x10
#define R_PROXY_CONSTANT            240
#define W3GS_HEADER_CONSTANT		247


class CPUBProtocol
{
public:
	enum RedisSendProxyProtocol
    {
        R_SET   =   2,
        R_GET   =   3,
		R_HSET	=	4,
		R_GAMEID_INCR	=	5,
        R_RPUSH	=	6
    };

	enum W3GS_Protocol {
		W3GS_PING_FROM_HOST		= 1,	// 0x01
		W3GS_SLOTINFOJOIN		= 4,	// 0x04
		W3GS_REJECTJOIN			= 5,	// 0x05
		W3GS_PLAYERINFO			= 6,	// 0x06
		W3GS_PLAYERLEAVE_OTHERS	= 7,	// 0x07
		W3GS_GAMELOADED_OTHERS	= 8,	// 0x08
		W3GS_SLOTINFO			= 9,	// 0x09
		W3GS_COUNTDOWN_START	= 10,	// 0x0A
		W3GS_COUNTDOWN_END		= 11,	// 0x0B
		W3GS_INCOMING_ACTION	= 12,	// 0x0C
		W3GS_CHAT_FROM_HOST		= 15,	// 0x0F
		W3GS_START_LAG			= 16,	// 0x10
		W3GS_STOP_LAG			= 17,	// 0x11
		W3GS_HOST_KICK_PLAYER	= 28,	// 0x1C
		W3GS_REQJOIN			= 30,	// 0x1E
		W3GS_LEAVEGAME			= 33,	// 0x21
		W3GS_GAMELOADED_SELF	= 35,	// 0x23
		W3GS_OUTGOING_ACTION	= 38,	// 0x26
		W3GS_OUTGOING_KEEPALIVE	= 39,	// 0x27
		W3GS_CHAT_TO_HOST		= 40,	// 0x28
		W3GS_DROPREQ			= 41,	// 0x29
		W3GS_SEARCHGAME			= 47,	// 0x2F (UDP/LAN)
		W3GS_GAMEINFO			= 48,	// 0x30 (UDP/LAN)
		W3GS_CREATEGAME			= 49,	// 0x31 (UDP/LAN)
		W3GS_REFRESHGAME		= 50,	// 0x32 (UDP/LAN)
		W3GS_DECREATEGAME		= 51,	// 0x33 (UDP/LAN)
		W3GS_CHAT_OTHERS		= 52,	// 0x34
		W3GS_PING_FROM_OTHERS	= 53,	// 0x35
		W3GS_PONG_TO_OTHERS		= 54,	// 0x36
		W3GS_MAPCHECK			= 61,	// 0x3D
		W3GS_STARTDOWNLOAD		= 63,	// 0x3F
		W3GS_MAPSIZE			= 66,	// 0x42
		W3GS_MAPPART			= 67,	// 0x43
		W3GS_MAPPARTOK			= 68,	// 0x44
		W3GS_MAPPARTNOTOK		= 69,	// 0x45 - just a guess, received this packet after forgetting to send a crc in W3GS_MAPPART (f7 45 0a 00 01 02 01 00 00 00)
		W3GS_PONG_TO_HOST		= 70,	// 0x46
		W3GS_INCOMING_ACTION2	= 72	// 0x48 - received this packet when there are too many actions to fit in W3GS_INCOMING_ACTION
	};

	enum Protocol {
		PUB_INIT				= 1,

		PUB_AUTH_HEADER         = 0x11,
		PUB_AUTH_ACCEPT         = 0x12,
		PUB_AUTH_FAILED         = 0x13,
		PUB_AUTH_NAME           = 0x14,

		PUB_BOT_GAME_KEY        = 0x22,

		PUB_VERSION				= 0x31,
		PUB_VERSION_OK          = 0x32,
		PUB_VERSION_FAILED      = 0x33,

		PUB_CHAT_TO_GAME		= 0x41,

		PUB_PING				= 0x50,
		PUB_PONG				= 0x51,
		PUB_CURRENT_ONLINE      = 0x52,
		PUB_CREATE_GAME			= 0x53,
		PUB_CREATE_SAVEDGAME	= 0x54,
		PUB_CHATTOGAME			= 0x55,
		PUB_CHATFROMGAME        = 0x56,
		PUB_PLAYERJOIN          = 0x58,
		PUB_PLAYERLEAVE         = 0x59,
		PUB_GAMECREATE          = 0x60,
		PUB_GAMEENDED           = 0x61,
		PUB_GAMESTARTED         = 0x62,
		PUB_JOINROOM			= 0x63,
		PUB_LEAVEROOM			= 0x64,
        PUB_GAMEPLAYERS			= 0x65,
        PUB_ALLGAMEINFO         = 0x66,
		PUB_GETSCORE	        = 0x67,
        PUB_GETSCOREANS	        = 0x68,
        PUB_BOTCREATEGAME       = 0x69,
        PUB_UNABLETOCREATE      = 0x70,
        PUB_LOWSCORE            = 0x71
	};

	CPUBProtocol( );
	~CPUBProtocol( );

	// receive functions
	BYTEARRAY SEND_GAME_KEY( string key, string login );

	// send functions
    BYTEARRAY SendAuthAccept( string login, string pass, string key, vector<CBotData> BotList );
    BYTEARRAY SendAuthFailed(  );

    BYTEARRAY SendRealName( const string login, const string& key );
    BYTEARRAY SendVersionOk();
    BYTEARRAY SendVersionFailed( vector<CUpdaterFile>& nNotCompareFiles, const string& nDownloadUrl, uint32_t allSize );

    BYTEARRAY SendChatToGame(const string& login, const string& message);
    BYTEARRAY SendChatFromGame(const string& login, const string& bot_ip, const string& gamename, uint16_t gameport, const string& message);

    BYTEARRAY SendPing();
    BYTEARRAY SendPong();

    BYTEARRAY SendLobbyPlayers( const string& gamename, const string& bot_ip, uint16_t gameport, vector<CGamePlayer>& nPlayers );

    BYTEARRAY SendRedisIncrGameID( uint32_t next_gameid );

    BYTEARRAY SendAllGameInfo( vector<CGame*> & nGameInfo );
    BYTEARRAY SendScorePlayer( const string& login, const string& score, const uint32_t games_count);

    BYTEARRAY SendCurrentOnline( const uint32_t nCurrentOnline );
    BYTEARRAY SendBotCreateGame( const string& creatorname, const string& gamename, const string& gamemode, vector<string> hold_list, uint16_t min_score, uint16_t max_score, bool is_ladder, bool is_balance );
    BYTEARRAY SendBotCreateSavedGame( const string& gamename, const string& savefilename, BYTEARRAY nMagicNumber, const string& owner, const string& saveData );
    BYTEARRAY SendUnableToCreate( );
    BYTEARRAY SendPlayerLowScore( uint16_t nScore );
	// other functions

private:
	bool AssignLength( BYTEARRAY &content );
	bool ValidateLength( BYTEARRAY &content );
};


#endif
