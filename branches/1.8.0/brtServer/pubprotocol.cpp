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

#include "util.h"
#include "server.h"
#include "updater.h"
#include "pubprotocol.h"

uint32_t GetTime( );

CPUBProtocol :: CPUBProtocol( )
{

}

CPUBProtocol :: ~CPUBProtocol( )
{

}

BYTEARRAY CPUBProtocol :: SendPlayerLowScore( uint16_t nScore )
{
     BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );		// Auth header  1 byte
    packet.push_back( PUB_LOWSCORE );           // 1 byte

    packet.push_back( 0 );           // 1 byte
    packet.push_back( 0 );           // 1 byte

    UTIL_AppendByteArray( packet, nScore, false);

    AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: SendUnableToCreate( )
{
    BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );		// Auth header  1 byte
    packet.push_back( PUB_UNABLETOCREATE );           // 1 byte

    packet.push_back( 0 );           // 1 byte
    packet.push_back( 0 );           // 1 byte

    AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: SendBotCreateSavedGame( const string& gamename, const string& savefilename, BYTEARRAY nMagicNumber, const string& owner, const string& saveData )
{
	BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );
    packet.push_back( PUB_CREATE_SAVEDGAME );

    packet.push_back( 0 );
    packet.push_back( 0 );

	UTIL_AppendByteArray(packet, gamename, true);
	UTIL_AppendByteArray(packet, savefilename, true);
	UTIL_AppendByteArray(packet, nMagicNumber);
	UTIL_AppendByteArray(packet, owner, true);
	UTIL_AppendByteArray(packet, saveData, false);

	AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: SendBotCreateGame( const string& creatorname, const string& gamename, const string& gamemode, vector<string> hold_list, uint16_t min_score, uint16_t max_score, bool is_ladder, bool is_balance )
{
	BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );
    packet.push_back( PUB_BOTCREATEGAME );

    packet.push_back( 0 );
    packet.push_back( 0 );

	packet.push_back( is_ladder );
	packet.push_back( is_balance );
	packet.push_back( hold_list.size() );
	packet.push_back( 0 );

	UTIL_AppendByteArray(packet, min_score, false);
	UTIL_AppendByteArray(packet, max_score, false);

	UTIL_AppendByteArray(packet, gamemode, true);
	UTIL_AppendByteArray(packet, gamename, true);
	UTIL_AppendByteArray(packet, creatorname, true);

	for ( uint16_t i = 0; i < hold_list.size(); ++i)
		UTIL_AppendByteArray(packet, hold_list[i], true);

	AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: SendCurrentOnline( const uint32_t nCurrentOnline )
{
    BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );		// Auth header  1 byte
    packet.push_back( PUB_CURRENT_ONLINE );           // 1 byte

    packet.push_back( 0 );           // 1 byte
    packet.push_back( 0 );           // 1 byte

    UTIL_AppendByteArray( packet, nCurrentOnline, false );

    AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: SendScorePlayer( const string& login, const string& score, const uint32_t games_count)
{
    BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );		// Auth header  1 byte
    packet.push_back( PUB_GETSCOREANS );           // 1 byte

    packet.push_back( 0 );           // 1 byte
    packet.push_back( 0 );           // 1 byte

    UTIL_AppendByteArray( packet, login, true);
    UTIL_AppendByteArray( packet, score, true);
    UTIL_AppendByteArray( packet, games_count, false );

    AssignLength(packet);

    return packet;

}

BYTEARRAY CPUBProtocol :: SendAllGameInfo( vector<CGame*> & nGameInfo )
{
    BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );		// Auth header  1 byte
    packet.push_back( PUB_ALLGAMEINFO );           // 1 byte

    packet.push_back( 0 );           // 1 byte
    packet.push_back( 0 );           // 1 byte

    UTIL_AppendByteArray( packet, (uint16_t)nGameInfo.size(), false);

    for (vector<CGame*>::iterator i = nGameInfo.begin(); i != nGameInfo.end(); ++i)
    {
        UTIL_AppendByteArray( packet, (*i)->getName(), true );
        UTIL_AppendByteArray( packet, GetTime() - (*i)->getStartedTime(), false );
    }

    AssignLength(packet);

    return packet;

}

BYTEARRAY CPUBProtocol :: SendRedisIncrGameID( uint32_t next_gameid )
{
 	BYTEARRAY packet;
	packet.push_back( R_PROXY_CONSTANT );
	packet.push_back( this->R_GAMEID_INCR );
	packet.push_back( 0 );
	packet.push_back( 0 );

    UTIL_AppendByteArray( packet, next_gameid, false);

	AssignLength( packet );
	return packet;
}

BYTEARRAY CPUBProtocol :: SendLobbyPlayers( const string& gamename, const string& bot_ip, uint16_t gameport, vector<CGamePlayer>& nPlayers )
{
    BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );		// Auth header  1 byte
    packet.push_back( PUB_GAMEPLAYERS );           // 1 byte

    packet.push_back( 0 );           // 1 byte
    packet.push_back( 0 );           // 1 byte

    UTIL_AppendByteArray( packet, gameport, false);
    UTIL_AppendByteArray( packet, bot_ip, true);
    UTIL_AppendByteArray( packet, gamename, true);

    packet.push_back( nPlayers.size() );
    for (vector<CGamePlayer>::iterator i = nPlayers.begin(); i != nPlayers.end(); ++i)
        UTIL_AppendByteArray( packet, (*i).GetName(), true );

    AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: SendAuthFailed( )
{
    BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );		// Auth header  1 byte
    packet.push_back( PUB_AUTH_FAILED );           // 1 byte

    packet.push_back(0);
    packet.push_back(0);

    AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: SendRealName( const string login, const string& key )
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

BYTEARRAY CPUBProtocol :: SendAuthAccept( string login, string pass, string key, vector<CBotData> BotList )
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
        packet.push_back(BotList[i].bot_ip.size( ));

        UTIL_AppendByteArray(packet, UTIL_CreateByteArray( (unsigned char *)BotList[i].bot_ip.c_str( ), BotList[i].bot_ip.size( ) ));
        UTIL_AppendByteArray(packet, UTIL_CreateByteArray( (uint16_t)(0) , false));
        UTIL_AppendByteArray(packet, UTIL_CreateByteArray( (uint16_t)(BotList[i].bot_gameport) , false));
    }

    packet.push_back( key.size( ) ); // 1 byte
    UTIL_AppendByteArray(packet, UTIL_CreateByteArray( (unsigned char *)key.c_str( ), key.size( ) ));

    AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: SendVersionOk()
{
    BYTEARRAY packet;

    packet.push_back( PUB_HEADER_CONSTANT );
    packet.push_back( PUB_VERSION_OK );

    packet.push_back( 0 ); // assign later
    packet.push_back( 0 ); // assign later

    AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: SendVersionFailed( vector<CUpdaterFile>& nNotCompareFiles, const string& nDownloadUrl, uint32_t allSize )
{
    BYTEARRAY packet;
    packet.push_back( PUB_HEADER_CONSTANT );
    packet.push_back( PUB_VERSION_FAILED );

    packet.push_back( 0 ); // assign later
    packet.push_back( 0 ); // assign later

    UTIL_AppendByteArray( packet, (uint16_t)nNotCompareFiles.size(), false );
    UTIL_AppendByteArray( packet, (uint32_t)allSize, false );
    UTIL_AppendByteArray( packet, nDownloadUrl, true );

    for ( vector<CUpdaterFile>::iterator i = nNotCompareFiles.begin(); i != nNotCompareFiles.end(); ++i )
    {
        UTIL_AppendByteArray( packet, (uint32_t)(*i).getSize(), false );
        UTIL_AppendByteArray( packet, (uint32_t)(*i).getSizeComp(), false );
        UTIL_AppendByteArray( packet, (*i).getName(), true );
    }

    AssignLength(packet);

    return packet;
}

BYTEARRAY CPUBProtocol :: SendPong()
{
	BYTEARRAY packet;
	packet.push_back( PUB_HEADER_CONSTANT );
	packet.push_back( this->PUB_PONG );
	packet.push_back( 0 );
	packet.push_back( 0 );

	AssignLength( packet );
	return packet;
}

BYTEARRAY CPUBProtocol :: SendPing()
{
	BYTEARRAY packet;
	packet.push_back( PUB_HEADER_CONSTANT );
	packet.push_back( this->PUB_PING );
	packet.push_back( 0 );
	packet.push_back( 0 );

	AssignLength( packet );
	return packet;
}


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

BYTEARRAY CPUBProtocol :: SendChatToGame(const string& login, const string& message)
{
 	BYTEARRAY packet;
	packet.push_back( PUB_HEADER_CONSTANT );
	packet.push_back( this->PUB_CHAT_TO_GAME );
	packet.push_back( 0 );
	packet.push_back( 0 );
	UTIL_AppendByteArray( packet, login, true );
	UTIL_AppendByteArray( packet, message, true );
	AssignLength( packet );
	return packet;
}

BYTEARRAY CPUBProtocol :: SendChatFromGame(const string& login, const string& bot_ip, const string& gamename, uint16_t gameport, const string& message)
{
 	BYTEARRAY packet;
	packet.push_back( PUB_HEADER_CONSTANT );
	packet.push_back( this->PUB_CHATFROMGAME );
	packet.push_back( 0 );
	packet.push_back( 0 );

    UTIL_AppendByteArray( packet, gameport, false);
    UTIL_AppendByteArray( packet, bot_ip, true);
    UTIL_AppendByteArray( packet, gamename, true);

	UTIL_AppendByteArray( packet, login, true );
	UTIL_AppendByteArray( packet, message, true );

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
