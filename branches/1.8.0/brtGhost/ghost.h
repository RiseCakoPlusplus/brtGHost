/*

   Copyright [2008] [Trevor Hogan]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

*/

#ifndef GHOST_H
#define GHOST_H

#include "includes.h"
#include "configdata.h"
#include "config.h"
#include "ghostdb.h"
#include "commandpacket.h"

//
// CGHost
//

class CUDPSocket;
class CTCPServer;
class CTCPSocket;
class CGPSProtocol;
class CPUBProtocol;
class CCRC32;
class CSHA1;
class CBNET;
class CBaseGame;
class CAdminGame;
class CGHostDB;
class CBaseCallable;
class CLanguage;
class CMap;
class CSaveGame;
//UDPCommandSocket patch
class CUDPServer;
class CConfig;
class CCommandPacket;


class CMyCallableDownloadFile : public CBaseCallable
{
protected:
	string m_File;
	string m_Path;
	uint32_t m_Result;
public:
	CMyCallableDownloadFile( string nFile, string nPath ) : CBaseCallable( ), m_File( nFile ), m_Path( nPath ), m_Result( 0 ) { }
	virtual ~CMyCallableDownloadFile( ) { }
	virtual void operator( )( );
	virtual string GetFile( )					{ return m_File; }
	virtual string GetPath( )					{ return m_Path; }
	virtual uint32_t GetResult( )				{ return m_Result; }
	virtual void SetResult( uint32_t nResult )	{ m_Result = nResult; }
};

class CbrtServer
{
private:
	CTCPServer *m_CommandSocketServer;			// command socket server
	CTCPSocket *m_CommandSocket;				// 
	bool nExiting;

	uint32_t m_Port;

	boost::thread* m_UpdateThread;

public:
	queue<CCommandPacket *> m_CommandPackets;  // queue incoming packets of command server
	queue<BYTEARRAY> m_PacketsToServer;		   // Packet who be sent to the main server

public:
	CbrtServer(uint32_t nPort);
	~CbrtServer();

	bool isExiting() { return nExiting; }

	void Update(int usecBlock);
	void UpdateThread();

	bool ExctactsCommandPackets();
};

class CGHost : boost::noncopyable
{
private:
	CConfigData* m_Config;

public:
	CUDPSocket *m_UDPSocket;				// a UDP socket for sending broadcasts and other junk (used with !sendlan)
	CTCPServer *m_ReconnectSocket;			// listening socket for GProxy++ reliable reconnects

	vector<CTCPSocket *> m_ReconnectSockets;// vector of sockets attempting to reconnect (connected but not identified yet)

	vector<CBNET *> m_BNETs;				// all our battle.net connections (there can be more than one)
	CBaseGame *m_CurrentGame;				// this game is still in the lobby state
	CAdminGame *m_AdminGame;				// this "fake game" allows an admin who knows the password to control the bot from the local network
	vector<CBaseGame *> m_Games;			// these games are in progress
	CGHostDB *m_DB;							// database
	CGHostDB *m_DBLocal;					// local database (for temporary data)
	vector<CBaseCallable *> m_Callables;	// vector of orphaned callables waiting to die
	vector<BYTEARRAY> m_LocalAddresses;		// vector of local IP addresses
	
	CMap *m_Map;							// the currently loaded map
	CMap *m_AdminMap;						// the map to use in the admin game
	CMap *m_AutoHostMap;					// the map to use when autohosting
	CSaveGame *m_SaveGame;					// the save game to use
	vector<PIDPlayer> m_EnforcePlayers;		// vector of pids to force players to use in the next game (used with saved games)
	
	bool m_Exiting;							// set to true to force ghost to shutdown next update (used by SignalCatcher)
	bool m_ExitingNice;						// set to true to force ghost to disconnect from all battle.net connections and wait for all games to finish before shutting down
	bool m_Enabled;							// set to false to prevent new games from being created
	string m_Version;						// GHost++ version string
	vector<string> providersn;
	vector<string> providers;
	uint32_t m_ScoresCount;
	bool m_ScoresCountSet;

	uint32_t m_HostCounter;					// the current host counter (a unique number to identify a game, incremented each time a game is created)

	CMyCallableDownloadFile *m_CallableDownloadFile;
	uint32_t m_LastAutoHostTime;			// GetTime when the last auto host was attempted
	bool m_AllGamesFinished;				// if all games finished (used when exiting nicely)
	uint32_t m_AllGamesFinishedTime;		// GetTime when all games finished (used when exiting nicely)

	vector<string> m_channeljoinex;

	vector<string> m_WarnForgetQueue;
	uint32_t m_MySQLQueueTicks;
	string m_AllowedCountries;
	string m_DeniedCountries;
	string m_OneVersion;
	string m_RootAdmin;

	vector<string> m_CensoredWords;

	string m_AutoHostServer;

	bool m_AutoHostMatchMaking;
	double m_AutoHostMinimumScore;
	double m_AutoHostMaximumScore;

	bool m_QuietRehost;
	bool m_CalculatingScores;
	string DBType;

	string m_LastGameName;

	uint32_t m_ExternalIPL;					// our external IP long format
	string m_Country;						// our country

	vector<string> m_CachedSpoofedIPs;
	vector<string> m_CachedSpoofedNames;
	vector<string> m_Providers;				//
	vector<string> m_Welcome;				// our welcome message
	vector<string> m_ChannelWelcome;		// our welcome message
	vector<string> m_Mars;					// our mars messages
	vector<string> m_MarsLast;				// our last mars messages

	string m_DisableReason;					// reason for disabling the bot

	bool m_Log;

	uint32_t m_NewRefreshTime;				// config value: send refresh every n seconds
	string m_AutoHostCountries;				// config value: which countries to allow
	string m_AutoHostCountries2;			// config value: which countries to deny
	bool m_AutoHosted;

	bool m_AutoHostCountryCheck;			// config value: country check enabled?
	bool m_AutoHostCountryCheck2;			// config value: country check2 enabled?
	bool m_AutoHostGArena;					// config value: only allow GArena

	uint32_t m_LastDynamicLatencyConsole;

	bool m_Rehosted;
	bool m_Hosted;

	bool m_newTimerStarted;
	string m_RehostedName;
	string m_HostedName;
	string m_RehostedServer;

	vector<string> m_Commands;

	string m_PlayersfromRMK;
	bool newGame;
	string newGameUser;
	string newGameServer;
	bool newGameProvidersCheck;
	bool newGameProviders2Check;
	string newGameProviders;
	string newGameProviders2;
	bool newGameCountryCheck;
	bool newGameCountry2Check;
	string newGameCountries;
	string newGameCountries2;
	unsigned char newGameGameState;
	string newGameName;
	bool newGameGArena;

	CGHost( CConfigData* nConfig );
	~CGHost( );

	string Commands(unsigned int idx);
	bool CommandAllowedToShow( string c);
	void ReadProviders();
	void ReadWelcome();
	void ReadChannelWelcome();
	void ReadMars();
	string GetMars();
	void SetTimerResolution();
	void EndTimer();
	void AdminGameMessage(string name, string message);

	bool ShouldFakePing(string name);
	bool IsRootAdmin(string name);
	void AddRootAdmin(string name);
	void DelRootAdmin(string name);

	uint32_t CMDAccessAddOwner (uint32_t acc);
	uint32_t CMDAccessAllOwner ();
	uint32_t CMDAccessAdd( uint32_t access, uint32_t acc);
	uint32_t CMDAccessDel( uint32_t access, uint32_t acc);

	void SaveHostCounter();
	void LoadHostCounter();
	void AddSpoofedIP (string name, string ip);
	bool IsSpoofedIP(string name, string ip);
	void ParseCensoredWords( );
	bool IsChannelException(string name);
	string Censor( string msg);
	string CensorMessage( string msg);
	string CensorRemoveDots( string msg);
	string IncGameNr( string name);
	uint32_t ScoresCount( );
	void CalculateScoresCount();

	// processing functions

	bool Update( unsigned long usecBlock );

	bool ProcessCommandPackets();

	// events

	void EventBNETConnecting( CBNET *bnet );
	void EventBNETConnected( CBNET *bnet );
	void EventBNETDisconnected( CBNET *bnet );
	void EventBNETLoggedIn( CBNET *bnet );
	void EventBNETGameRefreshed( CBNET *bnet );
	void EventBNETGameRefreshFailed( CBNET *bnet );
	void EventBNETConnectTimedOut( CBNET *bnet );
	void EventBNETWhisper( CBNET *bnet, string user, string message );
	void EventBNETChat( CBNET *bnet, string user, string message );
	void EventBNETEmote( CBNET *bnet, string user, string message );
	void EventGameDeleted( CBaseGame *game );

	// other functions
	virtual CMyCallableDownloadFile *ThreadedDownloadFile( string url, string path );

	void CheckConfigs( );

	void ExtractScripts( );
	void LoadIPToCountryData( );
	void LoadIPToCountryDataOpt( );
	void CreateGame( CMap *map, unsigned char gameState, bool saveGame, string gameName, string ownerName, string creatorName, string creatorServer, bool whisper );

	int m_CookieOffset;					// System used to remove need for bnet_bnlswardencookie. May need further optimization.
};

#endif
