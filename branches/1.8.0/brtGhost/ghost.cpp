/*

   Copyright [2011] [Igor Bygaev]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST++ PROJECT: http://codelain.com/

*/

#include "ghost.h"
#include "util.h"
#include "crc32.h"
#include "sha1.h"
#include "csvparser.h"
#include "config.h"
#include "language.h"
#include "socket.h"
#include "ghostdb.h"
#include "ghostdbsqlite.h"
#include "ghostdbmysql.h"
#include "bnet.h"
#include "map.h"
#include "packed.h"
#include "savegame.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "gpsprotocol.h"
#include "game_base.h"
#include "game.h"
#include "game_admin.h"
#include "gameplayer.h"

#include "bnetprotocol.h"
#include "pubprotocol.h"

#include <cstring>
#include <signal.h>
#include <stdlib.h>

#ifdef WIN32
 #include <ws2tcpip.h>		// for WSAIoctl
#endif

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/signals2/mutex.hpp>

using namespace boost :: filesystem;

#define __STORMLIB_SELF__
#include <stormlib/StormLib.h>

#ifdef WIN32
 #include <process.h>
#else
 #include <unistd.h>
#endif

#ifdef WIN32
 #include <windows.h>
 #include <wininet.h>
 #include <winsock.h>
#endif

#include <time.h>

#ifndef WIN32
 #include <sys/time.h>
#endif

#ifdef __APPLE__
 #include <mach/mach_time.h>
#endif

#ifdef WIN32
 LARGE_INTEGER gHighPerfStart;
 LARGE_INTEGER gHighPerfFrequency;
#endif

string	   gCFGFile;
ofstream  *gLog = NULL;
CGHost	  *gGHost = NULL;
CConfigData* gConfig = NULL;
CLanguage *m_Language = NULL;	// todotodo - Use singelton ?
CbrtServer* brtServer = NULL;	// Class for listen remote commands from brtServer			

boost::mutex printMutex;

string tr(const string& lang_id)					{ return m_Language->GetLang(lang_id);	   } 
string tr(const string& lang_id, const string& v1)  { return m_Language->GetLang(lang_id, v1); }

string tr(const string& lang_id,  const string& v1, const string& s1,
										string v2, string s2,
										string v3, string s3,
										string v4, string s4,
										string v5, string s5,
										string v6, string s6,
										string v7, string s7,
										string v8, string s8,
										string v9, string s9,
										string v10, string s10,
										string v11, string s11,
										string v12, string s12,
										string v13, string s13,
										string v14, string s14,
										string v15, string s15,
										string v16, string s16,
										string v17, string s17,
										string v18, string s18,
										string v19, string s19)
{
	return m_Language->GetLang(lang_id, v1, s1, v2, s2, v3, s3, v4, s4, v5, s5, v6, s6, v7, s7, v8, s8, v9, s9, v10, s10, v11, s11, v12, s12, v13, s13, v14, s14, v15, s15, v16, s16, v17, s17, v18, s18, v19, s19);
}

int GetPID()
{
	// This is used to create an unique warden cookie number.
#ifdef WIN32
    return _getpid();
#else
    return getpid();
#endif
}

uint32_t GetTime( )
{
	return GetTicks( ) / 1000;
}

uint32_t GetTicks( )
{
#ifdef WIN32
	// don't use GetTickCount anymore because it's not accurate enough (~16ms resolution)
	// don't use QueryPerformanceCounter anymore because it isn't guaranteed to be strictly increasing on some systems and thus requires "smoothing" code
	// use timeGetTime instead, which typically has a high resolution (5ms or more) but we request a lower resolution on startup

	if (gConfig && gConfig->m_newTimer)
		return timeGetTime( );
	return GetTickCount();
#elif __APPLE__
	uint64_t current = mach_absolute_time( );
	static mach_timebase_info_data_t info = { 0, 0 };
	// get timebase info
	if( info.denom == 0 )
		mach_timebase_info( &info );
	uint64_t elapsednano = current * ( info.numer / info.denom );
	// convert ns to ms
	return elapsednano / 1e6;
#else
	uint32_t ticks;
	struct timespec t;
	clock_gettime( CLOCK_MONOTONIC, &t );
	ticks = t.tv_sec * 1000;
	ticks += t.tv_nsec / 1000000;
	return ticks;
#endif
}

void SignalCatcher2( int s )
{
	CONSOLE_Print( "[!!!] caught signal " + UTIL_ToString( s ) + ", exiting NOW" );

	if( gGHost )
	{
		if( gGHost->m_Exiting )
			exit( 1 );
		else
			gGHost->m_Exiting = true;
	}
	else
		exit( 1 );
}

void SignalCatcher( int s )
{
	// signal( SIGABRT, SignalCatcher2 );
	signal( SIGINT, SignalCatcher2 );

	CONSOLE_Print( "[!!!] caught signal " + UTIL_ToString( s ) + ", exiting nicely" );

	if( gGHost )
		gGHost->m_ExitingNice = true;
	else
		exit( 1 );
}

// initialize ghost

void CONSOLE_PrintThread( string message )
{
	printMutex.lock();

	string::size_type length = message.find( "]", 0 );

	if ( length < 34 )
		message.insert(1, 34 - length, ' ');
	else
	if ( length < 45 )
		message.insert(1, 45 - length, ' ');

	cout << message << endl;

	if( !gConfig->logfile.empty( ) )
	{
		if( gConfig->logmethod == 1 )
		{
			ofstream Log;
			Log.open( gConfig->logfile.c_str( ), ios :: app );

			if( !Log.fail( ) )
			{
				time_t Now = time( NULL );
				string Time = asctime( localtime( &Now ) );

				// erase the newline

				Time.erase( Time.size( ) - 1 );
				Log << "[" << Time << "] " << message << endl;
				Log.close( );
			}
		}
		else if( gConfig->logmethod == 2 )
		{
			if( gLog && !gLog->fail( ) )
			{
				time_t Now = time( NULL );
				string Time = asctime( localtime( &Now ) );

				// erase the newline

				Time.erase( Time.size( ) - 1 );
				*gLog << "[" << Time << "] " << message << endl;
				gLog->flush( );
			}
		}
	}

	printMutex.unlock();
}

void CONSOLE_Print( const string& message )
{
	if ( gConfig->logmethod && !gConfig->logfile.empty( ) )
		boost::thread PrintThread( CONSOLE_PrintThread, message );
	else
		CONSOLE_PrintThread( message );
}

void DEBUG_Print( string message )
{
	cout << message << endl;
}

void DEBUG_Print( BYTEARRAY b )
{
	cout << "{ ";

	for( unsigned int i = 0; i < b.size( ); i++ )
		cout << hex << (int)b[i] << " ";

	cout << "}" << endl;
}

//
// main
//

int main( int argc, char **argv )
{
#ifdef WIN32
	string ProcessID = UTIL_ToString(GetCurrentProcessId( ));
	string ProcessFile = "data\\process.ini";
#else
	string ProcessID = UTIL_ToString(getpid( ));
	string ProcessFile = "data/process.ini";
#endif

	gCFGFile = "ghost.cfg";

	if( argc > 1 && argv[1] )
		gCFGFile = argv[1];

	// read config file

	gConfig = new CConfigData();

	if (gConfig->Parse(gCFGFile))
		return 0;
	
	if( !gConfig->logfile.empty( ) )
	{
		if( gConfig->logmethod == 1 )
		{
			// log method 1: open, append, and close the log for every message
			// this works well on Linux but poorly on Windows, particularly as the log file grows in size
			// the log file can be edited/moved/deleted while GHost++ is running
		}
		else if( gConfig->logmethod == 2 )
		{
			// log method 2: open the log on startup, flush the log for every message, close the log on shutdown
			// the log file CANNOT be edited/moved/deleted while GHost++ is running

			gLog = new ofstream( );
			gLog->open( gConfig->logfile.c_str( ), ios :: app );
		}
	}

	CONSOLE_Print( "[GHOST] starting up" );

	if( !gConfig->logfile.empty( ) )
	{
		if( gConfig->logmethod == 1 )
			CONSOLE_Print( "[GHOST] using log method 1, logging is enabled and [" + gConfig->logfile + "] will not be locked" );
		else if( gConfig->logmethod == 2 )
		{
			if( gLog->fail( ) )
				CONSOLE_Print( "[GHOST] using log method 2 but unable to open [" + gConfig->logfile + "] for appending, logging is disabled" );
			else
				CONSOLE_Print( "[GHOST] using log method 2, logging is enabled and [" + gConfig->logfile + "] is now locked" );
		}
	}
	else
		CONSOLE_Print( "[GHOST] no log file specified, logging is disabled" );

	// catch SIGABRT and SIGINT

	// signal( SIGABRT, SignalCatcher );
	signal( SIGINT, SignalCatcher );

#ifndef WIN32
	// disable SIGPIPE since some systems like OS X don't define MSG_NOSIGNAL

	signal( SIGPIPE, SIG_IGN );
#endif

unsigned int TimerResolution = 0;

#ifdef WIN32
	// initialize timer resolution
	// attempt to set the resolution as low as possible from 1ms to 5ms

	for( unsigned int i = 1; i <= 5; i++ )
	{
		if( timeBeginPeriod( i ) == TIMERR_NOERROR )
		{
			TimerResolution = i;
			break;
		}
		else if( i < 5 )
			CONSOLE_Print( "[GHOST] error setting Windows timer resolution to " + UTIL_ToString( i ) + " milliseconds, trying a higher resolution" );
		else
		{
			CONSOLE_Print( "[GHOST] error setting Windows timer resolution" );
			return 1;
		}
	}

	CONSOLE_Print( "[GHOST] using Windows timer with resolution " + UTIL_ToString( TimerResolution ) + " milliseconds" );
#elif __APPLE__
	// not sure how to get the resolution
#else
	// print the timer resolution

	struct timespec Resolution;

	if( clock_getres( CLOCK_MONOTONIC, &Resolution ) == -1 )
		CONSOLE_Print( "[GHOST] error getting monotonic timer resolution" );
	else
	{
		CONSOLE_Print( "[GHOST] using monotonic timer with resolution " + UTIL_ToString( (double)( Resolution.tv_nsec / 1000 ), 2 ) + " microseconds" );
		TimerResolution = ( Resolution.tv_nsec / 1000000 );
	}
#endif

#ifdef WIN32
	// initialize winsock

	CONSOLE_Print( "[GHOST] starting winsock" );
	WSADATA wsadata;

	if( WSAStartup( MAKEWORD( 2, 2 ), &wsadata ) != 0 )
	{
		CONSOLE_Print( "[GHOST] error starting winsock" );
		return 1;
	}

	// increase process priority

	CONSOLE_Print( "[GHOST] setting process priority to \"above normal\"" );
	SetPriorityClass( GetCurrentProcess( ), ABOVE_NORMAL_PRIORITY_CLASS );
#endif

	// initialize ghost

	gGHost = new CGHost( gConfig );

	while( 1 )
	{
		// block for 50ms on all sockets - if you intend to perform any timed actions more frequently you should change this
		// that said it's likely we'll loop more often than this due to there being data waiting on one of the sockets but there aren't any guarantees
//		uint32_t nLastTime = GetTicks();

		if( gGHost->Update( 50000 ) )
			break;

//		CONSOLE_Print("LastUpdate " + UTIL_ToString( GetTicks() - nLastTime ));
	}

	// shutdown ghost

	CONSOLE_Print( "[GHOST] shutting down" );

	TimerResolution = gConfig->m_newTimerResolution;
	bool TimerStarted = gGHost->m_newTimerStarted;

	delete gGHost;
	gGHost = NULL;

#ifdef WIN32
	// shutdown winsock

	CONSOLE_Print( "[GHOST] shutting down winsock" );
	WSACleanup( );

	// shutdown timer

if (TimerStarted)
	timeEndPeriod( TimerResolution );
#endif

	if( gLog )
	{
		if( !gLog->fail( ) )
			gLog->close( );

		delete gLog;
	}

	delete gConfig;

	return 0;
}

bool CMDCheck (uint32_t cmd, uint32_t acc)
{
	uint32_t Mask = 1;
	if (cmd != 0)
		for (unsigned int k=1;k<=cmd;k++)
			Mask = Mask * 2;
	if (acc & Mask)
		return true;
	else
		return false;
}

uint32_t CMDAccessAll ()
{
	uint32_t Result = 1;
	uint32_t Mask = 1;
	for (unsigned int k=1;k<=gGHost->m_Commands.size()-1;k++)
	{
		Mask = Mask*2;
		Result += Mask;
	}
	return Result;
}

//
// CGHost
//

CGHost :: CGHost( CConfigData* nConfig )
{
	srand( (unsigned int)time(0) );

	m_Config = nConfig;

	m_Log = true;

	m_UDPSocket = new CUDPSocket( );
	m_UDPSocket->SetBroadcastTarget( m_Config->m_udp_broadcasttarget );
	m_UDPSocket->SetDontRoute( m_Config->m_udp_dontroute );

	m_ReconnectSocket = NULL;

	m_CurrentGame = NULL;

	CONSOLE_Print( "[GHOST] opening primary database" );

	if( m_Config->m_DBType == "mysql" )
	{
#ifdef GHOST_MYSQL
		m_DB = new CGHostDBMySQL( m_Config );
#else
		CONSOLE_Print( "[GHOST] warning - this binary was not compiled with MySQL database support, using SQLite database instead" );
		m_DB = new CGHostDBSQLite( m_Config );
#endif
	}
	else
		m_DB = new CGHostDBSQLite( m_Config );

	CONSOLE_Print( "[GHOST] opening secondary (local) database" );
	m_DBLocal = new CGHostDBSQLite( m_Config );

	// get a list of local IP addresses
	// this list is used elsewhere to determine if a player connecting to the bot is local or not

	CONSOLE_Print( "[GHOST] attempting to find local IP addresses" );

#ifdef WIN32
	// use a more reliable Windows specific method since the portable method doesn't always work properly on Windows
	// code stolen from: http://tangentsoft.net/wskfaq/examples/getifaces.html

	SOCKET sd = WSASocket( AF_INET, SOCK_DGRAM, 0, 0, 0, 0 );

	if( sd == SOCKET_ERROR )
		CONSOLE_Print( "[GHOST] error finding local IP addresses - failed to create socket (error code " + UTIL_ToString( WSAGetLastError( ) ) + ")" );
	else
	{
		INTERFACE_INFO InterfaceList[20];
		unsigned long nBytesReturned;

		if( WSAIoctl( sd, SIO_GET_INTERFACE_LIST, 0, 0, &InterfaceList, sizeof(InterfaceList), &nBytesReturned, 0, 0 ) == SOCKET_ERROR )
			CONSOLE_Print( "[GHOST] error finding local IP addresses - WSAIoctl failed (error code " + UTIL_ToString( WSAGetLastError( ) ) + ")" );
		else
		{
			int nNumInterfaces = nBytesReturned / sizeof(INTERFACE_INFO);

			for( int i = 0; i < nNumInterfaces; i++ )
			{
				sockaddr_in *pAddress;
				pAddress = (sockaddr_in *)&(InterfaceList[i].iiAddress);
				CONSOLE_Print( "[GHOST] local IP address #" + UTIL_ToString( i + 1 ) + " is [" + string( inet_ntoa( pAddress->sin_addr ) ) + "]" );
				m_LocalAddresses.push_back( UTIL_CreateByteArray( (uint32_t)pAddress->sin_addr.s_addr, false ) );
			}
		}

		closesocket( sd );
	}
#else
	// use a portable method

	char HostName[255];

	if( gethostname( HostName, 255 ) == SOCKET_ERROR )
		CONSOLE_Print( "[GHOST] error finding local IP addresses - failed to get local hostname" );
	else
	{
		CONSOLE_Print( "[GHOST] local hostname is [" + string( HostName ) + "]" );
		struct hostent *HostEnt = gethostbyname( HostName );

		if( !HostEnt )
			CONSOLE_Print( "[GHOST] error finding local IP addresses - gethostbyname failed" );
		else
		{
			for( int i = 0; HostEnt->h_addr_list[i] != NULL; i++ )
			{
				struct in_addr Address;
				memcpy( &Address, HostEnt->h_addr_list[i], sizeof(struct in_addr) );
				CONSOLE_Print( "[GHOST] local IP address #" + UTIL_ToString( i + 1 ) + " is [" + string( inet_ntoa( Address ) ) + "]" );
				m_LocalAddresses.push_back( UTIL_CreateByteArray( (uint32_t)Address.s_addr, false ) );
			}
		}
	}
#endif

	m_Language = NULL;
	m_Exiting = false;
	m_ExitingNice = false;
	m_Enabled = true;
	m_Version = "1.8.0";
	stringstream SS;
	string istr = string();
	m_DisableReason = string();
	m_RootAdmin = string();
	m_CookieOffset = GetPID() * 10;
	m_CallableDownloadFile = NULL;

	UTIL_ExtractStrings(CMD_string, m_Commands);
	m_HostCounter = 1;
	m_AutoHostServer = string();
	m_AutoHostGArena = false;
	m_AutoHostCountries = string();
	m_AutoHostCountries2 = string();
	m_AutoHostCountryCheck = false;
	m_AutoHostCountryCheck2 = false;
//	m_AutoHostAutoStartPlayers = 0;
	m_LastAutoHostTime = 0;
	m_AutoHostMatchMaking = false;
	m_AutoHostMinimumScore = 0.0;
	m_AutoHostMaximumScore = 0.0;
	m_AllGamesFinished = false;
	m_AllGamesFinishedTime = 0;

	if( m_Config->tft )
		CONSOLE_Print( "[GHOST] acting as Warcraft III: The Frozen Throne" );
	else
		CONSOLE_Print( "[GHOST] acting as Warcraft III: Reign of Chaos" );

	m_ScoresCount = 0;
	m_ScoresCountSet = false;
	m_AutoHosted = false;
	m_CalculatingScores = false;
	m_QuietRehost = false;
	m_Rehosted = false;
	m_Hosted = false;
	m_LastGameName = string();

	CheckConfigs();
	// load the battle.net connections
	// we're just loading the config data and creating the CBNET classes here, the connections are established later (in the Update function)
	unsigned int num = 0;
	for (vector<CBNetConfigContainer>::iterator i = m_Config->m_BNETcfg.begin(); i != m_Config->m_BNETcfg.end(); ++i)
	{
		m_BNETs.push_back( new CBNET( this, m_Config, (*i).Server, 
			(*i).ServerAlias, 
			(*i).BNLSServer, 
			(uint16_t)(*i).BNLSPort, 
			(uint32_t)(*i).BNLSWardenCookie + m_CookieOffset, 
			(*i).CDKeyROC, 
			(*i).CDKeyTFT, 
			(*i).CountryAbbrev, 
			(*i).Country, 
			(*i).LocaleID, 
			(*i).UserName, 
			(*i).UserPassword, 
			(*i).FirstChannel, 
			(*i).RootAdmin, 
			(*i).BNETCommandTrigger[0], 
			(*i).HoldFriends, 
			(*i).HoldClan, 
			(*i).PublicCommands, 
			(*i).War3Version, 
			(*i).EXEVersion, 
			(*i).EXEVersionHash, 
			(*i).PasswordHashType, 
			(*i).PVPGNRealmName, 
			(*i).MaxMessageLength, num++ ) );

		m_BNETs[m_BNETs.size()-1]->SetWhereis( (*i).Whereis );

		if (m_AutoHostServer.empty())
			m_AutoHostServer = (*i).Server;
	}

	if( m_BNETs.empty( ) )
		CONSOLE_Print( "[GHOST] warning - no battle.net connections found in config file" );

	// extract common.j and blizzard.j from War3Patch.mpq if we can
	// these two files are necessary for calculating "map_crc" when loading maps so we make sure to do it before loading the default map
	// see CMap :: Load for more information

	ExtractScripts( );

	// load the default maps (note: make sure to run ExtractScripts first)

	if( m_Config->m_DefaultMap.size( ) < 4 || m_Config->m_DefaultMap.substr( m_Config->m_DefaultMap.size( ) - 4 ) != ".cfg" )
	{
		m_Config->m_DefaultMap += ".cfg";
//		CONSOLE_Print( "[GHOST] adding \".cfg\" to default map -> new default is [" + m_Config->m_DefaultMap + "]" );
	}

	CConfig MapCFG;
	MapCFG.Read( m_Config->mapcfgpath + m_Config->m_DefaultMap );
	m_Map = new CMap( m_Config, &MapCFG, m_Config->mapcfgpath + m_Config->m_DefaultMap );

	if( !m_Config->m_AdminGameMap.empty( ) )
	{
		if( m_Config->m_AdminGameMap.size( ) < 4 || m_Config->m_AdminGameMap.substr( m_Config->m_AdminGameMap.size( ) - 4 ) != ".cfg" )
		{
			m_Config->m_AdminGameMap += ".cfg";
//			CONSOLE_Print( "[GHOST] adding \".cfg\" to default admin game map -> new default is [" + m_AdminGameMap + "]" );
		}

		CONSOLE_Print( "[GHOST] trying to load default admin game map" );
		CConfig AdminMapCFG;
		AdminMapCFG.Read( m_Config->mapcfgpath + m_Config->m_AdminGameMap );
		m_AdminMap = new CMap( m_Config, &AdminMapCFG, m_Config->mapcfgpath + m_Config->m_AdminGameMap );

		if( !m_AdminMap->GetValid( ) )
		{
			CONSOLE_Print( "[GHOST] default admin game map isn't valid, using hardcoded admin game map instead" );
			delete m_AdminMap;
			m_AdminMap = new CMap( m_Config );
		}
	}
	else
	{
		CONSOLE_Print( "[GHOST] using hardcoded admin game map" );
		m_AdminMap = new CMap( m_Config );
	}

	if( m_Config->m_AutoHostMapCFG.size( ) < 4 || m_Config->m_AutoHostMapCFG.substr( m_Config->m_AutoHostMapCFG.size( ) - 4 ) != ".cfg" )
	{
		m_Config->m_AutoHostMapCFG += ".cfg";
		CONSOLE_Print( "[GHOST] adding \".cfg\" to autohost map -> new one is [" + m_Config->m_AutoHostMapCFG + "]" );
	}

	CConfig AutohostMapCFG;
	AutohostMapCFG.Read( m_Config->m_AutoHostMapCFG );

	m_AutoHostMap = new CMap( m_Config, &AutohostMapCFG, m_Config->m_AutoHostMapCFG );

//	m_AutoHostMap = new CMap( *m_Map );
	m_SaveGame = new CSaveGame( );

	// load the iptocountry data

	LoadIPToCountryDataOpt();

	// external ip and country

	if (!m_Config->m_ExternalIP.empty())
	{
		m_ExternalIPL = ntohl(inet_addr(m_Config->m_ExternalIP.c_str()));
		m_Country = m_DBLocal->FromCheck(m_ExternalIPL);

		CONSOLE_Print( "[GHOST] External IP is " + m_Config->m_ExternalIP);
		CONSOLE_Print( "[GHOST] Country is " + m_Country);
	}

	// create the admin game
	if( m_Config->m_AdminGameCreate )
	{
		CONSOLE_Print( "[GHOST] creating admin game" );
		m_AdminGame = new CAdminGame( this, m_Config, m_AdminMap, NULL, m_Config->port_admingame, 0, "brtGHost Admin Game", m_Config->m_AdminGamePassword );

		if( m_Config->port_admingame == m_Config->port_host )
			CONSOLE_Print( "[GHOST] warning - admingame_port and bot_hostport are set to the same value, you won't be able to host any games" );
	}
	else
		m_AdminGame = NULL;

	if( m_BNETs.empty( ) && !m_AdminGame )
		CONSOLE_Print( "[GHOST] warning - no battle.net connections found and no admin game created" );

	if (m_Config->m_BrtServerEnable)
		brtServer = new CbrtServer( m_Config->port_command );

#ifdef GHOST_MYSQL
	CONSOLE_Print( "[GHOST] brtGHost Version " + m_Version + " (with MySQL support)" );
#else
	CONSOLE_Print( "[GHOST] brtGHost Version " + m_Version + " (without MySQL support)" );
#endif
}

CGHost :: ~CGHost( )
{
	delete brtServer; brtServer = NULL;

	delete m_ReconnectSocket; m_ReconnectSocket = NULL;

	for( vector<CTCPSocket *> :: iterator i = m_ReconnectSockets.begin( ); i != m_ReconnectSockets.end( ); i++ )
		delete *i;

//	delete m_GPSProtocol;
//	delete m_CRC;
//	delete m_SHA;

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		delete *i;

	delete m_CurrentGame; m_CurrentGame = NULL;
	delete m_AdminGame; m_AdminGame = NULL;

	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); i++ )
		delete *i;

	delete m_DB; m_DB = NULL;
	delete m_DBLocal; m_DBLocal = NULL;

	// warning: we don't delete any entries of m_Callables here because we can't be guaranteed that the associated threads have terminated
	// this is fine if the program is currently exiting because the OS will clean up after us
	// but if you try to recreate the CGHost object within a single session you will probably leak resources!

	if( !m_Callables.empty( ) )
		CONSOLE_Print( "[GHOST] warning - " + UTIL_ToString( m_Callables.size( ) ) + " orphaned callables were leaked (this is not an error)" );

	delete m_Language; m_Language = NULL;
	delete m_Map; m_Map = NULL;
	delete m_AdminMap; m_AdminMap = NULL;
	delete m_AutoHostMap; m_AutoHostMap = NULL;
	delete m_SaveGame; m_SaveGame = NULL;
}

CbrtServer :: CbrtServer( uint32_t nPort )
{
	nExiting = false;

	m_Port = nPort;

	m_CommandSocketServer = new CTCPServer( );
	m_CommandSocketServer->Listen( string( ), m_Port );
	m_CommandSocket = NULL;

	m_UpdateThread = new boost::thread( boost::bind( &CbrtServer::UpdateThread, this ) );

	CONSOLE_Print( "[GHOST] Listening brtServer on port [" + UTIL_ToString( m_Port ) +"]");
}

CbrtServer::~CbrtServer()
{
	nExiting = true;

	m_UpdateThread->join();
	delete m_UpdateThread;

	delete m_CommandSocketServer;
	m_CommandSocketServer = NULL;
}

void CbrtServer::UpdateThread()
{
	while (!nExiting)
	{
		Update( 40000 );
	}
}

void CbrtServer::Update( int usecBlock )
{
	int nfds = 0;
	fd_set fd;
	fd_set send_fd;
	FD_ZERO( &fd );
	FD_ZERO( &send_fd );

	if (m_CommandSocketServer)
		m_CommandSocketServer->SetFD( &fd, &send_fd, &nfds );

	if( m_CommandSocket && m_CommandSocket->GetConnected( ) && !m_CommandSocket->HasError( ))
		m_CommandSocket->SetFD( &fd, &send_fd, &nfds );

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

	// update remote PUB command socket

	CTCPSocket *NewSocket = m_CommandSocketServer->Accept( &fd );

	if (NewSocket && ( !m_CommandSocket || m_CommandSocket->HasError() || !m_CommandSocket->GetConnected()))
	{
		m_CommandSocket = NewSocket;
//		getNextGameID();

		CONSOLE_Print("[GHOST] brtServer connected to this bot. ");
	}

	if ( m_CommandSocket && m_CommandSocket->HasError() )
	{
		CONSOLE_Print ("[GHOST] Disconnected from brtServer, error " + m_CommandSocket->GetErrorString() );

		delete m_CommandSocket;
		m_CommandSocket = NULL;
	}

	// Process PUB command from BOT to command server
 
	if ( m_CommandSocket && !m_CommandSocket->HasError() && m_CommandSocket->GetConnected() )
	{
		m_CommandSocket->DoRecv( &fd );

		// Excracts command packet
		ExctactsCommandPackets();

		m_CommandSocket->DoSend( &send_fd );

		if ( m_CommandSocket->GetBytes()->size() == 0 )
		{
			if ( !m_PacketsToServer.empty() )
			{
				BYTEARRAY nBytes = m_PacketsToServer.front();

				m_CommandSocket->PutBytes( nBytes );

				m_PacketsToServer.pop();
			}
		}

	}
}

bool CbrtServer :: ExctactsCommandPackets()
{
	if( !m_CommandSocket )
		return false;

	// extract as many packets as possible from the socket's receive buffer and put them in the m_Packets queue

	string *RecvBuffer = m_CommandSocket->GetBytes( );
	BYTEARRAY Bytes = UTIL_CreateByteArray( (unsigned char *)RecvBuffer->c_str( ), RecvBuffer->size( ) );

	// a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

	while( Bytes.size( ) >= 4 )
	{
		if( Bytes[0] == PUB_HEADER_CONSTANT )
		{
			// bytes 2 and 3 contain the length of the packet

			uint16_t Length = UTIL_ByteArrayToUInt16( Bytes, false, 2 );

			if( Length >= 4 )
			{
				if( Bytes.size( ) >= Length )
				{
					m_CommandPackets.push( new CCommandPacket( Bytes[0], Bytes[1], BYTEARRAY( Bytes.begin( ), Bytes.begin( ) + Length ) ) );

					*RecvBuffer = RecvBuffer->substr( Length );
					Bytes = BYTEARRAY( Bytes.begin( ) + Length, Bytes.end( ) );
				}
				else
					return false;
			}
			else
			{
				//m_ErrorString = "received invalid packet from player (bad length)";
				return true;
			}
		}
		else
		{
			//m_ErrorString = "received invalid packet from player (bad header constant)";
			return false;
		}
	}

	return true;
}

bool CGHost :: ProcessCommandPackets()
{
    while ( !brtServer->m_CommandPackets.empty( ) )
	{
		CCommandPacket * packet = brtServer->m_CommandPackets.front( );
		brtServer->m_CommandPackets.pop( );

		switch( packet->GetID( ) )
		{
			/*
			case CPUBProtocol::PUB_CREATE_SAVEDGAME:
			{
				BYTEARRAY data = packet->GetData();

		        BYTEARRAY nGameName = UTIL_ExtractCString( data, 4 );
		        BYTEARRAY nSaveFile = UTIL_ExtractCString( data, nGameName.size() + 5 );

		        if ( data.size() < nGameName.size() + nSaveFile.size() + 10)
		        {
                    CONSOLE_Print("[PACKET] size error PUB_CREATE_SAVEDGAME" );

                    break;  // packet size error
		        }

		        BYTEARRAY nMagicNumber = BYTEARRAY( data.begin() + nGameName.size() + nSaveFile.size() + 6, data.begin() + nGameName.size() + nSaveFile.size() + 10);
				BYTEARRAY nOwner = UTIL_ExtractCString( data, nGameName.size() + nSaveFile.size() + 10 );
				BYTEARRAY nSaveData = BYTEARRAY(data.begin() + nGameName.size() + nSaveFile.size() + nOwner.size() + 11, data.end() );

				CONSOLE_Print("[SAVEdata] size = " + UTIL_ToString(nSaveData.size()));

				string FileNoPath = string( nSaveFile.begin(), nSaveFile.end() );

				string sGameName = string( nGameName.begin(), nGameName.end() );
				string sOwner = string( nOwner.begin(), nOwner.end() );
				string File = m_SaveGamePath + sOwner + sGameName + FileNoPath;

				CONSOLE_Print( "[GHOST] Creating new game" );

				m_SaveGame->LoadFromMemory( string(nSaveData.begin(), nSaveData.end()), false );
				m_SaveGame->ParseSaveGame( );
				m_SaveGame->SetFileName( File );
				m_SaveGame->SetFileNameNoPath( FileNoPath );
				m_SaveGame->SetMagicNumber( nMagicNumber );

				if ( m_SaveGame->GetEnforcePlayers().size() )
					m_EnforcePlayers = m_SaveGame->GetEnforcePlayers();

				CreateGame( m_Map, GAME_PRIVATE, true, sGameName, getNextGameID(), sOwner, sOwner, string( ), false );
				m_CurrentGame->m_isLadderGame = true;
				m_CurrentGame->m_isBalanceGame = false;
				m_CurrentGame->SetMinimumScore( 0 );
				m_CurrentGame->SetMaximumScore( 15000 );

				break;
			}
*/
			case CPUBProtocol::PUB_BOTCREATEGAME:
			{
				BYTEARRAY data = packet->GetData();

				bool isLadder = data[4] > 0 ? true : false;
				bool isBalance = data[5] > 0 ? true : false;
                uint16_t nHoldListSize = data[6];
				bool nReserve = data[7] > 0 ? true : false;

                uint16_t nMinScore = UTIL_ByteArrayToUInt16( BYTEARRAY( data.begin() + 8, data.begin() + 10), false );
                uint16_t nMaxScore = UTIL_ByteArrayToUInt16( BYTEARRAY( data.begin() + 10, data.begin() + 12), false );

                BYTEARRAY bGameMode = UTIL_ExtractCString( data, 12 );
                string nGameMode = string( bGameMode.begin(), bGameMode.end() );

                BYTEARRAY bGameName = UTIL_ExtractCString( data, nGameMode.size() + 13 );
                string nGameName = string( bGameName.begin(), bGameName.end() );

				BYTEARRAY bCreatorName = UTIL_ExtractCString( data, nGameMode.size() + nGameName.size() + 14 );
				string nCreatorName = string( bCreatorName.begin(), bCreatorName.end() );

                vector<string> nHoldList;
                uint16_t mark = 15 + nGameMode.size() + nGameName.size() + nCreatorName.size();

                for ( uint16_t i = 0; i < nHoldListSize; ++i)
                {
                    BYTEARRAY bName = UTIL_ExtractCString( data, mark );
                    string nName = string( bName.begin(), bName.end() );

                    nHoldList.push_back( nName );

                    mark += bName.size() + 1;
					CONSOLE_Print("[HOLD] " + nName );
                }

				CONSOLE_Print("[GAME] Create " + nGameName + " " + nGameMode + " " + nCreatorName + " " + UTIL_ToString( nMinScore ) + " " +  UTIL_ToString( nMaxScore ) );

				if (!m_CurrentGame)
				{
					CreateGame( m_Map, newGameGameState, false, nGameName, /*getNextGameID()*/ nCreatorName, nCreatorName, newGameServer, true );
//							  ( CMap *map, unsigned char gameState, bool saveGame, string gameName, string ownerName, string creatorName, string creatorServer, bool whisper );
					m_CurrentGame->m_isLadderGame = isLadder;
					m_CurrentGame->m_isBalanceGame = isBalance;

					for ( uint16_t i = 0; i < nHoldList.size(); ++i)
						m_CurrentGame->AddToReserved( nHoldList[i], 255 );

					m_CurrentGame->SetMinimumScore( nMinScore );
					m_CurrentGame->SetMaximumScore( nMaxScore );
					m_CurrentGame->SetHCL( nGameMode );

					if (!isLadder)
					{
						m_CurrentGame->m_DisableStats = false;
					}
				}
				

				break;
			}

			case CPUBProtocol::PUB_GETSCOREANS:
			{
				BYTEARRAY packet_data = packet->GetData();

				BYTEARRAY bLogin =  UTIL_ExtractCString( packet_data, 4);
				BYTEARRAY bScore = UTIL_ExtractCString( packet_data, 5 + bLogin.size() );

				string login = string(bLogin.begin(), bLogin.end());
				string sScore = string(bScore.begin(), bScore.end());
				double score = UTIL_ToDouble( sScore );

				if (m_CurrentGame)
				{
					CGamePlayer * Player = m_CurrentGame->GetPlayerFromName( login, false);

					if (Player)
					{
						Player->SetScoreS( UTIL_ToString( score, 2) );
						Player->SetRankS( "0" );
						Player->SetScore( score );
						Player->SetLeaveCount( 0 );

						if ( score < m_CurrentGame->m_MinimumScore  || score > m_CurrentGame->m_MaximumScore )
						{
							Player->SetDeleteMe( true );
							m_CurrentGame->EventPlayerDeleted( Player );

						} else
						{
							m_CurrentGame->SendAllChat( m_Language->GetLang("lang_1226", "$USER$", login, "$SCORE$", UTIL_ToString( score, 2) ));
							
							if (m_CurrentGame->m_isBalanceGame)
								m_CurrentGame->BalanceSlots();
						}
					}

					for( vector<CPotentialPlayer *> :: iterator j = m_CurrentGame->m_Potentials.begin( ); j != m_CurrentGame->m_Potentials.end( ); j++ )
					{
						if( (*j)->GetJoinPlayer( ) && (*j)->GetJoinPlayer( )->GetName( ) == login )
						{
							//if ( m_CurrentGame->m_MatchMakingMethod )
								m_CurrentGame->EventPlayerJoinedWithScore( *j, (*j)->GetJoinPlayer( ), score );

								CONSOLE_Print("EventPlayerJoinedWithScore");

								break;
						}

					}


				}

//				CONSOLE_Print("[SERVER] " + string(bLogin.begin(), bLogin.end()) + " score " + string(bScore.begin(), bScore.end()));

				break;
			}

			case CPUBProtocol::PUB_CHAT_TO_GAME:
			{
				BYTEARRAY packet_data = packet->GetData();

				BYTEARRAY blogin = UTIL_ExtractCString( packet_data, 4);
				BYTEARRAY bmsg = UTIL_ExtractCString( packet_data, blogin.size() + 5 );

				string login = string( blogin.begin(), blogin.end() );
				string msg = string( bmsg.begin(), bmsg.end() );

				if (m_CurrentGame && !m_CurrentGame->GetGameLoaded() && !m_CurrentGame->GetGameLoading() )
					m_CurrentGame->SendAllChat( login + ": " + msg );

				break;
			}

		    case CPUBProtocol::PUB_AUTH_NAME:
		    {
		        BYTEARRAY packet_data = packet->GetData();

				int length = packet_data[4];
				string login = string(packet_data.begin() + 5, packet_data.begin() + 5 + length);

				int length_login = packet_data[5 + length];
				string key = string(packet_data.begin() + 5 + length + 1, packet_data.begin() + 5 + length + length_login + 1);

//				UpdatePlayersNames(login, key);

		        break;
		    }

		}

	}

    return true;

}
/*
void CbrtServer :: brtServerThread()
{
	while (!nExiting)
	{
		int nfds = 0;
		fd_set fd;
		fd_set send_fd;
		FD_ZERO( &fd );
		FD_ZERO( &send_fd );

		m_CommandSocketServer->SetFD( &fd, &send_fd, &nfds );

		if( m_CommandSocket && m_CommandSocket->GetConnected( ) && !m_CommandSocket->HasError( ))
			m_CommandSocket->SetFD( &fd, &send_fd, &nfds );

		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 40000;

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

	// update remote PUB command socket

		CTCPSocket *NewSocket = m_CommandSocketServer->Accept( &fd );

		if (NewSocket && ( !m_CommandSocket || m_CommandSocket->HasError() || !m_CommandSocket->GetConnected()))
		{
			m_CommandSocket = NewSocket;
	//		getNextGameID();

			CONSOLE_Print("[GHOST] brtServer connected to this bot. ");
		}

	}

	delete m_CommandSocketServer;
	m_CommandSocketServer = NULL;

	delete m_CommandSocket;
	m_CommandSocket = NULL;
}
*/

bool CGHost :: Update( unsigned long usecBlock )
{
	// todotodo: do we really want to shutdown if there's a database error? is there any way to recover from this?
/*
	if( m_DB->HasError( ) && !nAlreadyDBErrorPrinted )
	{
		CONSOLE_Print( "[GHOST] database error - " + m_DB->GetError( ) );
		return true;
	}
*/

	if( m_DBLocal->HasError( ) )
	{
		CONSOLE_Print( "[GHOST] local database error - " + m_DBLocal->GetError( ) );
		return true;
	}

	// try to exit nicely if requested to do so

	if( m_ExitingNice )
	{
		if( !m_BNETs.empty( ) )
		{
			CONSOLE_Print( "[GHOST] deleting all battle.net connections in preparation for exiting nicely" );

			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
				delete *i;

			m_BNETs.clear( );
		}

		if( m_CurrentGame )
		{
			CONSOLE_Print( "[GHOST] deleting current game in preparation for exiting nicely" );
			delete m_CurrentGame;
			m_CurrentGame = NULL;
		}

		if( m_AdminGame )
		{
			CONSOLE_Print( "[GHOST] deleting admin game in preparation for exiting nicely" );
			delete m_AdminGame;
			m_AdminGame = NULL;
		}

		if( m_Games.empty( ) )
		{
			if( !m_AllGamesFinished )
			{
				CONSOLE_Print( "[GHOST] all games finished, waiting 60 seconds for threads to finish" );
				CONSOLE_Print( "[GHOST] there are " + UTIL_ToString( m_Callables.size( ) ) + " threads in progress" );
				m_AllGamesFinished = true;
				m_AllGamesFinishedTime = GetTime( );
			}
			else
			{
				if( m_Callables.empty( ) )
				{
					CONSOLE_Print( "[GHOST] all threads finished, exiting nicely" );
					m_Exiting = true;
				}
				else if( GetTime( ) - m_AllGamesFinishedTime >= 60 )
				{
					CONSOLE_Print( "[GHOST] waited 60 seconds for threads to finish, exiting anyway" );
					CONSOLE_Print( "[GHOST] there are " + UTIL_ToString( m_Callables.size( ) ) + " threads still in progress which will be terminated" );
					m_Exiting = true;
				}
			}
		}
	}

	// update callables

	for( vector<CBaseCallable *> :: iterator i = m_Callables.begin( ); i != m_Callables.end( ); )
	{
		if( (*i)->GetReady( ) )
		{
			m_DB->RecoverCallable( *i );
			delete *i;
			i = m_Callables.erase( i );
		}
		else
			i++;
	}

	// create the GProxy++ reconnect listener

	if( m_Config->gproxy_enable )
	{
		if( !m_ReconnectSocket )
		{
			m_ReconnectSocket = new CTCPServer( );

			if( m_ReconnectSocket->Listen( m_Config->m_BindAddress, m_Config->port_reconnect ) )
				CONSOLE_Print( "[GHOST] listening for GProxy++ reconnects on port " + UTIL_ToString( m_Config->port_reconnect ) );
			else
			{
				CONSOLE_Print( "[GHOST] error listening for GProxy++ reconnects on port " + UTIL_ToString( m_Config->port_reconnect ) );
				delete m_ReconnectSocket;
				m_ReconnectSocket = NULL;
				m_Config->gproxy_enable = false;	
			}
		}
		else if( m_ReconnectSocket->HasError( ) )
		{
			CONSOLE_Print( "[GHOST] GProxy++ reconnect listener error (" + m_ReconnectSocket->GetErrorString( ) + ")" );
			delete m_ReconnectSocket;
			m_ReconnectSocket = NULL;
			m_Config->gproxy_enable = false; // TODOTODO
		}
	}

	unsigned int NumFDs = 0;

	// take every socket we own and throw it in one giant select statement so we can block on all sockets

	int nfds = 0;
	fd_set fd;
	fd_set send_fd;
	FD_ZERO( &fd );
	FD_ZERO( &send_fd );

	// 1. all battle.net sockets

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		NumFDs += (*i)->SetFD( &fd, &send_fd, &nfds );

	// 2. the current game's server and player sockets

	if( m_CurrentGame )
		NumFDs += m_CurrentGame->SetFD( &fd, &send_fd, &nfds );

	// 3. the admin game's server and player sockets

	if( m_AdminGame )
		NumFDs += m_AdminGame->SetFD( &fd, &send_fd, &nfds );

	// 4. all running games' player sockets

	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); i++ )
		NumFDs += (*i)->SetFD( &fd, &send_fd, &nfds );

	// 5. the GProxy++ reconnect socket(s)

	if( m_Config->gproxy_enable && m_ReconnectSocket )
	{
		m_ReconnectSocket->SetFD( &fd, &send_fd, &nfds );
		NumFDs++;
	}

	for( vector<CTCPSocket *> :: iterator i = m_ReconnectSockets.begin( ); i != m_ReconnectSockets.end( ); i++ )
	{
		(*i)->SetFD( &fd, &send_fd, &nfds );
		NumFDs++;
	}

	// before we call select we need to determine how long to block for
	// previously we just blocked for a maximum of the passed usecBlock microseconds
	// however, in an effort to make game updates happen closer to the desired latency setting we now use a dynamic block interval
	// note: we still use the passed usecBlock as a hard maximum

	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); i++ )
	{
		if( (*i)->GetNextTimedActionTicks( ) * 1000 < usecBlock )
			usecBlock = (*i)->GetNextTimedActionTicks( ) * 1000;
	}

	// always block for at least 1ms just in case something goes wrong
	// this prevents the bot from sucking up all the available CPU if a game keeps asking for immediate updates
	// it's a bit ridiculous to include this check since, in theory, the bot is programmed well enough to never make this mistake
	// however, considering who programmed it, it's worthwhile to do it anyway

	if( usecBlock < 1000 )
		usecBlock = 1000;

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
	{
		// we don't have any sockets (i.e. we aren't connected to battle.net maybe due to a lost connection and there aren't any games running)
		// select will return immediately and we'll chew up the CPU if we let it loop so just sleep for 50ms to kill some time

		MILLISLEEP( 50 );
	}

	bool AdminExit = false;
	bool BNETExit = false;

	// update current game

	if( m_CurrentGame )
	{
		if( m_CurrentGame->Update( &fd, &send_fd ) )
		{
			CONSOLE_Print( "[GHOST] deleting current game [" + m_CurrentGame->GetGameName( ) + "]" );

#ifdef WIN32
			if (m_Config->m_wtv && m_CurrentGame->wtvprocessid != 0)
			{
				HANDLE hProcess = OpenProcess( PROCESS_ALL_ACCESS, FALSE, m_CurrentGame->wtvprocessid );

				if( m_CurrentGame->wtvprocessid != GetCurrentProcessId( ) && m_CurrentGame->wtvprocessid!= 0 )
				{
					CONSOLE_Print( "[WaaaghTV] shutting down wtvrecorder.exe" );

					if( !TerminateProcess(hProcess, 0) )
						CONSOLE_Print( "[WaaaghTV] an error has occurred when trying to terminate wtvrecorder.exe (1-1)" );
					else
						CloseHandle( hProcess );
					m_CurrentGame->wtvprocessid = 0;
				}
				else
					CONSOLE_Print( "[WaaaghTV] an error has occurred when trying to terminate wtvrecorder.exe (1-2)" );
			}
#endif

			delete m_CurrentGame;
			m_CurrentGame = NULL;

			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
			{
				(*i)->QueueGameUncreate( );
				(*i)->QueueEnterChat( );
			}

			if ( newGame )
			{
				for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
				{
					if ((*i)->GetServer()==newGameServer)
					{
						{
							CreateGame( m_Map, newGameGameState, false, newGameName, newGameUser, newGameUser, newGameServer, true );
							if (m_CurrentGame)
							{
								m_CurrentGame->m_GarenaOnly = newGameGArena;
								m_CurrentGame->m_Countries = newGameCountries;
								m_CurrentGame->m_Countries2 = newGameCountries2;
								m_CurrentGame->m_CountryCheck = newGameCountryCheck;
								m_CurrentGame->m_CountryCheck2 = newGameCountry2Check;
								m_CurrentGame->m_Providers = newGameProviders;
								m_CurrentGame->m_Providers2 = newGameProviders2;
								m_CurrentGame->m_ProviderCheck = newGameProvidersCheck;
								m_CurrentGame->m_ProviderCheck2 = newGameProviders2Check;
								newGame = false;
							}
						}
					}
				}

			}
		}
		else if( m_CurrentGame )
			m_CurrentGame->UpdatePost( &send_fd );
	}

	// update admin game

	if( m_AdminGame )
	{
		if( m_AdminGame->Update( &fd, &send_fd ) )
		{
			CONSOLE_Print( "[GHOST] deleting admin game" );
			delete m_AdminGame;
			m_AdminGame = NULL;
			AdminExit = true;
		}
		else if( m_AdminGame )
			m_AdminGame->UpdatePost( &send_fd );
	}

	// update running games
	uint32_t GameNr = 0;
	for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); )
	{
		if( (*i)->Update( &fd, &send_fd ) )
		{
			CONSOLE_Print( "[GHOST] deleting game [" + (*i)->GetGameName( ) + "]" );

#ifdef WIN32
			if (m_Config->m_wtv && (*i)->wtvprocessid != 0)
			{
				HANDLE hProcess = OpenProcess( PROCESS_ALL_ACCESS, FALSE, (*i)->wtvprocessid );

				if( (*i)->wtvprocessid != GetCurrentProcessId( ) && (*i)->wtvprocessid != 0 )
				{
					CONSOLE_Print( "[WaaaghTV] shutting down wtvrecorder.exe" );

					if( !TerminateProcess( hProcess, 0 ) )
						CONSOLE_Print( "[WaaaghTV] an error has occurred when trying to terminate wtvrecorder.exe (2-1)" );
					else
						CloseHandle( hProcess );
					(*i)->wtvprocessid = 0;
				}
				else
					CONSOLE_Print( "[WaaaghTV] an error has occurred when trying to terminate wtvrecorder.exe (2-2)" );
			}
#endif

/*
			if (m_HoldPlayersForRMK)
			{
				for( vector<CGamePlayer *> :: iterator j = (*i)->m_Players.begin( ); j != (*i)->m_Players.end( ); j++ )
				{
					if (m_PlayersfromRMK.length()==0)
						m_PlayersfromRMK +=" ";
					m_PlayersfromRMK +=(*j)->GetName();
				}
				CONSOLE_Print( "[GHOST] reserving players from [" + (*i)->GetGameName( ) + "]" );
			}
*/
			for( vector<CBNET *> :: iterator l = m_BNETs.begin( ); l != m_BNETs.end( ); l++ )
			{
				(*l)->m_LastGameCountRefreshTime = 0;
			}

			EventGameDeleted( *i );
			delete *i;
			i = m_Games.erase( i );
		}
		else
		{
			(*i)->UpdatePost( &send_fd );
			i++;
		}
		GameNr++;
	}

	if (m_Rehosted)
		m_Rehosted = false;

	if (m_Hosted)
		m_Hosted = false;

	/*
	// update mysql queue
	if (GetTicks() - m_MySQLQueueTicks > 1000)
	{
		bool done = false;
		m_MySQLQueueTicks = GetTicks();
		if (m_WarnForgetQueue.size()>0)
		{
			string wfname = m_WarnForgetQueue[0];
			m_WarnForgetQueue.erase(m_WarnForgetQueue.begin());
			m_Callables.push_back( m_DB->ThreadedWarnForget( wfname, m_GameNumToForgetAWarn ));
			done = true;
		}
	}
	*/
	// update dynamic latency printout

	if (GetTicks() - m_LastDynamicLatencyConsole > 36000)
	{
		m_LastDynamicLatencyConsole = GetTicks();
	}

	// update GProxy++ reliable reconnect sockets

	if( m_Config->gproxy_enable && m_ReconnectSocket )
	{
		CTCPSocket *NewSocket = m_ReconnectSocket->Accept( &fd );

		if( NewSocket )
			m_ReconnectSockets.push_back( NewSocket );
	}

	for( vector<CTCPSocket *> :: iterator i = m_ReconnectSockets.begin( ); i != m_ReconnectSockets.end( ); )
	{
		if( (*i)->HasError( ) || !(*i)->GetConnected( ) || GetTime( ) - (*i)->GetLastRecv( ) >= 10 )
		{
			delete *i;
			i = m_ReconnectSockets.erase( i );
			continue;
		}

		(*i)->DoRecv( &fd );
		string *RecvBuffer = (*i)->GetBytes( );
		BYTEARRAY Bytes = UTIL_CreateByteArray( (unsigned char *)RecvBuffer->c_str( ), RecvBuffer->size( ) );

		// a packet is at least 4 bytes

		if( Bytes.size( ) >= 4 )
		{
			if( Bytes[0] == GPS_HEADER_CONSTANT )
			{
				// bytes 2 and 3 contain the length of the packet

				uint16_t Length = UTIL_ByteArrayToUInt16( Bytes, false, 2 );

				if( Length >= 4 )
				{
					if( Bytes.size( ) >= Length )
					{
						if( Bytes[1] == CGPSProtocol :: GPS_RECONNECT && Length == 13 )
						{
							unsigned char PID = Bytes[4];
							uint32_t ReconnectKey = UTIL_ByteArrayToUInt32( Bytes, false, 5 );
							uint32_t LastPacket = UTIL_ByteArrayToUInt32( Bytes, false, 9 );

							// look for a matching player in a running game

							CGamePlayer *Match = NULL;

							for( vector<CBaseGame *> :: iterator j = m_Games.begin( ); j != m_Games.end( ); j++ )
							{
								if( (*j)->GetGameLoaded( ) )
								{
									CGamePlayer *Player = (*j)->GetPlayerFromPID( PID );

									if( Player && Player->GetGProxy( ) && Player->GetGProxyReconnectKey( ) == ReconnectKey )
									{
										Match = Player;
										break;
									}
								}
							}

							if( Match )
							{
								// reconnect successful!

								*RecvBuffer = RecvBuffer->substr( Length );
								Match->EventGProxyReconnect( *i, LastPacket );
								i = m_ReconnectSockets.erase( i );
								continue;
							}
							else
							{
								(*i)->PutBytes( CGPSProtocol::SEND_GPSS_REJECT( REJECTGPS_NOTFOUND ) );
								(*i)->DoSend( &send_fd );
								delete *i;
								i = m_ReconnectSockets.erase( i );
								continue;
							}
						}
						else
						{
							(*i)->PutBytes( CGPSProtocol::SEND_GPSS_REJECT( REJECTGPS_INVALID ) );
							(*i)->DoSend( &send_fd );
							delete *i;
							i = m_ReconnectSockets.erase( i );
							continue;
						}
					}
				}
				else
				{
					(*i)->PutBytes( CGPSProtocol::SEND_GPSS_REJECT( REJECTGPS_INVALID ) );
					(*i)->DoSend( &send_fd );
					delete *i;
					i = m_ReconnectSockets.erase( i );
					continue;
				}
			}
			else
			{
				(*i)->PutBytes( CGPSProtocol::SEND_GPSS_REJECT( REJECTGPS_INVALID ) );
				(*i)->DoSend( &send_fd );
				delete *i;
				i = m_ReconnectSockets.erase( i );
				continue;
			}
		}

		(*i)->DoSend( &send_fd );
		i++;
	}


	// update battle.net connections

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
	{
		if( (*i)->Update( &fd, &send_fd ) )
			BNETExit = true;
	}

	// autohost

	if( !m_Config->m_AutoHostGameName.empty( ) && m_Config->m_AutoHostMaximumGames && m_Config->m_AutoHostAutoStartPlayers && GetTime( ) - m_LastAutoHostTime >= 30 )
	{
		// copy all the checks from CGHost :: CreateGame here because we don't want to spam the chat when there's an error
		// instead we fail silently and try again soon

		if( !m_ExitingNice && m_Enabled && !m_CurrentGame && m_Games.size( ) < m_Config->m_MaxGames && m_Games.size( ) < m_Config->m_AutoHostMaximumGames )
		{
			if( m_AutoHostMap->GetValid( ) )
			{
				string GameName = m_Config->m_AutoHostGameName + " #" + UTIL_ToString( m_HostCounter );

				if( GameName.size( ) <= 31 )
				{
					m_AutoHosted = true;
					CreateGame( m_AutoHostMap, GAME_PUBLIC, false, GameName, m_Config->m_AutoHostOwner, m_Config->m_AutoHostOwner, m_AutoHostServer, false );
					m_AutoHosted = false;

					if( m_CurrentGame )
					{
						m_CurrentGame->SetAutoStartPlayers( m_Config->m_AutoHostAutoStartPlayers );

						if( m_AutoHostMatchMaking )
						{
							if( !m_Map->GetMapMatchMakingCategory( ).empty( ) )
							{
								if( !( m_Map->GetMapOptions( ) & MAPOPT_FIXEDPLAYERSETTINGS ) )
									CONSOLE_Print( "[GHOST] autohostmm - map_matchmakingcategory [" + m_Map->GetMapMatchMakingCategory( ) + "] found but matchmaking can only be used with fixed player settings, matchmaking disabled" );
								else
								{
									CONSOLE_Print( "[GHOST] autohostmm - map_matchmakingcategory [" + m_Map->GetMapMatchMakingCategory( ) + "] found, matchmaking enabled" );

									m_CurrentGame->SetMatchMaking( true );
									m_CurrentGame->SetMinimumScore( m_AutoHostMinimumScore );
									m_CurrentGame->SetMaximumScore( m_AutoHostMaximumScore );
								}
							}
							else
								CONSOLE_Print( "[GHOST] autohostmm - map_matchmakingcategory not found, matchmaking disabled" );
						}
					}
				}
				else
				{
					CONSOLE_Print( "[GHOST] stopped auto hosting, next game name [" + GameName + "] is too long (the maximum is 31 characters)" );
					m_Config->m_AutoHostGameName.clear( );
					m_Config->m_AutoHostOwner.clear( );
					m_AutoHostServer.clear( );
					m_Config->m_AutoHostMaximumGames = 0;
					m_Config->m_AutoHostAutoStartPlayers = 0;
					m_AutoHostMatchMaking = false;
					m_AutoHostMinimumScore = 0.0;
					m_AutoHostMaximumScore = 0.0;
				}
			}
			else
			{
				CONSOLE_Print( "[GHOST] stopped auto hosting, map config file [" + m_AutoHostMap->GetCFGFile( ) + "] is invalid" );
				m_Config->m_AutoHostGameName.clear( );
				m_Config->m_AutoHostOwner.clear( );
				m_AutoHostServer.clear( );
				m_Config->m_AutoHostMaximumGames = 0;
				m_Config->m_AutoHostAutoStartPlayers = 0;
				m_AutoHostMatchMaking = false;
				m_AutoHostMinimumScore = 0.0;
				m_AutoHostMaximumScore = 0.0;
			}
		}

		m_LastAutoHostTime = GetTime( );
	}

	// process packets from brtServer

	ProcessCommandPackets();

	return m_Exiting || AdminExit || BNETExit;
}

void CGHost :: EventBNETConnecting( CBNET *bnet )
{
	if( m_AdminGame)
		m_AdminGame->SendAllChat( m_Language->GetLang( "lang_0105", bnet->GetServer( ) ) ); // ConnectingToBNET

	if( m_CurrentGame ) 
		m_CurrentGame->SendAllChat( m_Language->GetLang( "lang_0105", bnet->GetServer( ) ) ); // ConnectingToBNET
	
}

void CGHost :: EventBNETConnected( CBNET *bnet )
{
	if( m_AdminGame)
		m_AdminGame->SendAllChat( m_Language->GetLang( "lang_0105", bnet->GetServer( ) ) ); // ConnectedToBNET

	if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->GetLang( "lang_0105", bnet->GetServer( ) ) ); // ConnectedToBNET
	
}

void CGHost :: EventBNETDisconnected( CBNET *bnet )
{
	if( m_AdminGame)
		m_AdminGame->SendAllChat( m_Language->GetLang("lang_0107", bnet->GetServer( ) ) ); // DisconnectedFromBNET


	if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->GetLang("lang_0107", bnet->GetServer( ) ) ); // DisconnectedFromBNET
		
}

void CGHost :: EventBNETLoggedIn( CBNET *bnet )
{
	if( m_AdminGame)
		m_AdminGame->SendAllChat( m_Language->GetLang("lang_0108", bnet->GetServer( ) ) ); // LoggedInToBNET

	
	if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->GetLang("lang_0108", bnet->GetServer( ) ) ); // LoggedInToBNET
		
}

void CGHost :: EventBNETGameRefreshed( CBNET *bnet )
{
	if(m_CurrentGame)
	{
		m_LastGameName = m_CurrentGame->GetGameName();
		if(m_CurrentGame->m_Rehost)
		{
			m_Rehosted = true;
			m_RehostedName = m_CurrentGame->GetGameName();
			m_RehostedServer = m_CurrentGame->GetCreatorServer();
			m_CurrentGame->m_Rehost = false;

			CONSOLE_Print( "[GAME: " + m_CurrentGame->GetGameName() + "] rehost worked");
			m_CurrentGame->m_LastPlayerJoinedTime = GetTime( );

			m_CurrentGame->SendAllChat( m_Language->GetLang("lang_1503", m_CurrentGame->GetGameName() ));
		} else
		{
			m_Hosted = true;
			m_HostedName = m_CurrentGame->GetGameName();
		}
	}
// commented as it spams too much for now
	//	if( m_AdminGame )
		//m_AdminGame->SendAllChat( m_Language->BNETGameHostingSucceeded( bnet->GetServer( ) ) );
}

void CGHost :: EventBNETGameRefreshFailed( CBNET *bnet )
{
	if( m_CurrentGame )
	{
		m_LastGameName = m_CurrentGame->GetGameName();
		if (!m_QuietRehost)
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			(*i)->QueueChatCommand( m_Language->GetLang("lang_0001", "$SERVER$", bnet->GetServer( ), "$GAMENAME$", m_CurrentGame->GetGameName( ) ) ); // UnableToCreateGameTryAnotherName

			if( (*i)->GetServer( ) == m_CurrentGame->GetCreatorServer( ) )
				(*i)->QueueChatCommand( m_Language->GetLang("lang_0001", "$SERVER$", bnet->GetServer( ), "$GAMENAME$", m_CurrentGame->GetGameName( ) ), m_CurrentGame->GetCreatorName( ), true ); // UnableToCreateGameTryAnotherName
		}

		if( m_AdminGame )
			m_AdminGame->SendAllChat( m_Language->GetLang("lang_0110", "$SERVER$", bnet->GetServer( ), "$GAMENAME$", m_CurrentGame->GetGameName( ) ) ); // BNETGameHostingFailed

		if (!m_Config->m_RehostIfNameTaken)
		{
			m_CurrentGame->SendAllChat( m_Language->GetLang("lang_0001", "$SERVER$", bnet->GetServer( ), "$GAMENAME$", m_CurrentGame->GetGameName( ) ) ); // UnableToCreateGameTryAnotherName

			// we take the easy route and simply close the lobby if a refresh fails
			// it's possible at least one refresh succeeded and therefore the game is still joinable on at least one battle.net (plus on the local network) but we don't keep track of that
			// we only close the game if it has no players since we support game rehosting (via !priv and !pub in the lobby)

			if( m_CurrentGame->GetNumHumanPlayers( ) == 0 )
				m_CurrentGame->SetExiting( true );
		}

		// if game name is taken, rehost with game name + 1
		if (m_Config->m_RehostIfNameTaken)
		{
			m_CurrentGame->SetGameName(IncGameNr(m_CurrentGame->GetGameName()));
			m_CurrentGame->AddGameName(m_CurrentGame->GetGameName());
			m_HostCounter++;
			SaveHostCounter();

			if (m_Config->m_MaxHostCounter > 0 && m_HostCounter > m_Config->m_MaxHostCounter)
				m_HostCounter = 1;

			m_CurrentGame->SetHostCounter(m_HostCounter);
			m_QuietRehost = true;
			m_CurrentGame->SetRefreshError(false);
			m_CurrentGame->SetRehost(true);
			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
			{
				(*i)->QueueGameUncreate( );
				(*i)->QueueEnterChat( );

				// the game creation message will be sent on the next refresh
			}
			m_CurrentGame->SetCreationTime(GetTime( ));
			m_CurrentGame->SetLastRefreshTime(GetTime( ));
			m_CurrentGame->m_LastPlayerJoinedTime = GetTime( )+5;
			return;
		}

		m_CurrentGame->SetRefreshError( true );
	}
}

void CGHost :: EventBNETConnectTimedOut( CBNET *bnet )
{
	if( m_AdminGame)
		m_AdminGame->SendAllChat( m_Language->GetLang("lang_0111", bnet->GetServer( ) ) ); // ConnectingToBNETTimedOut

	if( m_CurrentGame )
		m_CurrentGame->SendAllChat( m_Language->GetLang("lang_0111", bnet->GetServer( ) ) ); // ConnectingToBNETTimedOut
}

void CGHost :: EventBNETWhisper( CBNET *bnet, string user, string message )
{
	if( m_AdminGame )
	{
		m_AdminGame->SendAdminChat( "[W: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );

		if( m_CurrentGame )
			m_CurrentGame->SendLocalAdminChat( "[W: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );

		for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); i++ )
			(*i)->SendLocalAdminChat( "[W: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );
	}
}

void CGHost :: EventBNETChat( CBNET *bnet, string user, string message )
{
	if( m_AdminGame )
	{
		m_AdminGame->SendAdminChat( "[L: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );

		if( m_CurrentGame )
			m_CurrentGame->SendLocalAdminChat( "[L: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );

		for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); i++ )
			(*i)->SendLocalAdminChat( "[L: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );
	}
}

void CGHost :: EventBNETEmote( CBNET *bnet, string user, string message )
{
	if( m_AdminGame )
	{
		m_AdminGame->SendAdminChat( "[E: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );

		if( m_CurrentGame )
			m_CurrentGame->SendLocalAdminChat( "[E: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );

		for( vector<CBaseGame *> :: iterator i = m_Games.begin( ); i != m_Games.end( ); i++ )
			(*i)->SendLocalAdminChat( "[E: " + bnet->GetServerAlias( ) + "] [" + user + "] " + message );
	}
}

void CGHost :: EventGameDeleted( CBaseGame *game )
{
	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
	{
		(*i)->QueueChatCommand( m_Language->GetLang("lang_0040", game->GetDescription( ) ) ); // GameIsOver

		if( (*i)->GetServer( ) == game->GetCreatorServer( ) )
			(*i)->QueueChatCommand( m_Language->GetLang("lang_0040", game->GetDescription( ) ), game->GetCreatorName( ), true ); // GameIsOver
	}
}

void CGHost :: ExtractScripts( )
{
	string PatchMPQFileName = m_Config->war3path + "War3Patch.mpq";
	HANDLE PatchMPQ;

	if( SFileOpenArchive( PatchMPQFileName.c_str( ), 0, MPQ_OPEN_FORCE_MPQ_V1, &PatchMPQ ) )
	{
		CONSOLE_Print( "[GHOST] loading MPQ file [" + PatchMPQFileName + "]" );
		HANDLE SubFile;

		// common.j

		if( SFileOpenFileEx( PatchMPQ, "Scripts\\common.j", 0, &SubFile ) )
		{
			uint32_t FileLength = SFileGetFileSize( SubFile, NULL );

			if( FileLength > 0 && FileLength != 0xFFFFFFFF )
			{
				char *SubFileData = new char[FileLength];
				DWORD BytesRead = 0;

				if( SFileReadFile( SubFile, SubFileData, FileLength, &BytesRead ) )
				{
					CONSOLE_Print( "[GHOST] extracting Scripts\\common.j from MPQ file to [" + m_Config->mapcfgpath + "common.j]" );
					UTIL_FileWrite( m_Config->mapcfgpath + "common.j", (unsigned char *)SubFileData, BytesRead );
				}
				else
					CONSOLE_Print( "[GHOST] warning - unable to extract Scripts\\common.j from MPQ file" );

				delete [] SubFileData;
			}

			SFileCloseFile( SubFile );
		}
		else
			CONSOLE_Print( "[GHOST] couldn't find Scripts\\common.j in MPQ file" );

		// blizzard.j

		if( SFileOpenFileEx( PatchMPQ, "Scripts\\blizzard.j", 0, &SubFile ) )
		{
			uint32_t FileLength = SFileGetFileSize( SubFile, NULL );

			if( FileLength > 0 && FileLength != 0xFFFFFFFF )
			{
				char *SubFileData = new char[FileLength];
				DWORD BytesRead = 0;

				if( SFileReadFile( SubFile, SubFileData, FileLength, &BytesRead ) )
				{
					CONSOLE_Print( "[GHOST] extracting Scripts\\blizzard.j from MPQ file to [" + m_Config->mapcfgpath + "blizzard.j]" );
					UTIL_FileWrite( m_Config->mapcfgpath + "blizzard.j", (unsigned char *)SubFileData, BytesRead );
				}
				else
					CONSOLE_Print( "[GHOST] warning - unable to extract Scripts\\blizzard.j from MPQ file" );

				delete [] SubFileData;
			}

			SFileCloseFile( SubFile );
		}
		else
			CONSOLE_Print( "[GHOST] couldn't find Scripts\\blizzard.j in MPQ file" );

		SFileCloseArchive( PatchMPQ );
	}
	else
		CONSOLE_Print( "[GHOST] warning - unable to load MPQ file [" + PatchMPQFileName + "] - error code " + UTIL_ToString( GetLastError( ) ) );
}

void CGHost :: LoadIPToCountryDataOpt( )
{
	bool oldips = true;

	intmax_t file_len;
	file_len = file_size("ip-to-country.csv");

	if (exists("ips.cfg") && exists("ips.dbs"))
	{
		string File = "ips.cfg";
		CConfig CFGH;
		CFGH.Read( File );
		int filelen = CFGH.GetInt( "size", 0 );
		if (filelen==file_len)
			oldips = false;
	}

	if (oldips)
	{
		LoadIPToCountryData( );
		m_DBLocal->RunQuery("ATTACH DATABASE 'ips.dbs' AS ips");
		m_DBLocal->RunQuery("DELETE FROM ips.iptocountry");
		m_DBLocal->RunQuery("INSERT INTO ips.iptocountry SELECT * FROM iptocountry");
		m_DBLocal->RunQuery("DETACH DATABASE ips");
		if (file_len!=0)
		{
			string File = "ips.cfg";
			ofstream tmpcfg;
			tmpcfg.open( File.c_str( ), ios :: trunc );
			int size = (int)file_len;
			tmpcfg << "size = " << UTIL_ToString(size) << endl;
			tmpcfg.close( );
		}
	}
	else
	{
		CONSOLE_Print( "[GHOST] started loading [ips.dbs]" );
		uint32_t tim = GetTicks();
		m_DBLocal->RunQuery("ATTACH DATABASE 'ips.dbs' AS ips");
		m_DBLocal->RunQuery("INSERT INTO iptocountry SELECT * FROM ips.iptocountry");
		m_DBLocal->RunQuery("DETACH DATABASE ips");
		CONSOLE_Print("[GHOST] iptocountry loading finished in "+UTIL_ToString(GetTicks()-tim)+" ms");
	}
}

void CGHost :: LoadIPToCountryData( )
{
	ifstream in;
	in.open( "ip-to-country.csv" );

	if( in.fail( ) )
		CONSOLE_Print( "[GHOST] warning - unable to read file [ip-to-country.csv], iptocountry data not loaded" );
	else
	{
		CONSOLE_Print( "[GHOST] started loading [ip-to-country.csv]" );

		// the begin and commit statements are optimizations
		// we're about to insert ~4 MB of data into the database so if we allow the database to treat each insert as a transaction it will take a LONG time
		// todotodo: handle begin/commit failures a bit more gracefully

		if( !m_DBLocal->Begin( ) )
			CONSOLE_Print( "[GHOST] warning - failed to begin local database transaction, iptocountry data not loaded" );
		else
		{
			unsigned char Percent = 0;
			string Line;
			string IP1;
			string IP2;
			string Country;
			CSVParser parser;

			// get length of file for the progress meter

			in.seekg( 0, ios :: end );
			uint32_t FileLength = in.tellg( );
			in.seekg( 0, ios :: beg );

			uint32_t tim = GetTicks();

			while( !in.eof( ) )
			{
				getline( in, Line );

				if( Line.empty( ) )
					continue;

				parser << Line;
				parser >> IP1;
				parser >> IP2;
				parser >> Country;
				m_DBLocal->FromAdd( UTIL_ToUInt32( IP1 ), UTIL_ToUInt32( IP2 ), Country );

				// it's probably going to take awhile to load the iptocountry data (~10 seconds on my 3.2 GHz P4 when using SQLite3)
				// so let's print a progress meter just to keep the user from getting worried

				unsigned char NewPercent = (unsigned char)( (float)in.tellg( ) / FileLength * 100 );

				if( NewPercent != Percent && NewPercent % 10 == 0 )
				{
					Percent = NewPercent;
					CONSOLE_Print( "[GHOST] iptocountry data: " + UTIL_ToString( Percent ) + "% loaded" );
				}
			}

			CONSOLE_Print("[GHOST] iptocountry loading finished in "+UTIL_ToString(GetTicks()-tim)+" ms");

			if( !m_DBLocal->Commit( ) )
				CONSOLE_Print( "[GHOST] warning - failed to commit local database transaction, iptocountry data not loaded" );
			else
				CONSOLE_Print( "[GHOST] finished loading [ip-to-country.csv]" );
		}

		in.close( );
	}
}

void CGHost :: CreateGame( CMap *map, unsigned char gameState, bool saveGame, string gameName, string ownerName, string creatorName, string creatorServer, bool whisper )
{
	if( !m_Enabled )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->GetLang("lang_0125", "$GAMENAME$", gameName.substr(0,10)+"...", "$REASON$", m_DisableReason ), creatorName, whisper ); // UnableToCreateGameDisabled
		}

		if( m_AdminGame )
			m_AdminGame->SendAllChat( m_Language->GetLang("lang_0125", "$GAMENAME$", gameName.substr(0,10)+"...", "$REASON$", m_DisableReason ) ); // UnableToCreateGameDisabled

		return;
	}

	if( gameName.size( ) > 31 )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->GetLang("lang_0113", gameName ), creatorName, whisper ); // UnableToCreateGameNameTooLong
		}

		if( m_AdminGame )
			m_AdminGame->SendAllChat( m_Language->GetLang("lang_0113", gameName ) ); // UnableToCreateGameNameTooLong

		return;
	}

	if( !map->GetValid( ) )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->GetLang("lang_0128", gameName ), creatorName, whisper ); // UnableToCreateGameInvalidMap
		}

		if( m_AdminGame )
			m_AdminGame->SendAllChat( m_Language->GetLang("lang_0128", gameName ) ); // UnableToCreateGameInvalidMap

		return;
	}

	if( saveGame )
	{
		if( !m_SaveGame->GetValid( ) )
		{
			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
			{
				if( (*i)->GetServer( ) == creatorServer )
					(*i)->QueueChatCommand( m_Language->GetLang("lang_0140", gameName ), creatorName, whisper ); // UnableToCreateGameInvalidSaveGame
			}

			if( m_AdminGame )
				m_AdminGame->SendAllChat( m_Language->GetLang("lang_0140", gameName ) ); // UnableToCreateGameInvalidSaveGame

			return;
		}

		string MapPath1 = m_SaveGame->GetMapPath( );
		string MapPath2 = map->GetMapPath( );
		transform( MapPath1.begin( ), MapPath1.end( ), MapPath1.begin( ), (int(*)(int))tolower );
		transform( MapPath2.begin( ), MapPath2.end( ), MapPath2.begin( ), (int(*)(int))tolower );

		if( MapPath1 != MapPath2 )
		{
			CONSOLE_Print( "[GHOST] path mismatch, saved game path is [" + MapPath1 + "] but map path is [" + MapPath2 + "]" );

			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
			{
				if( (*i)->GetServer( ) == creatorServer )
					(*i)->QueueChatCommand( m_Language->GetLang("lang_0141", gameName ), creatorName, whisper ); // UnableToCreateGameSaveGameMapMismatch
			}

			if( m_AdminGame )
				m_AdminGame->SendAllChat( m_Language->GetLang("lang_0141", gameName ) ); // UnableToCreateGameSaveGameMapMismatch

			return;
		}

		if( m_EnforcePlayers.empty( ) )
		{
			for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
			{
				if( (*i)->GetServer( ) == creatorServer )
					(*i)->QueueChatCommand( m_Language->GetLang("lang_0207", gameName ), creatorName, whisper ); // UnableToCreateGameMustEnforceFirst
			}

			if( m_AdminGame )
				m_AdminGame->SendAllChat( m_Language->GetLang("lang_0207", gameName ) ); // UnableToCreateGameMustEnforceFirst

			return;
		}
	}

	if( m_CurrentGame )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->GetLang("lang_0038", "$GAMENAME$", gameName, "$DESCRIPTION$", m_CurrentGame->GetDescription( ) ), creatorName, whisper ); // UnableToCreateGameAnotherGameInLobby
		}

		if( m_AdminGame )
			m_AdminGame->SendAllChat( m_Language->GetLang("lang_0038", "$GAMENAME$", gameName, "$DESCRIPTION$", m_CurrentGame->GetDescription( ) ) ); // UnableToCreateGameAnotherGameInLobby

		return;
	}

	if( m_Games.size( ) >= m_Config->m_MaxGames )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			if( (*i)->GetServer( ) == creatorServer )
				(*i)->QueueChatCommand( m_Language->GetLang("lang_0039", "$GAMENAME$", gameName, "$MAX$", UTIL_ToString( m_Config->m_MaxGames ) ), creatorName, whisper ); // UnableToCreateGameMaxGamesReached
		}

		if( m_AdminGame )
			m_AdminGame->SendAllChat( m_Language->GetLang("lang_0039", "$GAMENAME$", gameName, "$MAX$", UTIL_ToString( m_Config->m_MaxGames ) ) ); // UnableToCreateGameMaxGamesReached

		return;
	}

	CONSOLE_Print( "[GHOST] creating game [" + gameName + "]" );

	m_LastGameName = gameName;

//	if (m_AutoHosted)
//	if( saveGame )
//		m_CurrentGame = new CGame( this, m_AutoHostMap, m_SaveGame, m_HostPort, gameState, gameName, ownerName, creatorName, creatorServer );
//	else
//		m_CurrentGame = new CGame( this, m_AutoHostMap, NULL, m_HostPort, gameState, gameName, ownerName, creatorName, creatorServer );
//	else
	unsigned char gs = gameState;
	if (gameState!=GAME_PRIVATE && gameState!=GAME_PUBLIC)
		gameState = GAME_PRIVATE;

	if( saveGame )
		m_CurrentGame = new CGame( this, m_Config, map, m_SaveGame, m_Config->port_host, gameState, gameName, ownerName, creatorName, creatorServer );
	else
		m_CurrentGame = new CGame( this, m_Config, map, NULL, m_Config->port_host, gameState, gameName, ownerName, creatorName, creatorServer );

	// todotodo: check if listening failed and report the error to the user

	if( m_SaveGame )
	{
		m_CurrentGame->SetEnforcePlayers( m_EnforcePlayers );
		m_EnforcePlayers.clear( );
	}

	m_CurrentGame->m_GameStateS = gs;

	m_CurrentGame->SetAutoHosted(m_AutoHosted);

	// auto set HCL if map_defaulthcl is not empty
	m_CurrentGame->AutoSetHCL();

	// country checks
	if (!m_AutoHosted)
	if (!m_AllowedCountries.empty())
	{
		m_CurrentGame->m_Countries = m_AllowedCountries;
		m_CurrentGame->m_CountryCheck = true;
	}

	// score checks
	if (!m_AutoHosted && m_Config->m_AllowedScores)
	{
		m_CurrentGame->m_Scores = m_Config->m_AllowedScores;
		m_CurrentGame->m_ScoreCheck = true;
	}

	// score checks in autohosted
	if (m_AutoHosted && m_Config->m_AutoHostAllowedScores)
	{
		m_CurrentGame->m_Scores = m_Config->m_AllowedScores;
		m_CurrentGame->m_ScoreCheck = true;
	}

	if (!m_AutoHosted)
	if (!m_DeniedCountries.empty())
	{
		m_CurrentGame->m_Countries2 = m_DeniedCountries;
		m_CurrentGame->m_CountryCheck2 = true;
	}

	// add players from RMK if any

	if (m_Config->m_HoldPlayersForRMK && m_PlayersfromRMK.length()>0)
	{
		CONSOLE_Print( "[GHOST] reserving players from last game");

		stringstream SS;
		string s;
		SS << m_PlayersfromRMK;
		while( !SS.eof( ) )
		{
			SS >> s;
			m_CurrentGame->AddToReserved(s, 255);
		}
		m_PlayersfromRMK = string();
	}

	bool AutoHostRefresh = m_Config->m_AutoHostLocal && m_AutoHosted;
	// don't advertise the game if it's autohosted locally
	if (!AutoHostRefresh )
	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
	{
		if( whisper && (*i)->GetServer( ) == creatorServer )
		{
			// note that we send this whisper only on the creator server

			if( gameState == GAME_PRIVATE )
				(*i)->QueueChatCommand( m_Language->GetLang("lang_0031", "$GAMENAME$", gameName, "$USER$", ownerName ), creatorName, whisper ); // CreatingPrivateGame
			else if( gameState == GAME_PUBLIC )
				(*i)->QueueChatCommand( m_Language->GetLang("lang_0032", "$GAMENAME$", gameName, "$USER$", ownerName ), creatorName, whisper ); // CreatingPublicGame
		}
		else
		{
			// note that we send this chat message on all other bnet servers

			if( gameState == GAME_PRIVATE )
				(*i)->QueueChatCommand( m_Language->GetLang("lang_0031", "$GAMENAME$", gameName, "$USER$", ownerName ) ); // CreatingPrivateGame
			else if( gameState == GAME_PUBLIC )
				(*i)->QueueChatCommand( m_Language->GetLang("lang_0032", "$GAMENAME$", gameName, "$USER$", ownerName ) ); // CreatingPublicGame
		}

		if( saveGame )
			(*i)->QueueGameCreate( gameState, gameName, string( ), map, m_SaveGame, m_CurrentGame->GetHostCounter( ) );
		else
			(*i)->QueueGameCreate( gameState, gameName, string( ), map, NULL, m_CurrentGame->GetHostCounter( ) );
	}

	if( m_AdminGame )
	{
		if( gameState == GAME_PRIVATE )
			m_AdminGame->SendAllChat( m_Language->GetLang("lang_0031", "$GAMENAME$", gameName, "$USER$", ownerName ) ); // CreatingPrivateGame
		else if( gameState == GAME_PUBLIC )
			m_AdminGame->SendAllChat( m_Language->GetLang("lang_0032", "$GAMENAME$", gameName, "$USER$", ownerName ) ); // CreatingPublicGame
	}

	// if we're creating a private game we don't need to send any game refresh messages so we can rejoin the chat immediately
	// unfortunately this doesn't work on PVPGN servers because they consider an enterchat message to be a gameuncreate message when in a game
	// so don't rejoin the chat if we're using PVPGN

	if( gameState == GAME_PRIVATE )
	{
		for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
		{
			if( (*i)->GetPasswordHashType( ) != "pvpgn" )
				(*i)->QueueEnterChat( );
		}
	}

	// hold friends and/or clan members

	for( vector<CBNET *> :: iterator i = m_BNETs.begin( ); i != m_BNETs.end( ); i++ )
	{
		if( (*i)->GetHoldFriends( ) )
			(*i)->HoldFriends( m_CurrentGame );

		if( (*i)->GetHoldClan( ) )
			(*i)->HoldClan( m_CurrentGame );
	}

	// WaaaghTV

#ifdef WIN32
	if (m_Config->m_wtv && m_CurrentGame->wtvprocessid == 0 && m_Map->GetMapObservers()>=3)
	{
		m_CurrentGame->CloseSlot( m_CurrentGame->m_Slots.size()-2, true );
		m_CurrentGame->CloseSlot( m_CurrentGame->m_Slots.size()-1, true );
		m_CurrentGame->CreateWTVPlayer( m_Config->m_wtvPlayerName, true );

		STARTUPINFO si;
		PROCESS_INFORMATION pi;

		ZeroMemory( &si, sizeof(si) );
		si.cb = sizeof(si);
		ZeroMemory( &pi, sizeof(pi) );

		string wtvRecorderEXE = m_Config->m_wtvPath + "wtvRecorder.exe";

		//hProcess = CreateProcess( tr("D:\\Programme\\wc3tv\\wtvRecorder.exe"), NULL, NULL, NULL, TRUE, HIGH_PRIORITY_CLASS, NULL, tr("D:\\Programme\\wc3tv\\" ), &si, &pi );
		//HANDLE hProcess = CreateProcess( wtvRecorderEXE, NULL, NULL, NULL, TRUE, HIGH_PRIORITY_CLASS, NULL, m_wtvPath, &si, &pi );
		int hProcess = CreateProcessA( wtvRecorderEXE.c_str( ), NULL, NULL, NULL, TRUE, HIGH_PRIORITY_CLASS, NULL, m_Config->m_wtvPath.c_str( ), LPSTARTUPINFOA(&si), &pi );

		if( !hProcess )
			CONSOLE_Print( "[WaaaghTV] : Failed to start wtvRecorder.exe" );
		else
		{
			m_CurrentGame->wtvprocessid = int( pi.dwProcessId );
			CONSOLE_Print( "[WaaaghTV] : wtvRecorder.exe started!" );
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
	}
#endif
}

void CGHost :: AdminGameMessage(string name, string message)
{
	if (m_AdminGame)
	{
		m_AdminGame->SendAllChat(name+": "+message);
	}
}

void CGHost :: CheckConfigs ()
{
	delete m_Language;
	m_Language = new CLanguage( m_Config->m_LanguageFile );

#ifndef WIN32
	m_Config->m_wtv = false;
#endif

	if (m_Config->m_wtv)
		CONSOLE_Print("[WTV] WaaaghTV is enabled.");
	else
		CONSOLE_Print("[WTV] WaaaghTV is not enabled.");


	if (m_AutoHostCountries2.length()>0)
		m_AutoHostCountryCheck2 = true;

	if (m_AutoHostCountries.length() > 0)
		m_AutoHostCountryCheck = true;

	if ((m_Config->m_AutoHostMapCFG.find("\\") == string::npos) && (m_Config->m_AutoHostMapCFG.find("/") == string::npos))
		m_Config->m_AutoHostMapCFG = m_Config->mapcfgpath + m_Config->m_AutoHostMapCFG;

	if( m_Config->m_VirtualHostName.size( ) > 15 )
	{
		m_Config->m_VirtualHostName = "|cFF4080C0GHost";
		CONSOLE_Print( "[GHOST] warning - bot_virtualhostname is longer than 15 characters, using default virtual host name" );
	}

	if( m_Config->m_VoteKickPercentage > 100 )
	{
		m_Config->m_VoteKickPercentage = 100;
		CONSOLE_Print( "[GHOST] warning - bot_votekickpercentage is greater than 100, using 100 instead" );
	}

	ParseCensoredWords();
	LoadHostCounter();

	m_DB->SetAdminAccess(m_Config->m_AdminAccess);

	UTIL_ExtractStrings( m_Config->m_channeljoinexceptions, m_channeljoinex );
	
	if (m_Config->m_ReplayTimeShift)
	{
		char Time[17];
		time_t Now = time( NULL );
		memset( Time, 0, sizeof( char ) * 17 );
		time_t times = Now + m_Config->m_ReplayTimeShift;
		strftime( Time, sizeof( char ) * 17, "%Y-%m-%d %H-%M", localtime( &times ) );
		CONSOLE_Print("[GHOST] shifted replay time: "+ (string)Time);
	}

	m_DB->SetFormula(m_Config->m_ScoreFormula);
	m_DB->SetMinGames( m_Config->m_ScoreMinGames );

	if( m_Config->m_VoteKickPercentage > 100 )
	{
		m_Config->m_VoteKickPercentage = 100;
		CONSOLE_Print( "[GHOST] warning - bot_votekickpercentage is greater than 100, using 100 instead" );
	}

	EndTimer();

	if (m_Config->m_newTimer)
	{
		SetTimerResolution();
	}

	transform( m_Config->m_RootAdmins.begin( ), m_Config->m_RootAdmins.end( ), m_Config->m_RootAdmins.begin( ), (int(*)(int))tolower );

	m_PlayersfromRMK = string();

	if ( m_Config->m_VirtualHostName.length() > 15 )
		m_Config->m_VirtualHostName = m_Config->m_VirtualHostName.substr(0,15);

	ReadProviders();
	size_t f;
	providersn.clear();
	providers.clear();
	for(uint32_t i=0; i<m_Providers.size(); i++)
	{
		f = m_Providers[i].find("=");
		if ( f !=string :: npos )
		{
			providersn.push_back(m_Providers[i].substr(0,f));
			providers.push_back(m_Providers[i].substr(f+1,500));
		}
	}

	ReadWelcome();
	ReadChannelWelcome();
	ReadMars();
}

void CGHost :: ReadChannelWelcome ()
{
	string file = "channelwelcome.txt";
	ifstream in;
	in.open( file.c_str( ) );
	m_ChannelWelcome.clear();
	if( in.fail( ) )
		CONSOLE_Print( "[GHOST] warning - unable to read file [" + file + "]" );
	else
	{
		CONSOLE_Print( "[GHOST] loading file [" + file + "]" );
		string Line;

		while( !in.eof( ) )
		{
			getline( in, Line );

			// ignore blank lines and comments

			if( Line.empty( ) || Line[0] == '#' )
				continue;
			m_ChannelWelcome.push_back(Line);
		}
	}
	in.close( );
}

void CGHost :: ReadProviders ()
{
	string file = "providers.txt";
	ifstream in;
	in.open( file.c_str( ) );
	m_Providers.clear();
	if( in.fail( ) )
		CONSOLE_Print( "[GHOST] warning - unable to read file [" + file + "]" );
	else
	{
		CONSOLE_Print( "[GHOST] loading file [" + file + "]" );
		string Line;

		while( !in.eof( ) )
		{
			getline( in, Line );

			// ignore blank lines and comments

			if( Line.empty( ) || Line[0] == '#' )
				continue;
			m_Providers.push_back(Line);
		}
	}
	in.close( );
}

void CGHost :: ReadWelcome ()
{
	string file = "welcome.txt";
	ifstream in;
	in.open( file.c_str( ) );
	m_Welcome.clear();
	if( in.fail( ) )
		CONSOLE_Print( "[GHOST] warning - unable to read file [" + file + "]" );
	else
	{
		CONSOLE_Print( "[GHOST] loading file [" + file + "]" );
		string Line;

		while( !in.eof( ) )
		{
			getline( in, Line );

			// ignore blank lines and comments

			if( Line.empty( ) || Line[0] == '#' )
				continue;
			m_Welcome.push_back(Line);
		}
	}
	in.close( );
}

string CGHost :: GetMars ()
{
	if (m_Mars.size()==0)
		return string();
	bool ok = true;
	// delete the oldest message
	if (m_MarsLast.size()>=m_Mars.size() || m_MarsLast.size()>15)
	if (m_MarsLast.size()>0)
		m_MarsLast.erase(m_MarsLast.begin());
	do
	{
		ok = true;
		random_shuffle( m_Mars.begin( ), m_Mars.end( ) );
		for (uint32_t i = 0; i<m_MarsLast.size(); i++)
		{
			if (m_MarsLast[i]==m_Mars[0])
			{
				ok = false;
				break;
			}
		}
	} while (!ok);

	m_MarsLast.push_back(m_Mars[0]);
	return m_Mars[0];
}

void CGHost :: ReadMars ()
{
	string file = "mars.txt";
	ifstream in;
	in.open( file.c_str( ) );
	m_Mars.clear();
	if( in.fail( ) )
		CONSOLE_Print( "[GHOST] warning - unable to read file [" + file + "]" );
	else
	{
		CONSOLE_Print( "[GHOST] loading file [" + file + "]" );
		string Line;

		while( !in.eof( ) )
		{
			getline( in, Line );

			// ignore blank lines and comments

			if( Line.empty( ) || Line[0] == '#' )
				continue;
			m_Mars.push_back(Line);
		}
	}
	in.close( );
	srand((unsigned)time(0));
}

string CGHost :: Commands(unsigned int idx)
{
	string Result = string();
	if (idx<=m_Commands.size())
		Result = m_Commands[idx];
	return Result;
}

bool CGHost :: CommandAllowedToShow( string c)
{
	bool allowed = false;
	if (CMD_stringshow.find(c)!= string ::npos)
		allowed = true;
	return allowed;
}

void CGHost :: ParseCensoredWords( )
{
	m_CensoredWords.clear();

	transform( m_Config->m_CensorWords.begin( ), m_Config->m_CensorWords.end( ), m_Config->m_CensorWords.begin( ), (int(*)(int))tolower );
	stringstream SS;
	string s;
	SS << m_Config->m_CensorWords;

	while( !SS.eof( ) )
	{
		SS >> s;
		m_CensoredWords.push_back(s);
	}
}

string CGHost :: Censor ( string msg)
{
	uint32_t idx = 1;

/* uncomment if you want the first two letter to be seen (instead of only one letter)
	if (msg.length()>2)
		idx = 2;
*/
	string Result = msg.substr(0,idx);

	for (uint32_t i = idx; i<=msg.length()-idx; i++)
	{
		Result+="*";
	}
	return Result;
}

string CGHost :: CensorRemoveDots( string msg)
{
	string msgnew = msg.substr(0,2);
	if (msg.length()<=4)
		return msg;
	for (uint32_t i = 2; i != msg.length()-2; i++)
	{
		if (msg.substr(i,1)!=".")
			msgnew += msg.substr(i,1);/* else
		{
			if (msg.substr(i-1,1)!=" " && msg.substr(i+1,1)!=" ")
		}*/
	}
	msgnew += msg.substr(msg.length()-2,2);
	return msgnew;
}

string CGHost :: CensorMessage( string msg)
{
	if (msg.length()<1)
		return msg;
	string Msg = msg;
	for( vector<string> :: iterator i = m_CensoredWords.begin( ); i != m_CensoredWords.end( ); i++ )
	{
		boost :: ireplace_all( Msg, (*i), Censor((*i)) );
//		UTIL_Replace( Msg, (*i), Censor((*i)) );
	}
	return Msg;
}

bool CGHost :: ShouldFakePing(string name)
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	stringstream SS;
	string s;
	SS << m_Config->m_FakePings;

	while( !SS.eof( ) )
	{
		SS >> s;
		if (name == s)
		{
			return true;
		}
	}
	return false;
}

bool CGHost :: IsRootAdmin(string name)
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	stringstream SS;
	string s;
	SS << m_Config->m_RootAdmins;

	while( !SS.eof( ) )
	{
		SS >> s;
		if (name == s)
		{
			return true;
		}
	}
	return false;
}

void CGHost :: DelRootAdmin( string name)
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	stringstream SS;
	string tusers="";
	string luser;
	SS << m_Config->m_RootAdmins;
	while( !SS.eof( ) )
	{
		SS >> luser;
		if (luser != name)
		if (tusers.length()==0)
			tusers = luser;
		else
			tusers +=" "+luser;
	}
	m_Config->m_RootAdmins = tusers;
}


void CGHost :: AddRootAdmin(string name)
{
	transform( name.begin( ), name.end( ), name.begin( ), (int(*)(int))tolower );
	if (m_Config->m_RootAdmins.length()==0)
		m_Config->m_RootAdmins = name;
	else
	if (!IsRootAdmin(name))
		m_Config->m_RootAdmins += " " + name;
}

uint32_t CGHost :: CMDAccessAddOwner (uint32_t acc)
{
	return acc | m_Config->m_OwnerAccess;
}

uint32_t CGHost :: CMDAccessDel (uint32_t access, uint32_t cmd)
{
	uint32_t taccess;
	taccess = access;
	uint32_t Mask = 1;
	uint32_t scmd = cmd;
	uint32_t son = 0;
	uint32_t sacc = 0;
	if (scmd != 0)
		for (unsigned int k=1;k<=scmd;k++)
			Mask = Mask * 2;
	if (Mask > access)
		son = 0;
	else
		son = access & Mask;

	if (son==sacc || son==Mask)
	{
		if (sacc == 0)
		{
//			QueueChatCommand("Admin "+suser+ " already doesn't have access to "+m_GHost->m_Commands[scmd], User, Whisper);
		}
		else
		{
//			QueueChatCommand("Admin "+suser+ " already has access to "+m_GHost->m_Commands[scmd], User, Whisper);
		}
		return access;
	}
	if (sacc == 1)
		taccess+= Mask;
	else
		taccess -= Mask;

	return taccess;
}

uint32_t CGHost :: CMDAccessAdd (uint32_t access, uint32_t cmd)
{
	uint32_t taccess;
	taccess = access;
	uint32_t Mask = 1;
	uint32_t scmd = cmd;
	uint32_t son = 0;
	uint32_t sacc = 1;
	if (scmd != 0)
		for (unsigned int k=1;k<=scmd;k++)
			Mask = Mask * 2;
	if (Mask > access)
		son = 0;
	else
		son = access & Mask;

	if (son==sacc || son==Mask)
	{
		if (sacc == 0)
		{
			//			QueueChatCommand("Admin "+suser+ " already doesn't have access to "+m_GHost->m_Commands[scmd], User, Whisper);
		}
		else
		{
			//			QueueChatCommand("Admin "+suser+ " already has access to "+m_GHost->m_Commands[scmd], User, Whisper);
		}
		return access;
	}
	if (sacc == 1)
		taccess+= Mask;
	else
		taccess -= Mask;

	return taccess;
}

uint32_t CGHost :: CMDAccessAllOwner ()
{
	return m_Config->m_OwnerAccess;
}

void CGHost :: SaveHostCounter()
{
	string File = "hostcounter.cfg";
	ofstream tmpcfg;
	tmpcfg.open( File.c_str( ), ios :: trunc );
	tmpcfg << "hostcounter = " << UTIL_ToString(m_HostCounter) << endl;
	tmpcfg.close( );
}

void CGHost :: LoadHostCounter()
{
	string File = "hostcounter.cfg";
	CConfig CFGH;
	CFGH.Read( File );
	m_HostCounter = CFGH.GetInt( "hostcounter", 1 );
}

void CGHost :: AddSpoofedIP(string name, string ip)
{
	m_CachedSpoofedIPs.push_back( ip );
	m_CachedSpoofedNames.push_back( name );
}

bool CGHost :: IsChannelException(string name)
{
	for( uint32_t i = 0; i != m_channeljoinex.size(); i++ )
	{
		if( m_channeljoinex[i] == name )
		{
			return true;
		}
	}
	return false;
}

bool CGHost :: IsSpoofedIP(string name, string ip)
{
	for( uint32_t i = 0; i != m_CachedSpoofedIPs.size(); i++ )
	{
		if( m_CachedSpoofedIPs[i] == ip && m_CachedSpoofedNames[i]== name)
		{
			return true;
		}
	}
	return false;
}

string CGHost :: IncGameNr ( string name )
{
	string GameName = name;
	string GameNr = string();
	bool found = false;
	uint32_t idx = 0;
	uint32_t id;
	uint32_t Nr = 0;

	idx = GameName.length()-1;
	for (id = 7; id >=1; id-- )
	{
		if (idx>=id)
			if (GameName.at(idx-id)=='#')
			{
				idx = idx-id+1;
				found = true;
				break;
			}
	}
	if (!found)
		idx = 0;

	// idx = 0, no Game Nr found in gamename
	if (idx == 0)
	{
		GameNr = "0";
		GameName = name + " #";
	}
	else
	{
		GameNr = GameName.substr(idx,GameName.length()-idx);
		GameName = GameName.substr(0,idx);
	}
	stringstream SS;
	SS << GameNr;
	SS >> Nr;

	Nr++;
	if (m_Config->m_MaxHostCounter && Nr > m_Config->m_MaxHostCounter)
		Nr = 1;

	GameNr = UTIL_ToString(Nr);
	GameName = GameName + GameNr;
	return GameName;
}

void CGHost :: CalculateScoresCount()
{
	m_ScoresCount = m_DB->ScoresCount(string());
	m_ScoresCountSet = true;
}

uint32_t CGHost :: ScoresCount()
{
	if (!m_ScoresCountSet)
		CalculateScoresCount();
	return m_ScoresCount;
}

void CGHost :: SetTimerResolution()
{
#ifdef WIN32
	// initialize timer resolution
	// attempt to set the resolution as low as possible from 1ms to 5ms

	unsigned int ii = 1;
	if (m_Config->m_newTimerResolution>0)
		ii = m_Config->m_newTimerResolution;
	if (ii>5)
		ii=5;

	for( unsigned int i = ii; i <= 5; i++ )
	{
		if( timeBeginPeriod( i ) == TIMERR_NOERROR )
		{
			m_Config->m_newTimerResolution = i;
			break;
		}
		else if( i < 5 )
			CONSOLE_Print( "[GHOST] error setting Windows timer resolution to " + UTIL_ToString( i ) + " milliseconds, trying a higher resolution" );
		else
		{
			CONSOLE_Print( "[GHOST] error setting Windows timer resolution, going back to old timer function" );
			m_Config->m_newTimer = false;
			return;
		}
	}

	CONSOLE_Print( "[GHOST] using Windows timer with resolution " + UTIL_ToString( m_Config->m_newTimerResolution ) + " milliseconds" );
	m_newTimerStarted = true;
#endif
}

void CGHost :: EndTimer( )
{
	if (m_newTimerStarted)
	{
#ifdef WIN32
		timeEndPeriod( m_Config->m_newTimerResolution );
#endif
		m_newTimerStarted = false;
	}
}

void CMyCallableDownloadFile :: operator( )( )
{
	Init( );

	if( m_Error.empty( ) )
		m_Result = 0;

	// Create socket object
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if(sock == SOCKET_ERROR)	// bad socket?
	{
		CONSOLE_Print("[GHOST] failed making socket");
		Close();
		return;
	}

	sockaddr_in sa;
	sa.sin_family	= AF_INET;
	sa.sin_port		= htons(80);	// HTTP service uses port 80

	int port;
	char protocol[20],host[128],request[2048];

	/* Parse the URL */
	char * url;
	url = new char[m_File.length()+1];
	strcpy(url, m_File.c_str());
	ParseURL(url,protocol,sizeof(protocol)-1,host,sizeof(host)-1,request,sizeof(request)-1,&port);
	string file = request;
	size_t fpos = file.rfind("/");
	if (fpos == string :: npos)
	{
		CONSOLE_Print("[GHOST] url not ok");
		Close();
		return;
	}
	file = file.substr(fpos+1, file.size()-fpos-1);

	// Time to get the hostname

	hostent *h = gethostbyname(host);
	if(!h)
	{
		CONSOLE_Print("[GHOST] failed to get host");
		Close();
		return;
	}

	memcpy(&sa.sin_addr.s_addr, h->h_addr_list[0], 4);

	if( connect(sock, (sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR )
	{
		CONSOLE_Print("[GHOST] failed to connect");
		Close();
		return;
	}

	// This is our packet that we are going to send to the HTTP server.
	//

	string sshost = string(host);
	transform( sshost.begin( ), sshost.end( ), sshost.begin( ), (int(*)(int))tolower );

	string shost;
	shost = "GET "+string(request)+" HTTP/1.1\r\n"+"Host: "+sshost+"\r\n"+"Referrer: \r\n"+"\r\n";
/*
	PCHAR Packet;
	Packet = new char[shost.length()+2];
	strcpy(Packet, shost.c_str());
*/

	int rtn = 0;
	int iOptVal = 3;
	int iOptLen = sizeof(int);
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&iOptVal, iOptLen);

//	rtn = send(sock, Packet, sizeof(Packet)-1, 0);
	rtn = send(sock, shost.c_str(), shost.size(), 0);
	if(rtn <= 0)
	{
		CONSOLE_Print("[GHOST] failed to send packet");
		Close();
		return;
	}

	char Buffer[16384] = {0};

	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&iOptVal, iOptLen);

	rtn = recv(sock, Buffer, sizeof(Buffer), 0);
//	closesocket(sock);
	if(rtn <= 0)
	{
		CONSOLE_Print("[GHOST] failed to receive packet");
		Close();
		return;
	}

	// Did we get a valid reply back from the server?
	bool redirect = !( _strnicmp(Buffer, "HTTP/1.1 302 Fo", 15));
	bool ok = !( _strnicmp(Buffer, "HTTP/1.1 200 OK", 15));

	if( !redirect && !ok)
	{
		CONSOLE_Print("[GHOST] invalid response");
		Close();
		return;
	}

	if (redirect)
	{
		string sbuf2 = Buffer;

		size_t pos3 = sbuf2.find("Location: ");
		if (pos3 == string :: npos)
		{
			CONSOLE_Print("[GHOST] content not OK!");
			Close();
			return;
		}
		pos3 += 10;
		size_t pos4 = sbuf2.find("\x0D\x0A", pos3);

		string clocation = sbuf2.substr(pos3, pos4-pos3);
		closesocket(sock);

		url = new char[clocation.length()+1];
		strcpy(url, clocation.c_str());
		ParseURL(url,protocol,sizeof(protocol)-1,host,sizeof(host)-1,request,sizeof(request)-1,&port);
		file = request;
		fpos = file.rfind("/");
		if (fpos == string :: npos)
		{
			CONSOLE_Print("[GHOST] url not ok");
			Close();
			return;
		}
		file = file.substr(fpos+1, file.size()-fpos-1);
		// Time to get the hostname

		h = gethostbyname(host);
		if(!h)
		{
			CONSOLE_Print("[GHOST] failed to get host");
			Close();
			return;
		}

		memcpy(&sa.sin_addr.s_addr, h->h_addr_list[0], 4);
		// Create socket object
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		if(sock == SOCKET_ERROR)	// bad socket?
		{
			CONSOLE_Print("[GHOST] failed making socket");
			Close();
			return;
		}
		if( connect(sock, (sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR )
		{
			CONSOLE_Print("[GHOST] failed to connect");
			Close();
			return;
		}
		string sshost2 = string(clocation);
		transform( sshost2.begin( ), sshost2.end( ), sshost2.begin( ), (int(*)(int))tolower );

		string shost2;
		shost2 = "GET "+string(request)+" HTTP/1.1\r\n"+"Host: "+host+"\r\n"+"Referrer: \r\n"+"\r\n";
		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&iOptVal, iOptLen);

		//	rtn = send(sock, Packet, sizeof(Packet)-1, 0);
		rtn = send(sock, shost2.c_str(), shost2.size(), 0);
		if(rtn <= 0)
		{
			CONSOLE_Print("[GHOST] failed to send packet");
			Close();
			return;
		}
		rtn = recv(sock, Buffer, sizeof(Buffer), 0);
		//	closesocket(sock);
		if(rtn <= 0)
		{
			CONSOLE_Print("[GHOST] failed to receive packet");
			Close();
			return;
		}

		// Did we get a valid reply back from the server?
		ok = !( _strnicmp(Buffer, "HTTP/1.1 200 OK", 15));
		if( !ok)
		{
			CONSOLE_Print("[GHOST] invalid response");
			Close();
			return;
		}
	}

	string sbuf = Buffer;

	size_t pos1 = sbuf.find("Content-Length: ");
	if (pos1 == string :: npos)
	{
		CONSOLE_Print("[GHOST] content not OK!");
		Close();
		return;
	}
	pos1 += 16;
	size_t pos2 = sbuf.find("\x0D\x0A", pos1);

	string clength = sbuf.substr(pos1, pos2-pos1);

	size_t pos = sbuf.find("\x0D\x0A\x0D\x0A");
	if (pos == string :: npos)
	{
		CONSOLE_Print("[GHOST] content not OK!");
		Close();
		return;
	}
	else
		pos = pos+4;

	UTIL_Replace( file, "%20", " ");
	uint32_t totalsize=0;
	ofstream myfile;
	string path = m_Path;
	string pfile = path + file;
	myfile.open (pfile.c_str(), ios_base::binary);
	myfile.write(&Buffer[pos], rtn-pos);
	totalsize += rtn-pos;
	while (rtn>0)
	{
		rtn = recv(sock, Buffer, sizeof(Buffer), 0);
		if (rtn>0)
		{
			myfile.write(Buffer, rtn);
			totalsize+=rtn;
		}
	}

	if (totalsize!=UTIL_ToUInt32(clength))
	{
		CONSOLE_Print("[GHOST] Error downloading file: "+UTIL_ToString(totalsize)+" out of "+clength+" received!");
		Close();
		return;
	}
	m_Result = totalsize;
	closesocket(sock);
	myfile.close();

	Close( );
}

CMyCallableDownloadFile *CGHost :: ThreadedDownloadFile( string url, string path )
{
	CMyCallableDownloadFile *Callable = new CMyCallableDownloadFile( url, path);
	m_DB->CreateThread( Callable );
	return Callable;
}
