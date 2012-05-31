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

#include "includes.h"
#include "socket.h"
#include "commandpacket.h"
#include "updater.h"
#include "server.h"
#include "util.h"
#include "user.h"
#include "pubprotocol.h"
#include "sha1.h"
//#include "redisclient.h"
#include "update_dota_elo.h"
#include "servermysql.h"

#include <time.h>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>

extern list<CUser*> DBUsersList;
extern list<CUser*> UsersList;

extern string CFGFile;

extern int UpdateUsersList();

//extern boost::shared_ptr<redis::client> redis_client;

bool AssignLength( BYTEARRAY &content )
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

CServer::CServer(int port_in, int port_out, CUpdater* nUpdater, string n_ClientVersion )
{
    portin = port_in;
    portout = port_out;

    m_Updater = nUpdater;

    m_ClientVersion = n_ClientVersion;

    m_MainServer = new CTCPServer( );
    m_MainServer->Listen( string( ) , port_in ); // Liten for client connects

    BotList.clear();

    srand ( GetTime() );

    last_bot_update_time = GetTime();
}

CServer::~CServer()
{
    delete m_MainServer;

    for (vector<CBotData>::iterator it = BotList.begin(); it != BotList.end(); it++)
    {
        for (vector<CGame*>::iterator i = (*it).m_GamesList.begin(); i != (*it).m_GamesList.end(); ++i)
            delete *i;

        delete (*it).GetSocket();
    }
 //       delete *it;

    for (list<CUser*>::iterator it = UsersList.begin(); it != UsersList.end(); it++)
        delete *it;
}

void CServer :: ProcessUserCommands( const CChatCommand& nCommand )
{
	if ( nCommand.m_Command == "pub" )
	{
	   bool is_GameCreated = false;

       for (vector<CBotData>::iterator i = BotList.begin(); i != BotList.end(); ++i)
        {
            if ( /*( (*i).bot_channel == nLocationName || nLocationName.empty() ) && */(*i).can_create_game )
            {
                bool isGameCreated = false;

                for (vector<CGame*>::iterator c = (*i).m_GamesList.begin(); c != (*i).m_GamesList.end(); ++c)
                {
                    if ( !(*c)->isStarted() )
                    {
                        isGameCreated = true;
                        break;
                    }
                }

                if ( !isGameCreated )
                {
					(*i).GetSocket()->PutBytes( m_PUBProtocol->SendBotCreateGame( nCommand.m_User,
																				  nCommand.m_Payload,
                                                                                  string(),
                                                                                  vector<string>(),
                                                                                  0,
                                                                                  30000,
                                                                                  true,
                                                                                  false ));

                    cout << "[USER] Game created " << nCommand.m_Payload << endl;

                    is_GameCreated = true;

                    break;
                }

            }
	   }

	   if (!is_GameCreated)
//			usr->GetSocket()->PutBytes( m_PUBProtocol->SendUnableToCreate() );
		;
	}
}

bool CServer :: UpdateBotStatus()
{
    for (vector<CBotData>::iterator it = BotList.begin(); it != BotList.end(); it++)
    {
        if ( !(*it).GetSocket() )
            (*it).SetSocket( new CTCPClient( ) );

            (*it).GetSocket() -> Connect( string() , (*it).bot_ip, (*it).bot_command_port );
            (*it).GetSocket() -> SetNoDelay( true );
    }

    return true;
}

CUser* CServer::GetRealUser(string key)
{
    for (list<CUser*>::iterator it = UsersList.begin(); it != UsersList.end(); it++)
        if ((*it)->GetKey() == key) return (*it);

    return NULL;
}

void CServer :: SendLobbyPlayers(CUser* usr, const string& gamename)
{
    for (vector<CChannel>::iterator c = usr->m_ChannelList.begin(); c != usr->m_ChannelList.end(); ++c)
    {
        if ( (*c).m_type == 3 ) // WAR3_GAME
        {
            for (vector<CBotData>::iterator i = BotList.begin(); i != BotList.end(); ++i)
            {
                if ( (*i).bot_ip == (*c).m_bot_ip && (*i).bot_gameport == (*c).m_gameport )
                {

                    for (vector<CGame*>::iterator it = (*i).m_GamesList.begin(); it != (*i).m_GamesList.end(); ++it)
                    {
                        if ( (*it)->getName() == (*c).m_name )
                        {
                            usr->GetSocket()->PutBytes( m_PUBProtocol->SendLobbyPlayers( gamename, (*i).bot_ip, (*i).bot_gameport,  (*it)->m_GamePlayers ));

                            break;
                        }

                    }

                    break;
                }

            }

        }
    }

}

void CServer :: UpdateRunningGamesList()
{
        vector<CGame*> m_allGamesList;

        for (vector<CBotData>::iterator i = BotList.begin(); i != BotList.end(); ++i)
            for (vector<CGame*>::iterator c = (*i).m_GamesList.begin(); c != (*i).m_GamesList.end(); ++c)
            if ( (*c)->isStarted() )
                m_allGamesList.push_back( *c );

        m_toAllPackets.push( m_PUBProtocol->SendAllGameInfo( m_allGamesList ) );
}

string CServer::GetUniqKey(string login)
{
    string hash = login + login;

    hash += UTIL_ToString( GetTime() );
    hash += UTIL_ToString( (rand() + 65) );
    hash += UTIL_ToString( (rand() + 67) + (rand() + 32 ) );
    hash += UTIL_ToString( GetTime() );

    return md5( md5(hash) + UTIL_ToString( GetTime() + rand() ) );
}

bool CServer::ExtractBotsPackets(CTCPSocket* socket)
{
	string *RecvBuffer = socket->GetBytes( );
	BYTEARRAY Bytes = UTIL_CreateByteArray( (unsigned char *)RecvBuffer->c_str( ), RecvBuffer->size( ) );

	// a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

	while( Bytes.size( ) >= 4 )
	{
		if( Bytes[0] == PUB_HEADER_CONSTANT || Bytes[0] == W3GS_HEADER_CONSTANT || Bytes[0] == R_PROXY_CONSTANT)
		{
			// bytes 2 and 3 contain the length of the packet

			uint16_t Length = UTIL_ByteArrayToUInt16( Bytes, false, 2 );

			if( Length >= 4 )
			{
				if( Bytes.size( ) >= Length )
				{
                    m_BotsPackets.push( new CCommandPacket( Bytes[0], Bytes[1], BYTEARRAY( Bytes.begin(), Bytes.begin() + Length), socket ));

					*RecvBuffer = RecvBuffer->substr( Length );
					Bytes = BYTEARRAY( Bytes.begin( ) + Length, Bytes.end( ) );
				}
				else
				{
				    CONSOLE_Print( "[SERVER] Error from remote server (bad length " + UTIL_ToString(Length) + ")" );

					return false;
                }
			}
			else
			{
				CONSOLE_Print( "[SERVER] received invalid packet from remote server (bad length)" );
				return false;
			}
		}
		else
		{
			CONSOLE_Print( "[SERVER] received invalid packet from remote server (bad header constant)" );
			return false;
		}
	}

	return true;
}

bool CServer::ExtractClientPackets(CTCPSocket* socket)
{
	string *RecvBuffer = socket->GetBytes( );
	BYTEARRAY Bytes = UTIL_CreateByteArray( (unsigned char *)RecvBuffer->c_str( ), RecvBuffer->size( ) );

	// a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

	while( Bytes.size( ) >= 4 )
	{
		if( Bytes[0] == PUB_HEADER_CONSTANT || Bytes[0] == W3GS_HEADER_CONSTANT)
		{
			// bytes 2 and 3 contain the length of the packet

			uint16_t Length = UTIL_ByteArrayToUInt16( Bytes, false, 2 );

			if( Length >= 4 )
			{
				if( Bytes.size( ) >= Length )
				{
					m_Packets.push( new CCommandPacket( Bytes[0], Bytes[1], BYTEARRAY( Bytes.begin( ), Bytes.begin( ) + Length ) , socket ));

					*RecvBuffer = RecvBuffer->substr( Length );
					Bytes = BYTEARRAY( Bytes.begin( ) + Length, Bytes.end( ) );
				}
				else
				{
				    return true;
				   // CONSOLE_Print( "[SRV] Error from remote server (bad length " + UTIL_ToString(Length) + ")" );

					// return false;
                }
			}
			else
			{
				CONSOLE_Print( "[SERVER] received invalid packet from remote server (bad length)" );
				return false;
			}
		}
		else
		{
			CONSOLE_Print( "[SERVER] received invalid packet from remote server (bad header constant)" );
			return false;
		}
	}

	return true;
}


bool CServer :: ProcessBotsPackets(CBotData* bot)
{
    while ( !m_BotsPackets.empty( ) )
	{
		CCommandPacket * packet = m_BotsPackets.front( );
		m_BotsPackets.pop( );

		if (packet->GetPacketType() == R_PROXY_CONSTANT)
		{
		    if ( packet->GetID() == CPUBProtocol::R_HSET )
		    {
		        BYTEARRAY data = packet->GetData();

		        string key = UTIL_ExtractCString(data, 4);
		        string key2 = UTIL_ExtractCString(data, 5 + key.size() );
                BYTEARRAY bvalue = BYTEARRAY( data.begin() + 6 + key.size() + key2.size(), data.end() );// UTIL_ExtractCString(data, 6 + key.size() + key2.size() );
                string value = string( bvalue.begin(), bvalue.end() );

 //               redis_client->hset( key, key2, value );

                if ( "savedata" ==  key2 )
                {
                    cout << "[HSET saveData] " << value << " size " << value.size() << endl;

                  //  cout << "[packetData] " << data << " size " << data.size() << endl;
                }

		    } else if ( packet->GetID() == CPUBProtocol::R_SET )
		    {
		        BYTEARRAY data = packet->GetData();
		        string key = UTIL_ExtractCString(data, 4);
                string value = UTIL_ExtractCString(data, 5 + key.size() );

 //               redis_client->set( key, value );

		    } else if ( packet->GetID() == CPUBProtocol::R_RPUSH )
		    {
		        BYTEARRAY data = packet->GetData();
		        string key = UTIL_ExtractCString(data, 4);
                string value = UTIL_ExtractCString(data, 5 + key.size() );

 //               redis_client->rpush( key, value );
		    }
		    else if ( packet->GetID() == CPUBProtocol::R_GAMEID_INCR )
		    {
		        BYTEARRAY data = packet->GetData();
		        string key = UTIL_ExtractCString(data, 4);

	//	        bot->GetSocket()->PutBytes( m_PUBProtocol->SendRedisIncrGameID( redis_client->incr( key )));
		    }

		} else if (packet->GetPacketType() == W3GS_HEADER_CONSTANT)
        {
/*
            for (list<CUser*>::iterator it = UsersList.begin(); it != UsersList.end(); ++it)
                  //  for (vector<CChannel>::iterator c = (*it)->m_ChannelList.begin(); c != (*it)->m_ChannelList.end(); ++c)
                  //  if ( (*c).m_name == bot->bot_channel)
                    if ( !isSavedGame )
                    {
                        (*it)->GetSocket()->PutBytes( packet->GetData() );
                  //      break;
                    }
                    else for ( vector<string>::iterator tt = nSaveGameNames.begin(); tt != nSaveGameNames.end(); ++tt)
                        {
                            if ( (*tt) == (*it)->GetName() )
                            {
                                (*it)->GetSocket()->PutBytes( packet->GetData() );
                                break;
                            }
                        }
*/

                BYTEARRAY packet_data = packet->GetData();

                bool isAccept = true;

                if (packet->GetID() == 48) // W3GS_GAMEINFO			= 48
                {
                    BYTEARRAY HostCounter = BYTEARRAY( packet_data.begin( ) + 12 , packet_data.begin( ) + 16 );
                    BYTEARRAY EntryKey = BYTEARRAY( packet_data.begin( ) + 16, packet_data.begin( ) +20 );
                    string nGameName = UTIL_ExtractCString( packet_data,  20 );
                    string nStatString = UTIL_ExtractCString( packet_data ,22 + nGameName.size( ) );

                    uint32_t hostCounter = UTIL_ByteArrayToUInt32(HostCounter, false);

                    uint32_t i = 23 + nGameName.size( )  + nStatString.size( ) + 22;

                    if ( packet_data.size() > i )
                    {
                        BYTEARRAY nValidPacket = packet_data;

                        isAccept = false; // this is saved game

                        nValidPacket.erase( nValidPacket.begin() + i, nValidPacket.end() );
                        UTIL_AppendByteArray(nValidPacket, bot->GetSocket()->GetIPString(), true);
                        AssignLength( nValidPacket );

                        unsigned char nSize = packet_data[i++];

                        for ( unsigned char j = 0; j < nSize; ++j )
                        {
                            string nName = UTIL_ExtractCString( packet_data, i );

                            for (list<CUser*>::iterator it = UsersList.begin(); it != UsersList.end(); it++)
                            {
                                if ( (*it)->GetName() == nName )
                                    (*it)->GetSocket()->PutBytes( nValidPacket );
                            }

                            i += nName.size() + 1;
                        }

                        packet_data = nValidPacket;
                    } else
                    {
//                        UTIL_AppendByteArray(packet_data, bot->GetSocket()->GetIPString(), true);
//                        AssignLength( packet_data );
                    }

                    bool find = false;

                    for (vector<CGame*>::iterator i = bot->m_GamesList.begin(); i != bot->m_GamesList.end(); ++i)
                        if ( (*i)->getName() == nGameName )
                        {
                            find = true;
                            break;
                        }

                    if (!find)
                    {
                        cout << "[GAME] Game created - " << nGameName << endl;

                        bot->m_GamesList.push_back( new CGame(nGameName, hostCounter) );
                    }
                }/* else
                if (packet->GetID() == 51) // W3GS_DECREATEGAME = 51            TODOTODO - Double check
                {
                    BYTEARRAY data = packet->GetData();

                    uint32_t hostCounter = UTIL_ByteArrayToUInt32( BYTEARRAY(data.begin() + 4, data.end() + 8), false );

                    for (vector<CGame*>::iterator i = bot->m_GamesList.begin(); i != bot->m_GamesList.end(); ++i)
                        if ( (*i)->getHostCounter() == hostCounter )
                        {
                            delete *i;
                            bot->m_GamesList.erase(i);

                            break;
                        }

                }*/


            if (isAccept)
                m_toAllPackets.push( packet_data );

        }
        else switch( packet->GetID( ) )
		{
		    case CPUBProtocol::PUB_GETSCORE:
		    {
                BYTEARRAY packet_data = packet->GetData();
                string login = UTIL_ExtractCString( packet_data, 4 );
/*
				try
				{
				    string score = redis_client->zscore("DOTA_ELO:scores", login );

				    uint32_t player_games_count = redis_client->llen("UID:" + login + ":games");

				    if ( score == "**nonexistent-key**" )
                        score = "1000.0";

                    bot->GetSocket()->PutBytes( m_PUBProtocol->SendScorePlayer( login , score, player_games_count ));

				} catch (redis::redis_error & e)
					{
						//cerr << "[REDIS] " << e.what() << endl;
					}
*/
             //   cout << "[PACKET] from bot PUB_GETSCORE " << string(login) << endl;

		        break;
		    }
		    case CPUBProtocol::PUB_CHATFROMGAME:
		    {
                BYTEARRAY packet_data = packet->GetData();

                BYTEARRAY hostCounter = BYTEARRAY( packet_data.begin( ) + 4 , packet_data.begin( ) + 8 );

                string game_name = UTIL_ExtractCString(packet_data, 8);
                string from = UTIL_ExtractCString(packet_data, game_name.size() + 9 );
                string msg = UTIL_ExtractCString(packet_data, game_name.size() + from.size() + 10);

                for (vector<CGame*>::iterator i = bot->m_GamesList.begin(); i != bot->m_GamesList.end(); ++i)
                    if ( (*i)->getName() == game_name && (*i)->getHostCounter() == UTIL_ByteArrayToUInt32(hostCounter, false) )
                        {
                            (*i)->m_GameMsg.push_back( CGameMsg( from, msg ) );

                            for (list<CUser*>::iterator it = UsersList.begin(); it != UsersList.end(); ++it)
                                for (vector<CChannel>::iterator c = (*it)->m_ChannelList.begin(); c != (*it)->m_ChannelList.end(); ++c)
                                    if ( (*c).m_name == (*i)->getName() && (*c).m_type == 3 )
                                    {
                                        (*it)->GetSocket()->PutBytes( m_PUBProtocol->SendChatFromGame(from, bot->bot_ip, game_name, bot->bot_gameport, msg) );
                                        break;
                                    }

                            break;
                        }

    //            cout << "[GAME] [LOBBY] game " << from << ": " << msg << endl;
		        break;
		    }

		    case CPUBProtocol::PUB_GAMEENDED:
		    {
                BYTEARRAY packet_data = packet->GetData();
                string game_name = UTIL_ExtractCString(packet_data, 4);

                for (vector<CGame*>::iterator i = bot->m_GamesList.begin(); i != bot->m_GamesList.end(); ++i)
                    if ( (*i)->getName() == game_name )
                    {
                        delete *i;
                        bot->m_GamesList.erase(i);

                        break;
                    }

                cout << "[GAME] ended game " << game_name << endl;

                UpdateRunningGamesList();

		        break;
		    }

		    case CPUBProtocol::PUB_GAMESTARTED:
		    {
                BYTEARRAY packet_data = packet->GetData();
                string game_name = UTIL_ExtractCString(packet_data, 4);

                for (vector<CGame*>::iterator i = bot->m_GamesList.begin(); i != bot->m_GamesList.end(); ++i)
                    if ( (*i)->getName() == game_name )
                    {
                        (*i)->setStarted(true);
                        break;
                    }

                cout << "[GAME] started game " << game_name << endl;

                UpdateRunningGamesList();

		        break;
		    }

		    case CPUBProtocol::PUB_PLAYERJOIN:
		    {
                BYTEARRAY packet_data = packet->GetData();

                string login = UTIL_ExtractCString(packet_data, 4);
                string game_name = UTIL_ExtractCString(packet_data, login.size() + 5);

                for (vector<CGame*>::iterator i = bot->m_GamesList.begin(); i != bot->m_GamesList.end(); ++i)
                    if ( (*i)->getName() == game_name )
                    {
                        (*i)->m_GamePlayers.push_back( CGamePlayer(login, 0) );

                        for (list<CUser*>::iterator it = UsersList.begin(); it != UsersList.end(); ++it)
                            for (vector<CChannel>::iterator c = (*it)->m_ChannelList.begin(); c != (*it)->m_ChannelList.end(); ++c)
                                 if ( (*c).m_name == (*i)->getName() && (*c).m_type == 3 )
                                   (*it)->GetSocket()->PutBytes( m_PUBProtocol->SendLobbyPlayers( game_name, bot->bot_ip, bot->bot_gameport, (*i)->m_GamePlayers) );

                        break;
                    }


   //             cout << "[GAME] " << login << " joined to the game " << game_name << endl;
                break;
		    }

            case CPUBProtocol::PUB_PLAYERLEAVE:
            {
                BYTEARRAY packet_data = packet->GetData();

                string login = UTIL_ExtractCString(packet_data, 4);
                string game_name = UTIL_ExtractCString(packet_data, login.size() + 5);

                for (vector<CGame*>::iterator i = bot->m_GamesList.begin(); i != bot->m_GamesList.end(); ++i)
                    if ( (*i)->getName() == game_name )
                    {
                        for (vector<CGamePlayer>::iterator it = (*i)->m_GamePlayers.begin(); it != (*i)->m_GamePlayers.end(); ++it )
                            if ( (*it).GetName() == login )
                            {
                                (*i)->m_GamePlayers.erase( it );

                                for (list<CUser*>::iterator it = UsersList.begin(); it != UsersList.end(); ++it)
                                    for (vector<CChannel>::iterator c = (*it)->m_ChannelList.begin(); c != (*it)->m_ChannelList.end(); ++c)
                                     if ( (*c).m_name == (*i)->getName() && (*c).m_type == 3 )
                                       (*it)->GetSocket()->PutBytes( m_PUBProtocol->SendLobbyPlayers( game_name, bot->bot_ip, bot->bot_gameport, (*i)->m_GamePlayers) );

                                break;
                            }

                        break;
                    }

      //          cout << "[GAME] " << login << " leaved from game " << game_name << endl;
                break;
            }

		    case CPUBProtocol::PUB_BOT_GAME_KEY:    // event join to the game
		    {
		        BYTEARRAY packet_data = packet->GetData();

				int length = packet_data[4];
				string key = string(packet_data.begin() + 5, packet_data.begin() + 4 + length + 1);

				int length_login = packet_data[4 + length + 1];
				string login = string(packet_data.begin() + 4 + length + 1, packet_data.begin() + 4 + length + 1 + length_login + 1);

                CUser* realuser = GetRealUser(key);

                if (realuser)
                {
                    packet->GetFromSocket()->PutBytes( m_PUBProtocol->SendRealName(realuser->GetName(), key) );
               //     cout << "[CLIENT] " << login << " joinned to game, send he's real name " << realuser->GetName() << endl;
                }
                else
                    packet->GetFromSocket()->PutBytes( m_PUBProtocol->SendAuthFailed() );

		        break;

            }
		}

		delete packet;
	}

    return true;
}

bool CServer :: ProcessClientPackets(CUser* usr)
{
    CTCPSocket* socket = usr->GetSocket();

    while ( !m_Packets.empty( ) )
	{
		CCommandPacket * packet = m_Packets.front( );
		m_Packets.pop( );

        if (packet->GetPacketType() == W3GS_HEADER_CONSTANT)
        {
            BYTEARRAY data = packet->GetData();

            if ( packet->GetID() == CPUBProtocol::W3GS_GAMEINFO )
            {
                BYTEARRAY ip = usr->GetSocket()->GetIP();

                data[16] = ip[0];
                data[17] = ip[1];
                data[18] = ip[2];
                data[19] = ip[3];
            }
            // TODOTODO

            for (list<CUser*>::iterator it = UsersList.begin(); it != UsersList.end(); ++it)
                for ( vector<CChannel>::iterator cc = (*it)->m_ChannelList.begin(); cc != (*it)->m_ChannelList.end(); ++cc)
                {
                    if ( (*cc).m_name == "host1" )
                    {
                        if ( (*it) != usr )
                        {
                            usr->GetSocket()->PutBytes ( data );
                            cout << "[DEBUG] SEND W3GS_HEADER_CONSTANT" << endl;
                        }

                        break;
                    }
                }

        } else switch( packet->GetID( ) )
		{
		    case CPUBProtocol::PUB_CREATE_SAVEDGAME:
		    {
		        BYTEARRAY data = packet->GetData();

		        string nGameName = UTIL_ExtractCString( data, 4 );
		        string nSaveFile = UTIL_ExtractCString( data, nGameName.size() + 5 );

		        if ( data.size() < nGameName.size() + nSaveFile.size() + 10)
		        {
                    cout << "[PACKET] size error PUB_CREATE_SAVEDGAME" << endl;

                    break;  // packet size error
		        }

		        BYTEARRAY nMagicNumber = BYTEARRAY( data.begin() + nGameName.size() + nSaveFile.size() + 6, data.begin() + nGameName.size() + nSaveFile.size() + 10);

                cout << "[CREATE] " << nGameName << endl;
/*
                try
				{
				    string botip     = redis_client->hget("SAVEGAME:" + nGameName + ":" + nSaveFile, "botip" );
				    string sGamePort = redis_client->hget("SAVEGAME:" + nGameName + ":" + nSaveFile, "botgameport" );
				    string saveData  = redis_client->hget("SAVEGAME:" + nGameName + ":" + nSaveFile, "savedata" );

				    cout << "[saveData] " << saveData << " size " << saveData.size() << endl;

				    cout << "[DEBUG] " << "SAVEGAME:" + nGameName + ":" + nSaveFile << endl;

				    uint16_t botgameport = UTIL_ToUInt16( sGamePort );

                    if (botip.size())
                    {
                        for (deque<CBotData>::iterator i = BotList.begin(); i != BotList.end(); ++i)
                        {
 //                           if ( (*i).can_create_game /*(*i).bot_ip == botip && (*i).bot_gameport == botgameport*/// )
  /*                          {
                                (*i).GetSocket()->PutBytes( m_PUBProtocol->SendBotCreateSavedGame(nGameName, nSaveFile, nMagicNumber, usr->GetName(), saveData) );

                                break;
                            }
                        }
                    }

				} catch (redis::redis_error & e)
					{
						//cerr << "[REDIS] " << e.what() << endl;
					}
*/

		        break;
		    }
		    case CPUBProtocol::PUB_CREATE_GAME:
		    {
		        BYTEARRAY data = packet->GetData();

				bool isLadder = data[4] > 0 ? true : false;
                bool isBalance = data[5]> 0 ? true : false;
                uint16_t nHoldListSize = data[6];
                bool nReserve = data[7] > 0 ? true : false;

                uint16_t nMinScore = UTIL_ByteArrayToUInt16( BYTEARRAY( data.begin() + 8, data.begin() + 10), false );
                uint16_t nMaxScore = UTIL_ByteArrayToUInt16( BYTEARRAY( data.begin() + 10, data.begin() + 12), false );

                string nGameMode = UTIL_ExtractCString( data, 12 );
                string nGameName = UTIL_ExtractCString( data, nGameMode.size() + 13 );
                string nLocationName = UTIL_ExtractCString( data, nGameMode.size() + nGameName.size() + 14);

                vector<string> nHoldList;
                uint16_t mark = 15 + nGameMode.size() + nGameName.size() + nLocationName.size();

                if (nHoldListSize > 11)          // TODOTODO TEST
                    nHoldListSize = 11;

                for ( uint16_t i = 0; i < nHoldListSize; ++i)
                {
                    string nName = UTIL_ExtractCString( data, mark );

                    nHoldList.push_back( nName );

                    mark += nName.size() + 1;
                }

                bool is_GameCreated = false;
                double nScore = 1000.0;
/*
				try
				{
				    string score = redis_client->zscore("DOTA_ELO:scores", usr->GetName() );


				    if ( score != "**nonexistent-key**" )
                       nScore = UTIL_ToDouble(score);

				} catch (redis::redis_error & e)
					{
						//cerr << "[REDIS] " << e.what() << endl;
					}
*/

					if ( nScore < nMinScore)
					{
                        usr->GetSocket()->PutBytes( m_PUBProtocol->SendPlayerLowScore( nScore ) );

                        break;
					}

                    for (vector<CBotData>::iterator i = BotList.begin(); i != BotList.end(); ++i)
                    {
                        if ( ( (*i).bot_channel == nLocationName || nLocationName.empty() ) && (*i).can_create_game )
                        {
                            bool isGameCreated = false;

                            for (vector<CGame*>::iterator c = (*i).m_GamesList.begin(); c != (*i).m_GamesList.end(); ++c)
                            {
                                if ( !(*c)->isStarted() )
                                {
                                    isGameCreated = true;
                                    break;
                                }
                            }

                            if ( !isGameCreated )
                            {
                                (*i).GetSocket()->PutBytes( m_PUBProtocol->SendBotCreateGame(usr->GetName(),
                                                                                              nGameName,
                                                                                              nGameMode,
                                                                                              nHoldList,
                                                                                              nMinScore,
                                                                                              nMaxScore,
                                                                                              isLadder,
                                                                                              isBalance ));

                                cout << "[USER] Game created " << nGameName << " location " << nLocationName << endl;

                                is_GameCreated = true;

                                break;
                            }

                        }
                    }

                    if (!is_GameCreated)
                        usr->GetSocket()->PutBytes( m_PUBProtocol->SendUnableToCreate() );

		        break;
		    }

		    case CPUBProtocol::PUB_GETSCORE:
		    {
                BYTEARRAY packet_data = packet->GetData();
                string login = UTIL_ExtractCString( packet_data, 4 );
/*
				try
				{
				    string score = redis_client->zscore("DOTA_ELO:scores", login );
				    uint32_t player_games_count = redis_client->llen("UID:" + login + ":games");

				    if ( score != "**nonexistent-key**" )
                        usr->GetSocket()->PutBytes( m_PUBProtocol->SendScorePlayer(login , UTIL_ToString( round( UTIL_ToDouble(score))), player_games_count ));

				} catch (redis::redis_error & e)
					{
						//cerr << "[REDIS] " << e.what() << endl;
					}
*/

		        break;
		    }

		    case CPUBProtocol::PUB_JOINROOM:
		    {
		        BYTEARRAY packet_data = packet->GetData();

		        uint16_t gameport = UTIL_ByteArrayToUInt16( packet_data, false, 4 );
		        unsigned char type = packet_data[6];

                string login = UTIL_ExtractCString( packet_data, 7);
                string roomname = UTIL_ExtractCString( packet_data, login.size() + 8 );
                string botip = UTIL_ExtractCString( packet_data, login.size() + roomname.size() + 9);

                usr->m_ChannelList.push_back( CChannel(roomname, type, botip, gameport ) );

                if ( type == 3 && usr->GetSocket() )
                {
                    for (vector<CBotData>::iterator i = BotList.begin(); i != BotList.end(); ++i)
                    {
                        if ( (*i).bot_ip == botip && (*i).bot_gameport == gameport )
                        {
                            for (vector<CGame*>::iterator c = (*i).m_GamesList.begin(); c != (*i).m_GamesList.end(); ++c)
                                if ( (*c)->getName() == roomname )
                                {
                                    usr->GetSocket()->PutBytes( m_PUBProtocol->SendLobbyPlayers( roomname, botip, gameport, (*c)->m_GamePlayers ));

                                    uint16_t num_message = (*c)->m_GameMsg.size();

                                    if (num_message > 10 )
                                        num_message = 10;

                                    for (vector<CGameMsg>::iterator g = (*c)->m_GameMsg.end() - num_message; g != (*c)->m_GameMsg.end(); ++g )
                                        usr->GetSocket()->PutBytes( m_PUBProtocol->SendChatFromGame( (*g).GetFrom(), (*i).bot_ip, roomname, (*i).bot_gameport, (*g).GetMsg()) );

                                    break;
                                }

                            break;
                        }

                    }


                }

                cout << "[SERVER] Player " << login << " join the room " << roomname << endl;

                break;
		    }

		    case CPUBProtocol::PUB_LEAVEROOM:
		    {
		        BYTEARRAY packet_data = packet->GetData();

		        uint16_t gameport = UTIL_ByteArrayToUInt16( packet_data, false, 4 );
		        unsigned char type = packet_data[6];

                string login = UTIL_ExtractCString( packet_data, 7);
                string roomname = UTIL_ExtractCString( packet_data, login.size() + 8 );
                string botip = UTIL_ExtractCString( packet_data, login.size() + roomname.size() + 9);

                for (vector<CChannel>::iterator i = usr->m_ChannelList.begin(); i != usr->m_ChannelList.end(); ++i)
                    if ( (*i).m_name == roomname )
                    {
                            usr->m_ChannelList.erase(i);
                            break;
                    }

                cout << "[SERVER] Player " << login << " leave the room " << roomname << endl;

                break;
		    }

		    case CPUBProtocol::PUB_CHATTOGAME:
		    {
		        BYTEARRAY packet_data = packet->GetData();

                uint16_t bot_gameport = UTIL_ByteArrayToUInt16( packet_data, false, 4 );
                string bot_ip = UTIL_ExtractCString(packet_data, 6 );
                string message = UTIL_ExtractCString(packet_data, bot_ip.size() + 7);

                for (vector<CBotData>::iterator i = BotList.begin(); i != BotList.end(); ++i)
                    if ( (*i).bot_ip == bot_ip && bot_gameport == (*i).bot_gameport)
                        if ( (*i).GetSocket() && GetTime() - usr->m_last_chat_to_game_send > 4 )
                        {
                            (*i).GetSocket()->PutBytes(m_PUBProtocol->SendChatToGame(usr->GetName(), message));
                            usr->m_last_chat_to_game_send = GetTime();

                            cout << "[GAME] Chat to game from " << usr->GetName() << " msg " << message << " bot_ip = " << bot_ip << " gameport " << bot_gameport << endl;
                        }
		        break;
		    }

            case CPUBProtocol::PUB_PONG:
		    {
		        usr->last_pong_recv = GetTime();
		        break;
		    }

		    case CPUBProtocol::PUB_VERSION: // event to check client version
		    {
                    BYTEARRAY packet_data = packet->GetData();
                    string str_version = UTIL_ExtractCString(packet_data, 4);

 //                   cout <<  "[PACKET] CheckVersion - client version " << str_version << endl;

                    if ( packet_data.size() > 5 + str_version.size() )
                    {
                        BYTEARRAY cData = UTIL_Uncompress( BYTEARRAY( packet_data.begin() + 5 + str_version.size(), packet_data.end() ) );

                        uint16_t size = UTIL_ByteArrayToUInt16( cData, false, 0 );
                        uint16_t mark = 2;

                        vector<CUpdaterFile> testFiles;
                        vector<CUpdaterFile> notCompareFiles;

                        for ( uint16_t i = 0; i < size; ++i )
                        {
                            uint32_t nFileSize = UTIL_ByteArrayToUInt32( cData, false, mark );
                            string nFileName = UTIL_ExtractCString( cData, mark + 4 );
                            string nFileHash = UTIL_ExtractCString( cData, mark + 5 + nFileName.size() );

                            mark += 6 + nFileName.size() + nFileHash.size();

                            testFiles.push_back( CUpdaterFile( nFileName, nFileHash, nFileSize, 0) );
                        }

                        if ( !m_Updater->CheckFiles(testFiles, notCompareFiles) )
                        {
                            uint32_t nSize = 0;

                            for ( vector<CUpdaterFile>::iterator i = notCompareFiles.begin(); i < notCompareFiles.end(); ++i )
                                nSize +=  file_size( m_Updater->GetLocalDownloadUrl() + (*i).getName() );

                             socket->PutBytes( m_PUBProtocol->SendVersionFailed( notCompareFiles, m_Updater->GetDownloadUrl(), nSize ) );

  //                           cout << " check failed " << endl;
                        } else
                        {
                             socket->PutBytes( m_PUBProtocol->SendVersionOk() );
                             usr->setVersionChecked(true);

                             break;
                        }

                    }

                    if ( m_ClientVersion == str_version)
                    {
                         socket->PutBytes( m_PUBProtocol->SendVersionOk() );

                         usr->setVersionChecked(true);

   //                      cout << ", ok." << endl;
                    }
                        else
                    {

                        if ( find(m_AllreadySendVersionInvalid.begin(), m_AllreadySendVersionInvalid.end(), usr->GetName()) == m_AllreadySendVersionInvalid.end() )
                        {
                             //socket->PutBytes( m_PUBProtocol->SendVersionFailed( vector<CUpdaterFile> (), client_last_url) );

 //                            cout << ", too old. Login failed. " << endl;

                             usr->setVersionChecked(false);

                             m_AllreadySendVersionInvalid.push_back( usr->GetName() );
                        }
                    }

                break;
		    }


			case CPUBProtocol::PUB_AUTH_HEADER : // event to try auth
			{
                BYTEARRAY packet_data = packet->GetData();

                int login_length = packet_data[4];
                int pass_length = packet_data[5];
                int key_length = packet_data[6];

                string login, lower_login, pass, oldkey;

                if (login_length)
                    login  = string( packet_data.begin() + 7, packet_data.begin() + 7 + login_length );
                if (pass_length)
                    pass   = string( packet_data.begin() + 7 + login_length, packet_data.begin() + 7 + login_length + pass_length);
                if (key_length)
                    oldkey = string( packet_data.begin() + 7 + login_length + pass_length, packet_data.end());

//			    cout << "[PACKET] AUTH - login " << login << ", pass " << pass << endl;

			    lower_login.resize( login.size() );
			    transform( login.begin( ), login.end( ), lower_login.begin( ), (int(*)(int))tolower );

			    bool accept = false;

			    if ( !usr->isVersionChecked() )
			    {
			        if ( find(m_AllreadySendVersionInvalid.begin(), m_AllreadySendVersionInvalid.end(), usr->GetName()) == m_AllreadySendVersionInvalid.end() )
			        {
                        cout << "[SERVER] Auth login " << login << " failed, due the invalid version. Game functions wiil be disable." << endl;

                        socket->PutBytes( m_PUBProtocol->SendAuthFailed() );

                        m_AllreadySendVersionInvalid.push_back( usr->GetName() );
                        break;
			        }

			        break;
			    }

			    if (!DBUsersList.empty())
                {
 //                   cout << sha1(lower_login + pass) << endl;

                    cout << "[SERVER] Client " << UsersList.size() << " login " << login << " ";

                    for (list<CUser*>::iterator it = DBUsersList.begin(); it != DBUsersList.end(); it++)
                        if  ( (*it)->GetLowerName() == lower_login &&
                            ( (*it)->GetHashPassword() == md5(pass) || (*it)->GetHashPassword() == sha1( (*it)->GetLowerName() + pass) ))
                        {
                            cout << "accepted." << endl;
                            accept = true;

                            string send_key = string();

/*
                            for (list<CUser*>::iterator ui = UsersList.begin(); ui != UsersList.end(); )
                            {
                                if ( (*ui)->GetName() == login )
                                    {
                                    //if (*ui)
                                    //    (*ui)->SetStatus(CLIENT_NOT_AUTH);

                                        ui = UsersList.erase(ui);
                                        continue;
                                    }

                                ++ui;
                            }
*/


                            login = (*it)->GetName();

                            usr->SetName(login);
                            usr->SetHashPassword(pass);
                            usr->SetKey(GetUniqKey(login + md5(md5(pass) + pass) + login + lower_login));
                            usr->SetStatus( CLIENT_AUTH ); // Auth accepted

                            accept = true;
                            send_key = usr->GetKey();
  //                          cout << "New user " << usr->GetName() << endl;

                            usr->GetSocket()->PutBytes( m_PUBProtocol->SendAuthAccept(login, pass, send_key, BotList) );
                            usr->GetSocket()->PutBytes( m_PUBProtocol->SendCurrentOnline( UsersList.size() ) );

                            UpdateRunningGamesList();

                            break;
                    }

                    if (!accept)
                    {
                        usr->GetSocket()->PutBytes( m_PUBProtocol->SendAuthFailed() );

                        cout << "failed. ( Bad login/password )" << endl;

                        break;
                    }

                }

			    break;
            }
		} // PUB_AUTH_HEADER

		delete packet;
	}

    return true;
}

bool CServer ::Update(long usecBlock)
{
	unsigned int NumFDs = 0;
	// take every socket we own and throw it in one giant select statement so we can block on all sockets

	int nfds = 0;
	fd_set fd;
	fd_set send_fd;
	FD_ZERO( &fd );
	FD_ZERO( &send_fd );

	// 1. The game server listener

	m_MainServer->SetFD( &fd, &send_fd, &nfds );
	NumFDs++;

	// 2. The main clients socket

    for (list<CUser*>::iterator it = UsersList.begin(); it != UsersList.end(); it++)
    {
        if ( (*it) && (*it)->GetSocket() && (*it)->GetSocket()->GetConnected() && !(*it)->GetSocket()->HasError() )
        {
            (*it)->GetSocket()->SetFD( &fd, &send_fd, &nfds);
            NumFDs++;
        }

    }

    // 3. Bots socket

    for (vector<CBotData>::iterator ib = BotList.begin(); ib != BotList.end();ib++)
    {
        if ( (*ib).GetSocket() && (*ib).GetSocket()->GetConnected( ) && !(*ib).GetSocket()->HasError() )
        {
            (*ib).GetSocket()->SetFD( &fd, &send_fd, &nfds);
            NumFDs++;
        }
    }

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = usecBlock;

	struct timeval send_tv;
	send_tv.tv_sec = 0;
	send_tv.tv_usec = 0;

#ifdef WIN32
	select( 1, &fd, NULL, NULL, &tv );
	select( 1, NULL, &send_fd, NULL, &send_tv );
#else
	select( nfds + 1, &fd, NULL, NULL, &tv );
	select( nfds + 1, NULL, &send_fd, NULL, &send_tv );
#endif

	if( NumFDs == 0 )
		MILLISLEEP( 50 );

    // 1. Client sockets

	CTCPSocket* tmpSocket = m_MainServer->Accept( &fd );

	if (tmpSocket)
	{
        static uint32_t max = 0;

	//    cout << "[SRV] Client number " << UsersList.size() << " connected.";
/*
	    if ( UsersList.size() > max )
	    {
            time_t current_time = time( NULL );
            string c_current_time = UTIL_ToString( current_time );

            max = UsersList.size();

            redis_client->set("GLOBAL:usersOnline:max:" + c_current_time, UTIL_ToString( max ) );
            redis_client->rpush("GLOBAL:usersOnline:max:timestamps", UTIL_ToString( current_time ) );
	    }
*/
	    tmpSocket -> SetNoDelay( true );

	    UsersList.push_back( new CUser(tmpSocket) );
	}

    BYTEARRAY nToAllBytes;

    if ( m_toAllPackets.size() )
    {
        nToAllBytes = m_toAllPackets.front();

        m_toAllPackets.pop();
    }

    for (list<CUser*>::iterator it = UsersList.begin(); it != UsersList.end(); )
    {
        if (
            ((  (*it)->GetStatus() != CLIENT_AUTH) && (GetTime() - (*it)->GetSocket()->GetCreatedTime() > 5))
            || !(*it)->GetSocket()
            || !(*it)->GetSocket() -> GetConnected()
            ||  (*it)->GetSocket() -> HasError()
            || ( GetTime() - (*it)->last_pong_recv > 600 )
           )
        {
            delete *it;
            it = UsersList.erase(it);

            continue;
        }

            (*it)->GetSocket()->PutBytes( nToAllBytes );

            if ( GetTime() - (*it)->last_ping_send > 30 )
            {
                (*it)->GetSocket()->PutBytes( m_PUBProtocol->SendPing() );
                (*it)->last_ping_send = GetTime();
            }

            if ( GetTime() - last_bot_update_time >= 60 )
                (*it)->GetSocket()->PutBytes( m_PUBProtocol->SendCurrentOnline( UsersList.size() ) );

            (*it)->GetSocket()->DoRecv( &fd );

            if ( !ExtractClientPackets( (*it)->GetSocket() ) )
            {
                cout << "[SERVER] invalid packet from " << (*it)->GetSocket()->GetIPString() << ", disconnected." << endl;

                delete *it;
                it = UsersList.erase( it );

                continue;
            }

            ProcessClientPackets( (*it) );

            (*it)->GetSocket()->DoSend( &send_fd );

            ++it;
    }

    // 2. Bot sockets

    for (vector<CBotData>::iterator ib = BotList.begin(); ib != BotList.end(); ++ib)
    {
        bool connected = true;

        if ((*ib).GetSocket() && (*ib).GetSocket()->HasError())
        {
            if ( (GetTime() - last_bot_update_time >= 60) )
                cout << "[SERVER] Socket to bot error " << (*ib).GetSocket()->GetErrorString() << endl;

            connected = false;
        }
        else if ( !(*ib).GetSocket()->GetConnected() && !(*ib).GetSocket()->GetConnecting())
        {
            if ( (GetTime() - last_bot_update_time >= 60) )
                cout << "[SERVER] Socket to bot disconnected" << endl;

            connected = false;
        }
        else if ((*ib).GetSocket()->GetConnecting())
        {
            if ((*ib).GetSocket()->CheckConnect())
            {
                if ( (GetTime() - last_bot_update_time >= 60) )
                    cout << "[SERVER] Socket to the bot connected" << endl;
            }
        }
        else if ( (*ib).GetSocket()->GetConnected() )
            {
                (*ib).GetSocket()->DoRecv( &fd );

                if ( !ExtractBotsPackets( (*ib).GetSocket() ) )
                {
                    cout << "[SERVER] invalid packet from " << (*ib).GetSocket()->GetIPString() << ", disconnected." << endl;

                    (*ib).GetSocket()->Disconnect();

                    connected = false;
                }

                ProcessBotsPackets( &(*ib) );

                (*ib).GetSocket()->DoSend( &send_fd );

            }

            if (!connected && (GetTime() - last_bot_update_time >= 60))
            {
                (*ib).GetSocket()->Reset();
                (*ib).GetSocket()->Connect(string(), (*ib).bot_ip, (*ib).bot_command_port );
                (*ib).GetSocket()->SetNoDelay(true);
                (*ib).m_GamesList.clear();
            }

    }

    // Update bot connections

    if ( GetTime() - last_bot_update_time >= 60 )
    {
        UpdateUsersList();

        time_t current_time = time( NULL );
        string c_current_time = UTIL_ToString( current_time );

 //       redis_client->set("GLOBAL:usersOnline:" + c_current_time, UTIL_ToString(UsersList.size()) );
 //       redis_client->rpush("GLOBAL:userOnline:timestamps", c_current_time );

        last_bot_update_time = GetTime();
    }

	return 0;
}
