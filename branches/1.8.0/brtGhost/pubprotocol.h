#ifndef PUBPROTOCOL_H
#define PUBPROTOCOL_H

#define PUB_HEADER_CONSTANT 0x10
#define R_PROXY_CONSTANT            240

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
//    BYTEARRAY SendAuthAccept( string& login, string& pass, string key, vector<CBotData*> &BotList );
    BYTEARRAY SendAuthFailed( );
    BYTEARRAY SendRealName( string& login, string& key);
	BYTEARRAY SendPlayerJoinToLobby( const string& GameName, unsigned char team, const string& login );
	BYTEARRAY SendPlayerLeaveFromLobby( const string& GameName, const string& login );
	BYTEARRAY SendGameCreated( const string& GameName );
	BYTEARRAY SendGameStarted( const string& GameName );
	BYTEARRAY SendGameEnded( const string& GameName );

	BYTEARRAY RedisSet( const string & key, string value );
	BYTEARRAY RedisHSet( string key, string key2, string value);
	BYTEARRAY RedisRPUSH( const string & key, string value);
	BYTEARRAY RedisIncrGameID( string key );

	BYTEARRAY SendGameLobbyMsg( const string& GameName, const string& From, const string& Msg, uint32_t hostCounter );
	// other functions
	BYTEARRAY SendGetScore( const string& login );

//	BYTEARRAY SendBotInfo( bool isCurrentGame, uint16_t nActiveGames, uint16_t nMaxGames, vector<CGameBase*> nGameNames );

private:
	bool AssignLength( BYTEARRAY &content );
	bool ValidateLength( BYTEARRAY &content );
};


#endif
