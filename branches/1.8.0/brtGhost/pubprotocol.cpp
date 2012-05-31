#include "includes.h"
#include "util.h"
#include "pubprotocol.h"

CPUBProtocol :: CPUBProtocol( )
{

}

CPUBProtocol :: ~CPUBProtocol( )
{

}
/*
BYTEARRAY CPUBProtocol :: SendBotInfo( bool isCurrentGame, uint16_t nActiveGames, uint16_t nMaxGames, vector<string> nGameNames, vector<uint32_t> nGameDuration )
{
	BYTEARRAY packet;

	packet.push_back( PUB_HEADER_CONSTANT );		
	packet.push_back( PUB_BOT_INFO );	

	packet.push_back( 0 );		// assign later
	packet.push_back( 0 );		// assign later

	packet.push_back( isCurrentGame );

	AssignLength(packet);

	return packet;	
}
*/
BYTEARRAY CPUBProtocol :: SendGetScore( const string& login )
{
	BYTEARRAY packet;

	packet.push_back( PUB_HEADER_CONSTANT );		
	packet.push_back( PUB_GETSCORE );	

	packet.push_back( 0 );		// assign later
	packet.push_back( 0 );		// assign later

	UTIL_AppendByteArray(packet, login, true);

	AssignLength(packet);

	return packet;	
}

BYTEARRAY CPUBProtocol :: RedisIncrGameID( string key )
{
    BYTEARRAY packet;

    packet.push_back( R_PROXY_CONSTANT );		// Auth header  1 byte
    packet.push_back( R_GAMEID_INCR );           // 1 byte

    packet.push_back( 0 );           // 1 byte
    packet.push_back( 0 );           // 1 byte

	UTIL_AppendByteArray(packet, key, true);

    AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: RedisHSet( string key, string key2, string value)
{
    BYTEARRAY packet;

    packet.push_back( R_PROXY_CONSTANT );		// Auth header  1 byte
    packet.push_back( R_HSET );           // 1 byte

    packet.push_back( 0 );           // 1 byte
    packet.push_back( 0 );           // 1 byte

	UTIL_AppendByteArray(packet, key, true);
	UTIL_AppendByteArray(packet, key2, true);
    UTIL_AppendByteArray(packet, value, false);

    AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: RedisSet( const string & key, string value)
{
    BYTEARRAY packet;

    packet.push_back( R_PROXY_CONSTANT );		// Auth header  1 byte
    packet.push_back( R_HSET );           // 1 byte

    packet.push_back( 0 );           // 1 byte
    packet.push_back( 0 );           // 1 byte

	UTIL_AppendByteArray(packet, key, true);
    UTIL_AppendByteArray(packet, value, true);

    AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: RedisRPUSH( const string & key, string value)
{
	BYTEARRAY packet;

    packet.push_back( R_PROXY_CONSTANT );		// Auth header  1 byte
    packet.push_back( R_RPUSH );           // 1 byte

    packet.push_back( 0 );           // 1 byte
    packet.push_back( 0 );           // 1 byte

	UTIL_AppendByteArray(packet, key, true);
    UTIL_AppendByteArray(packet, value, true);

    AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: SendGameLobbyMsg( const string& GameName, const string& From, const string& Msg, uint32_t hostCounter  )
{
    BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );		// Auth header  1 byte
    packet.push_back( PUB_CHATFROMGAME );           // 1 byte

    packet.push_back( 0 );           // 1 byte
    packet.push_back( 0 );           // 1 byte

	UTIL_AppendByteArray(packet, hostCounter, false);

    UTIL_AppendByteArray(packet, GameName, true);
	UTIL_AppendByteArray(packet, From, true);
	UTIL_AppendByteArray(packet, Msg, true);

    AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: SendGameStarted( const string& GameName )
{
    BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );
    packet.push_back( PUB_GAMESTARTED );

    packet.push_back( 0 );          
    packet.push_back( 0 );         

    UTIL_AppendByteArray(packet, GameName, true);

    AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: SendGameEnded( const string& GameName )
{
    BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );
    packet.push_back( PUB_GAMEENDED );

    packet.push_back( 0 );          
    packet.push_back( 0 );         

    UTIL_AppendByteArray(packet, GameName, true);

    AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: SendGameCreated( const string& GameName )
{
    BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );
    packet.push_back( PUB_GAMECREATE );

    packet.push_back( 0 );          
    packet.push_back( 0 );         

    UTIL_AppendByteArray(packet, GameName, true);

    AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: SendPlayerJoinToLobby( const string& GameName, unsigned char team, const string& login )
{
    BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );		// Auth header  1 byte
    packet.push_back( PUB_PLAYERJOIN );           // 1 byte

    packet.push_back( 0 );           // 1 byte
    packet.push_back( 0 );           // 1 byte

    UTIL_AppendByteArray(packet, login, true);
	UTIL_AppendByteArray(packet, GameName, true);

    AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: SendPlayerLeaveFromLobby( const string& GameName, const string& login )
{
    BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );		// Auth header  1 byte
    packet.push_back( PUB_PLAYERLEAVE );           // 1 byte

    packet.push_back( 0 );           // 1 byte
    packet.push_back( 0 );           // 1 byte

    UTIL_AppendByteArray(packet, login, true);
	UTIL_AppendByteArray(packet, GameName, true);

    AssignLength(packet);

    return packet;
}


BYTEARRAY CPUBProtocol :: SendAuthFailed( )
{
    BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );		// Auth header  1 byte
    packet.push_back( PUB_AUTH_FAILED );           // 1 byte

    UTIL_AppendByteArray(packet, UTIL_CreateByteArray( (uint16_t) (4), false)); // 2 byte

    return packet;
}

BYTEARRAY CPUBProtocol :: SendRealName( string& login, string& key )
{
    BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );		// Auth header  1 byte
    packet.push_back( PUB_AUTH_NAME );           // 1 byte

    packet.push_back( 0 );           // 1 byte
    packet.push_back( 0 );           // 1 byte

    packet.push_back( login.size() );
    UTIL_AppendByteArray(packet, UTIL_CreateByteArray( (unsigned char *)login.c_str( ), login.size( ) ));

    packet.push_back( key.size() );
    UTIL_AppendByteArray(packet, UTIL_CreateByteArray( (unsigned char *)key.c_str( ), key.size( ) ));

    AssignLength(packet);

    return packet;
}
/*
BYTEARRAY CPUBProtocol :: SendAuthAccept( string& login, string& pass, string key, vector<CBotData*> &BotList )
{
    BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );		// Auth header  1 byte
    packet.push_back( PUB_AUTH_ACCEPT );           // 1 byte

    packet.push_back( 0 ); // assign later
    packet.push_back( 0 ); // assign later

    packet.push_back( login.size( ) ); // 1 byte
    UTIL_AppendByteArray(packet, UTIL_CreateByteArray( (unsigned char *)login.c_str( ), login.size( ) )); //

    packet.push_back( BotList.size() ); // 1 byte

    for (uint32_t i = 0; i < BotList.size(); i++)
    {
        packet.push_back(BotList[i]->bot_ip.size( ));

        UTIL_AppendByteArray(packet, UTIL_CreateByteArray( (unsigned char *)BotList[i]->bot_ip.c_str( ), BotList[i]->bot_ip.size( ) ));
        UTIL_AppendByteArray(packet, UTIL_CreateByteArray( (uint16_t)(BotList[i]->bot_port) , false));
    }

    packet.push_back( key.size( ) ); // 1 byte
    UTIL_AppendByteArray(packet, UTIL_CreateByteArray( (unsigned char *)key.c_str( ), key.size( ) ));

    cout << "[CPUB] send new key " << key << endl;

    AssignLength(packet);

    return packet;
}
*/

BYTEARRAY CPUBProtocol :: SEND_GAME_KEY( string key, string login )
{
	BYTEARRAY packet;
	packet.push_back( PUB_HEADER_CONSTANT );
	packet.push_back( this->PUB_BOT_GAME_KEY );
	packet.push_back( 0 );
	packet.push_back( 0 );
	packet.push_back( key.size() );
	UTIL_AppendByteArray( packet, key, false );
	packet.push_back( login.size() );
	UTIL_AppendByteArray( packet, login, false );
	AssignLength( packet );
	return packet;
}

bool CPUBProtocol :: AssignLength( BYTEARRAY &content )
{
	// insert the actual length of the content array into bytes 3 and 4 (indices 2 and 3)

	BYTEARRAY LengthBytes;

	if( content.size( ) >= 4 && content.size( ) <= 65535 )
	{
		LengthBytes = UTIL_CreateByteArray( (uint16_t)content.size( ), false );
		content[2] = LengthBytes[0];
		content[3] = LengthBytes[1];
		return true;
	}

	return false;
}

bool CPUBProtocol :: ValidateLength( BYTEARRAY &content )
{
	// verify that bytes 3 and 4 (indices 2 and 3) of the content array describe the length

	uint16_t Length;
	BYTEARRAY LengthBytes;

	if( content.size( ) >= 4 && content.size( ) <= 65535 )
	{
		LengthBytes.push_back( content[2] );
		LengthBytes.push_back( content[3] );
		Length = UTIL_ByteArrayToUInt16( LengthBytes, false );

		if( Length == content.size( ) )
			return true;
	}

	return false;
}
