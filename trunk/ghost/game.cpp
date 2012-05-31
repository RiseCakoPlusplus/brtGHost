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

//#include "stdio.h"
//#include "stdlib.h"
#ifdef WIN32
//#include "dirent.h"
#endif
#include "ghost.h"
#include "util.h"
#include "config.h"
#include "language.h"
#include "socket.h"
#include "ghostdb.h"
#include "bnet.h"
#include "map.h"
#include "packed.h"
#include "savegame.h"
//#include "replay.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "game_base.h"
#include "game.h"
#include "stats.h"
#include "statsdota.h"
#include "statsw3mmd.h"

#include <cmath>
#include <string.h>
#include <time.h>
#ifdef WIN32
//#include <windows.h>
#endif

//#include <boost/filesystem.hpp>
//#include <boost/regex.hpp>

//using namespace boost :: filesystem;

//
// sorting classes
//

class CGamePlayerSortAscByPing
{
public:
	bool operator( ) ( CGamePlayer *Player1, CGamePlayer *Player2 ) const
	{
		return Player1->GetPing( false ) < Player2->GetPing( false );
	}
};

class CGamePlayerSortDescByPing
{
public:
	bool operator( ) ( CGamePlayer *Player1, CGamePlayer *Player2 ) const
	{
		return Player1->GetPing( false ) > Player2->GetPing( false );
	}
};

//
// CGame
//

CGame :: CGame( CGHost *nGHost, CMap *nMap, CSaveGame *nSaveGame, uint16_t nHostPort, unsigned char nGameState, string nGameName, string nOwnerName, string nCreatorName, string nCreatorServer ) : CBaseGame( nGHost, nMap, nSaveGame, nHostPort, nGameState, nGameName, nOwnerName, nCreatorName, nCreatorServer )
{
	m_DBBanLast = NULL;
	m_DBGame = new CDBGame( 0, string( ), m_Map->GetMapPath( ), string( ), string( ), string( ), 0 );

	if( m_Map->GetMapType( ) == "w3mmd" )
		m_Stats = new CStatsW3MMD( this, m_Map->GetMapStatsW3MMDCategory( ) );
	else if( m_Map->GetMapType( ) == "dota" )
		m_Stats = new CStatsDOTA( this );
	else
		m_Stats = NULL;

	m_CallableGameAdd = NULL;

	m_KilledTowers = "";
	m_GameOverTime = 0;
	m_GameLoadedTime = 0;
	m_Server = nCreatorServer;
}

CGame :: ~CGame( )
{
	uint32_t timehasleft;
	uint32_t endtime = m_GameOverTime;
	if (endtime == 0)
		endtime = GetTime();
	for( vector<CDBGamePlayer *> :: iterator i = m_DBGamePlayers.begin( ); i != m_DBGamePlayers.end( ); i++ ) {
		if (IsAutoBanned((*i)->GetName()))
		{
			timehasleft = (*i)->GetLeavingTime();
			if (endtime>timehasleft+m_GHost->m_AutoBanGameEndMins*60)
			{
				string Reason = CustomReason( timehasleft, string(), (*i)->GetName() );
				Reason = "Autobanned"+Reason;
				CONSOLE_Print( "[AUTOBAN: " + m_GameName + "] Autobanning " + (*i)->GetName( ) + " (" + Reason +")" );

				m_GHost->m_Callables.push_back( m_GHost->m_DB->ThreadedBanAdd( (*i)->GetSpoofedRealm(), (*i)->GetName( ), (*i)->GetIP(), m_GameName, "AUTOBAN", Reason, 0, 0 ));
			}
		}
	}
	if( m_CallableGameAdd && m_CallableGameAdd->GetReady( ) )
	{
		m_DatabaseID = m_CallableGameAdd->GetResult();
		if( m_CallableGameAdd->GetResult( ) > 0 )
		{
			CONSOLE_Print( "[GAME: " + m_GameName + "] saving player/stats data to database" );

			// store the CDBGamePlayers in the database
			uint32_t EndingWarnMark = 0;
			if( m_DoAutoWarns )
			{
				uint32_t ElapsedTime = ( GetTime() - m_GameLoadedTime ) / 60;
				for( vector<uint32_t> :: iterator i = m_AutoWarnMarks.begin( ); i != m_AutoWarnMarks.end( ); i++ )
				{
					if( ElapsedTime >= *i )
						EndingWarnMark++;
				}
			}

			for( vector<CDBGamePlayer *> :: iterator i = m_DBGamePlayers.begin( ); i != m_DBGamePlayers.end( ); i++ )
			{
				if( m_DoAutoWarns && (*i)->GetLeftEarly( ) > 0 && (*i)->GetLeftEarly( ) + 2 <= EndingWarnMark )
				{
					string VictimLower = (*i)->GetName();
					transform( VictimLower.begin( ), VictimLower.end( ), VictimLower.begin( ), (int(*)(int))tolower );
					CDBBan *Match = NULL;

					for( vector<CDBBan *> :: iterator j = m_DBBans.begin( ); j != m_DBBans.end( ); j++ )
					{
						string TestName = (*j)->GetName( );
						transform( TestName.begin( ), TestName.end( ), TestName.begin( ), (int(*)(int))tolower );

						if( TestName.compare( VictimLower ) == 0 )
						{
							Match = *j;
							break;
						}
					}

					if( Match != NULL )
					{
						m_GHost->m_Callables.push_back( m_GHost->m_DB->ThreadedBanAdd( Match->GetServer( ), Match->GetName( ), Match->GetIP( ), m_GameName, "Autowarn", "Early leaver", m_GHost->m_WarnTimeOfWarnedPlayer, 1));

						uint32_t WarnCount = 0;
						for(int i = 0; i < 3 && WarnCount == 0; i++)
						{
//							if(i > 0)
//								MILLISLEEP(20);
							WarnCount = m_GHost->m_DB->BanCount( Match->GetName( ), 1 );
						}

						if(WarnCount >= m_GHost->m_BanTheWarnedPlayerQuota)
						{
							if( !Match->GetServer( ).empty( ) )
							{
								m_GHost->m_Callables.push_back( m_GHost->m_DB->ThreadedBanAdd( Match->GetServer( ), Match->GetName( ), Match->GetIP( ), m_GameName, "Autowarn", tr("lang_1125"), m_GHost->m_BanTimeOfWarnedPlayer, 0 ));
								m_GHost->m_Callables.push_back( m_GHost->m_DB->ThreadedWarnUpdate( Match->GetName( ), 3, 2));
								m_GHost->m_Callables.push_back( m_GHost->m_DB->ThreadedWarnUpdate( Match->GetName( ), 1, 3));
							}
							else
							{
								for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
								{
									m_GHost->m_Callables.push_back( m_GHost->m_DB->ThreadedBanAdd( (*i)->GetServer( ), Match->GetName( ), Match->GetIP( ), m_GameName, "Autowarn", tr("lang_1125"), m_GHost->m_BanTimeOfWarnedPlayer, 0 ));
									m_GHost->m_Callables.push_back( m_GHost->m_DB->ThreadedWarnUpdate( Match->GetName( ), 3, 2));
									m_GHost->m_Callables.push_back( m_GHost->m_DB->ThreadedWarnUpdate( Match->GetName( ), 1, 3));
								}
							}
						}
					}
				}

				if(m_GHost->m_GameNumToForgetAWarn > 0)
					// "forget" one of this player's warns
					m_GHost->m_WarnForgetQueue.push_back( (*i)->GetName( ));
//					m_GHost->m_Callables.push_back( m_GHost->m_DB->ThreadedWarnForget( (*i)->GetName( ), m_GHost->m_GameNumToForgetAWarn ));
			}

			for( vector<CDBGamePlayer *> :: iterator i = m_DBGamePlayers.begin( ); i != m_DBGamePlayers.end( ); i++ )
				m_GHost->m_Callables.push_back( m_GHost->m_DB->ThreadedGamePlayerAdd( m_CallableGameAdd->GetResult( ), (*i)->GetName( ), (*i)->GetIP( ), (*i)->GetSpoofed( ), (*i)->GetSpoofedRealm( ), (*i)->GetReserved( ), (*i)->GetLoadingTime( ), (*i)->GetLeft( ), (*i)->GetLeftReason( ), (*i)->GetTeam( ), (*i)->GetColour( ) ) );

			// store the stats in the database

			if( m_Stats )
			{
				m_Stats->Save( m_GHost, m_GHost->m_DB, m_CallableGameAdd->GetResult( ) );
				if (m_GHost->DBType == "mysql")
				if (m_GHost->m_UpdateDotaScoreAfterGame)
				{
					if (!m_GHost->m_CalculatingScores)
					{
						string formula = m_GHost->m_ScoreFormula;
						string mingames = m_GHost->m_ScoreMinGames;
//						m_GHost->m_CalculatingScores = true;
						CONSOLE_Print( "[GAME: " + m_GameName + "] calculating scores..." );
						m_PairedCalculateScores.push_back( PairedCalculateScores( m_OwnerName, m_GHost->m_DB->ThreadedCalculateScores( formula, mingames ) ) );
					}
				}
				else
				{
#ifdef WIN32
					if (m_GHost->m_UpdateDotaEloAfterGame)
						if (m_GHost->DBType == "mysql")
						{
							CONSOLE_Print( "[GAME: " + m_GameName + "] running update_dota_elo.exe" );
							system("update_dota_elo.exe");
						}
#endif
				}
			}
		}
		else
			CONSOLE_Print( "[GAME: " + m_GameName + "] unable to save player/stats data to database" );

		m_GHost->m_DB->RecoverCallable( m_CallableGameAdd );
		delete m_CallableGameAdd;
		m_CallableGameAdd = NULL;
	}

	for( vector<PairedBanCheck> :: iterator i = m_PairedBanChecks.begin( ); i != m_PairedBanChecks.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedBanAdd> :: iterator i = m_PairedBanAdds.begin( ); i != m_PairedBanAdds.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedBanRemove> :: iterator i = m_PairedBanRemoves.begin( ); i != m_PairedBanRemoves.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedRanks> :: iterator i = m_PairedRanks.begin( ); i != m_PairedRanks.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedCalculateScores> :: iterator i = m_PairedCalculateScores.begin( ); i != m_PairedCalculateScores.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedSafeAdd> :: iterator i = m_PairedSafeAdds.begin( ); i != m_PairedSafeAdds.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedSafeRemove> :: iterator i = m_PairedSafeRemoves.begin( ); i != m_PairedSafeRemoves.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedGPSCheck> :: iterator i = m_PairedGPSChecks.begin( ); i != m_PairedGPSChecks.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<PairedDPSCheck> :: iterator i = m_PairedDPSChecks.begin( ); i != m_PairedDPSChecks.end( ); i++ )
		m_GHost->m_Callables.push_back( i->second );

	for( vector<CDBBan *> :: iterator i = m_DBBans.begin( ); i != m_DBBans.end( ); i++ )
		delete *i;

	delete m_DBGame;

	for( vector<CDBGamePlayer *> :: iterator i = m_DBGamePlayers.begin( ); i != m_DBGamePlayers.end( ); i++ )
		delete *i;

	delete m_Stats;

	// it's a "bad thing" if m_CallableGameAdd is non NULL here
	// it means the game is being deleted after m_CallableGameAdd was created (the first step to saving the game data) but before the associated thread terminated
	// rather than failing horribly we choose to allow the thread to complete in the orphaned callables list but step 2 will never be completed
	// so this will create a game entry in the database without any gameplayers and/or DotA stats

	if( m_CallableGameAdd )
	{
		CONSOLE_Print( "[GAME: " + m_GameName + "] game is being deleted before all game data was saved, game data has been lost" );
		m_GHost->m_Callables.push_back( m_CallableGameAdd );
	}
}

bool CGame :: Update( void *fd, void *send_fd )
{
	// show score of
	if (!m_ShowScoreOf.empty())
	{
//		CONSOLE_Print( "[GAME: " + m_GameName + "] checking score for "+ m_ShowScoreOf );
		if (!m_GHost->m_CalculatingScores)
		m_PairedDPSChecks.push_back( PairedDPSCheck( "%", m_GHost->m_DB->ThreadedDotAPlayerSummaryCheck( m_ShowScoreOf, m_GHost->m_ScoreFormula, m_GHost->m_ScoreMinGames, string() ) ) );
		m_ShowScoreOf = string();
	}

	if (!m_PlayerScoreCheck.empty())
	{
		if (!m_GHost->m_CalculatingScores)
			m_PairedDPSChecks.push_back( PairedDPSCheck( "n", m_GHost->m_DB->ThreadedDotAPlayerSummaryCheck( m_PlayerScoreCheck, m_GHost->m_ScoreFormula, m_GHost->m_ScoreMinGames, string() ) ) );

		m_PlayerScoreCheck = string();
	}

	// show note of
	if (!m_ShowNoteOf.empty())
	{
		//		CONSOLE_Print( "[GAME: " + m_GameName + "] checking score for "+ m_ShowScoreOf );
		bool noted = false;
		string note = Note(m_ShowNoteOf);
		noted = IsNoted( m_ShowNoteOf);
		if (noted)
			SendAdminChat(m_ShowNoteOf+" - "+note);
		m_ShowNoteOf=string();
	}

	// removing creator from friend list

	if (m_GHost->m_addcreatorasfriendonhost && m_CreatorAsFriend )
	if (GetTime()>m_CreationTime+15)
	{
		for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
		{
			if( (*i)->GetServer( ) == m_CreatorServer )
				(*i)->QueueChatCommand( "/f r "+m_OwnerName);
		}
		m_CreatorAsFriend = false;
	}

	// check if we should disable stats (for ex: -wtf detected)
	if (m_DisableStats)
	{
		if (m_Stats)
			delete m_Stats;

		m_DisableStats = false;
	}

	// update callables

	for( vector<PairedBanCheck> :: iterator i = m_PairedBanChecks.begin( ); i != m_PairedBanChecks.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			CDBBan *Ban = i->second->GetResult( );

			if( Ban )
                if( Ban->GetExpireDate().empty())
                SendAllChat( tr("lang_0011", "$SERVER$", i->second->GetServer( ), "$VICTIM$", i->second->GetUser( ), "$DATE$", Ban->GetDate( ), "$ADMIN$", Ban->GetAdmin( ), "$REASON$", Ban->GetReason( ))); 
				else
                SendAllChat( tr("lang_0500", "$SERVER$", i->second->GetServer( ), "$VICTIM$", i->second->GetUser( ), "$DATE$", Ban->GetDate( ), "$ADMIN$", Ban->GetAdmin( ), "$REASON$", Ban->GetReason( ), "$DAYSREMAINING$", Ban->GetExpireDate())); //				SendAllChat( m_GHost->m_Language->UserWasBannedOnByBecause( i->second->GetServer( ), i->second->GetUser( ), Ban->GetDate( ), Ban->GetDaysRemaining( ), Ban->GetAdmin( ), Ban->GetReason( ), Ban->GetExpireDate() ) ); // UserWasBannedOnByBecause

			else
                SendAllChat( tr("lang_0012", "$SERVER$", i->second->GetServer( ), "$VICTIM$", i->second->GetUser( ))); // SendAllChat( m_GHost->m_Language->UserIsNotBanned( i->second->GetServer( ), i->second->GetUser( ) ) ); // UserIsNotBanned

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedBanChecks.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedBanAdd> :: iterator i = m_PairedBanAdds.begin( ); i != m_PairedBanAdds.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			if( i->second->GetResult( ) )
			{
				for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
				{
					string sDate = string();
					if (i->second->GetExpireDayTime()>0)
					{
						struct tm * timeinfo;
						char buffer [80];
						time_t Now = time( NULL );
						Now += 3600*24*i->second->GetExpireDayTime();
						timeinfo = localtime( &Now );
						strftime (buffer,80,"%d-%m-%Y",timeinfo);
						sDate = buffer;
					}

					if( (*j)->GetServer( ) == i->second->GetServer( ) )
						(*j)->AddBan( i->second->GetUser( ), i->second->GetIP( ), i->second->GetGameName( ), i->second->GetAdmin( ), i->second->GetReason( ), sDate );
				}

//				SendAllChat( m_GHost->m_Language->PlayerWasBannedByPlayer( i->second->GetServer( ), i->second->GetUser( ), i->first ) );
			}

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedBanAdds.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedBanRemove> :: iterator i = m_PairedBanRemoves.begin( ); i != m_PairedBanRemoves.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			if( i->second->GetResult( ) )
			{
				for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
				{
					if( (*j)->GetServer( ) == i->second->GetServer( ) )
						(*j)->RemoveBan( i->second->GetUser( ) );
				}
			}

			CGamePlayer *Player = GetPlayerFromName( i->first, true );

			if( Player )
			{
				if( i->second->GetResult( ) )
					SendChat( Player, tr("lang_0022", i->second->GetUser( ) ) ); // UnbannedUser
				else
					SendChat( Player, tr("lang_0023", i->second->GetUser( ) ) ); // ErrorUnbanningUser
			}

			if (m_GHost->m_UnbanRemovesChannelBans)
			{
				for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
					(*j)->QueueChatCommand("/unban "+i->second->GetUser( ));
			}

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedBanRemoves.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedRanks> :: iterator i = m_PairedRanks.begin( ); i != m_PairedRanks.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			string Scores = i->second->GetResult();
//			if (Scores.length()>160)
//				Scores = Scores.substr(0,160);
			SendAllChat(Scores);

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedRanks.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedCalculateScores> :: iterator i = m_PairedCalculateScores.begin( ); i != m_PairedCalculateScores.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			bool ok = i->second->GetResult();
			m_GHost->m_CalculatingScores = false;
			if (ok)
			{
				m_GHost->CalculateScoresCount();
				SendAllChat(tr("lang_1136", UTIL_ToString(m_GHost->ScoresCount())));  // m_GHost->ScoresCount())+ " scores have been calculated"
			}

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedCalculateScores.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedSafeAdd> :: iterator i = m_PairedSafeAdds.begin( ); i != m_PairedSafeAdds.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			if( i->second->GetResult( ) )
			{
				for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
				{
					if( (*j)->GetServer( ) == i->second->GetServer( ) )
						(*j)->AddSafe( i->second->GetUser( ), i->second->GetVoucher( ));
				}
				SendAllChat(tr( "lang_0992", i->second->GetUser( ))); // AddedPlayerToTheSafeList
			} else
				SendAllChat(tr( "lang_1137", i->second->GetUser( )));  // "Error adding "+i->second->GetUser( )+" to safelist")

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedSafeAdds.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedSafeRemove> :: iterator i = m_PairedSafeRemoves.begin( ); i != m_PairedSafeRemoves.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			if( i->second->GetResult( ) )
			{
				for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
				{
					if( (*j)->GetServer( ) == i->second->GetServer( ) )
						(*j)->RemoveSafe( i->second->GetUser( ));
				}
				SendAllChat(tr( "lang_0993", i->second->GetUser( ))); // RemovedPlayerFromTheSafeList
			} else
				SendAllChat(tr( "lang_1138", i->second->GetUser( )));  // "Error removing "+i->second->GetUser( )+" from safelist"

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedSafeRemoves.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedGPSCheck> :: iterator i = m_PairedGPSChecks.begin( ); i != m_PairedGPSChecks.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			CDBGamePlayerSummary *GamePlayerSummary = i->second->GetResult( );

			if( GamePlayerSummary )
			{
				if( i->first.empty( ) )
					SendAllChat( tr("lang_0061","$USER$", i->second->GetName( ), "$FIRSTGAME$", GamePlayerSummary->GetFirstGameDateTime( ), "$LASTGAME$", GamePlayerSummary->GetLastGameDateTime( ), "$TOTALGAMES$", UTIL_ToString( GamePlayerSummary->GetTotalGames( ) ), "$AVGLOADINGTIME$", UTIL_ToString( (float)GamePlayerSummary->GetAvgLoadingTime( ) / 1000, 2 ), "$AVGSTAY$", UTIL_ToString( GamePlayerSummary->GetAvgLeftPercent( ) ) ) ); // HasPlayedGamesWithThisBot
				else
				{
					CGamePlayer *Player = GetPlayerFromName( i->first, true );

					if( Player )
					SendChat(Player, tr("lang_0061","$USER$", i->second->GetName( ), "$FIRSTGAME$", GamePlayerSummary->GetFirstGameDateTime( ), "$LASTGAME$", GamePlayerSummary->GetLastGameDateTime( ), "$TOTALGAMES$", UTIL_ToString( GamePlayerSummary->GetTotalGames( ) ), "$AVGLOADINGTIME$", UTIL_ToString( (float)GamePlayerSummary->GetAvgLoadingTime( ) / 1000, 2 ), "$AVGSTAY$", UTIL_ToString( GamePlayerSummary->GetAvgLeftPercent( ) ) ) ); // HasPlayedGamesWithThisBot
						//SendChat( Player, m_GHost->m_Language->HasPlayedGamesWithThisBot( i->second->GetName( ), GamePlayerSummary->GetFirstGameDateTime( ), GamePlayerSummary->GetLastGameDateTime( ), UTIL_ToString( GamePlayerSummary->GetTotalGames( ) ), UTIL_ToString( (float)GamePlayerSummary->GetAvgLoadingTime( ) / 1000, 2 ), UTIL_ToString( GamePlayerSummary->GetAvgLeftPercent( ) ) ) ); // HasPlayedGamesWithThisBot
				}
			}
			else
			{
				if( i->first.empty( ) )
					SendAllChat( tr("lang_0062", i->second->GetName( ) ) ); // HasntPlayedGamesWithThisBot
				else
				{
					CGamePlayer *Player = GetPlayerFromName( i->first, true );

					if( Player )
						SendChat( Player, tr("lang_0062", i->second->GetName( ) ) ); // HasntPlayedGamesWithThisBot
				}
			}

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedGPSChecks.erase( i );
		}
		else
			i++;
	}

	for( vector<PairedDPSCheck> :: iterator i = m_PairedDPSChecks.begin( ); i != m_PairedDPSChecks.end( ); )
	{
		if( i->second->GetReady( ) )
		{
			CDBDotAPlayerSummary *DotAPlayerSummary = i->second->GetResult( );

			bool sd = false;
			bool show_score = true;
			bool Whisper = !i->first.empty();
			string name = i->first;

			if (i->first[0]=='%')
			{
				name = i->first.substr(1,i->first.length()-1);
				Whisper = i->first.length()>1;
				sd = true;
			} 
			else if (i->first[0]=='n') show_score = false; // If we don't want show score when player join

			if (sd)
			if( DotAPlayerSummary )
			{

				bool isAdmin = false;
				bool isRootAdmin = false;

				if (m_GHost->m_dontshowsdforadmins)
				for( vector<CBNET *> :: iterator it = m_GHost->m_BNETs.begin( ); it != m_GHost->m_BNETs.end( ); it++ )
				{

					if ( (*it)->IsRootAdmin( i->second->GetName() ) )
					{
						isRootAdmin = true;
						break;
					}

					if( (*it)->IsAdmin( i->second->GetName() ) )
					{
						isAdmin = true;
						break;
					} 
				}

				uint32_t scorescount = m_GHost->ScoresCount();

				CGamePlayer *PlayerN = GetPlayerFromName( i->second->GetName(), true );

				if( PlayerN )
				{
					PlayerN->SetScore( DotAPlayerSummary->GetScore() );
					PlayerN->SetScoreS(UTIL_ToString( DotAPlayerSummary->GetScore(), 2));
					PlayerN->SetRankS(UTIL_ToString( DotAPlayerSummary->GetRank()));
				}

				string RankS = UTIL_ToString( DotAPlayerSummary->GetRank());

                string Summary;
				string leave_games_count = UTIL_ToString( (100 * DotAPlayerSummary->GetLeaveCount()) / DotAPlayerSummary->GetTotalGames( )); // In percent
				string player_class = tr("lang_1061");
				float newbie_value = 1.5f;

				if (DotAPlayerSummary->GetRank()>0)
					RankS = RankS + "/" + UTIL_ToString(scorescount);

                player_class = "";

                if (DotAPlayerSummary->GetKillsPerGame( ) && DotAPlayerSummary->GetDeathsPerGame( ) && DotAPlayerSummary->GetAssistsPerGame( ) )

				if (  (DotAPlayerSummary->GetTotalGames( ) > 10) &&
					 ((DotAPlayerSummary->GetKillsPerGame() * newbie_value + DotAPlayerSummary->GetAssistsPerGame()) / DotAPlayerSummary->GetDeathsPerGame( ) < newbie_value) ||
					  (DotAPlayerSummary->GetCreepDeniesPerGame() < newbie_value))
					player_class = tr("lang_1061"); else // Newbie

                if (DotAPlayerSummary->GetKillsPerGame( ) >= DotAPlayerSummary->GetDeathsPerGame( ) &&
                    DotAPlayerSummary->GetKillsPerGame( ) >= DotAPlayerSummary->GetAssistsPerGame( ))
                    player_class = tr("lang_1059"); else 
                if (DotAPlayerSummary->GetDeathsPerGame( ) >= DotAPlayerSummary->GetAssistsPerGame( ) &&
                    DotAPlayerSummary->GetDeathsPerGame( ) >= DotAPlayerSummary->GetKillsPerGame( ))
                    player_class = tr("lang_1060"); else 
                if (DotAPlayerSummary->GetAssistsPerGame( ) >= DotAPlayerSummary->GetKillsPerGame( ) &&
                    DotAPlayerSummary->GetAssistsPerGame( ) >= DotAPlayerSummary->GetDeathsPerGame( ))
                    player_class =  tr("lang_1058");


				
				string sd_lang = "lang_0995";

				if (isAdmin) 
				{
					player_class = tr("lang_1061a"); // Administrator
					sd_lang = "lang_0995a";
				}
				else
					if (isRootAdmin) 
					{
						player_class = tr("lang_1061r"); // Root admin
						sd_lang = "lang_0995a";
					}

                if (DotAPlayerSummary->GetScore()) // HasPlayedDotAGamesWithThisBot2

				Summary = tr(sd_lang,
					  "$USER$", i->second->GetName( ),
					  "$TOTALGAMES$", UTIL_ToString(DotAPlayerSummary->GetTotalGames( )),
					  "$COUNT$", leave_games_count,
					  "$RANK$", RankS,
					  "$LEAVE$", UTIL_ToString(DotAPlayerSummary->GetLeaveCount()),
					  "$LEAVEPCNT$", leave_games_count,
					  "$SCORE$", UTIL_ToString(DotAPlayerSummary->GetScore()),
					  "$WINS$", UTIL_ToString( DotAPlayerSummary->GetWinsPerGame( )),
					  "$CLASS$", player_class,
					  "$KILLS$", UTIL_ToString( DotAPlayerSummary->GetKillsPerGame( )),
					  "$DEATHS$", UTIL_ToString( DotAPlayerSummary->GetDeathsPerGame( )),
					  "$ASSISTS$", UTIL_ToString( DotAPlayerSummary->GetAssistsPerGame( )),
					  "$CKILLS$", UTIL_ToString( DotAPlayerSummary->GetCreepKillsPerGame( )),
					  "$CDENIED$", UTIL_ToString( DotAPlayerSummary->GetCreepDeniesPerGame( )));
				 else
                Summary = tr("lang_1000", "$USER$", i->second->GetName( ),
					"$TOTALGAMES$", UTIL_ToString( DotAPlayerSummary->GetTotalGames( )),
                    "$WPG$", UTIL_ToString( DotAPlayerSummary->GetWinsPerGame( )),
					"$LPG$", UTIL_ToString( DotAPlayerSummary->GetLossesPerGame( )),
                    "$KPG$", UTIL_ToString( DotAPlayerSummary->GetKillsPerGame( )),
					"$DPG$", UTIL_ToString( DotAPlayerSummary->GetDeathsPerGame( )),
                    "$CKPG$", UTIL_ToString( DotAPlayerSummary->GetCreepKillsPerGame( )),
					"$CDPG$", UTIL_ToString( DotAPlayerSummary->GetCreepDeniesPerGame( )),
                    "$APG$", UTIL_ToString( DotAPlayerSummary->GetAssistsPerGame( )),
					"$NKPG$", UTIL_ToString( DotAPlayerSummary->GetNeutralKillsPerGame( )),
                    "$TKPG$", UTIL_ToString( DotAPlayerSummary->GetTowerKillsPerGame( )),
					"$RKPG$", UTIL_ToString( DotAPlayerSummary->GetRaxKillsPerGame( )),
                    "$CouKPG$", UTIL_ToString( DotAPlayerSummary->GetCourierKillsPerGame( )),
                    "$SCORE$", UTIL_ToString( DotAPlayerSummary->GetScore(), 2),
					"$TITLE1$", player_class,
                    "$RANK$", RankS);

				if (show_score)
					if (!Whisper)
						SendAllChat(Summary);
					else
					{
						CGamePlayer *Player = GetPlayerFromName( i->first, true );

						if( Player )
							SendChat( Player, Summary );
					}
			}
			if (!sd)
			if( DotAPlayerSummary )
			{   //  HasPlayedDotAGamesWithThisBot

			      string Summary = tr("lang_0074","$USER$", i->second->GetName( ),
                                                                        "$TOTALGAMES$", UTIL_ToString( DotAPlayerSummary->GetTotalGames( ) ),
                                                                        "$TOTALWINS$", UTIL_ToString( DotAPlayerSummary->GetTotalWins( ) ),
                                                                        "$TOTALLOSSES$", UTIL_ToString( DotAPlayerSummary->GetTotalLosses( ) ),
                                                                        "$TOTALKILLS$", UTIL_ToString( DotAPlayerSummary->GetTotalKills( ) ),
                                                                        "$TOTALDEATHS$", UTIL_ToString( DotAPlayerSummary->GetTotalDeaths( ) ),
                                                                        "$TOTALASSISTS$", UTIL_ToString( DotAPlayerSummary->GetTotalAssists( ) ),
                                                                        "$TOTALCREEPKILLS$", UTIL_ToString( DotAPlayerSummary->GetTotalCreepKills( ) ),
                                                                        "$TOTALCREEPDENIES$", UTIL_ToString( DotAPlayerSummary->GetTotalCreepDenies( ) ),
                                                                        "$TOTALNEUTRALKILLS$", UTIL_ToString( DotAPlayerSummary->GetTotalNeutralKills( ) ),
                                                                        "$TOTALTOWERKILLS$", UTIL_ToString( DotAPlayerSummary->GetTotalTowerKills( ) ),
                                                                        "$TOTALRAXKILLS$", UTIL_ToString( DotAPlayerSummary->GetTotalRaxKills( ) ),
                                                                        "$TOTALCOURIERKILLS$", UTIL_ToString( DotAPlayerSummary->GetTotalCourierKills( ) ),
                                                                        "$AVGKILLS$", UTIL_ToString( DotAPlayerSummary->GetAvgKills( ), 2 ),
                                                                        "$AVGDEATHS$", UTIL_ToString( DotAPlayerSummary->GetAvgDeaths( ), 2 ),
                                                                        "$AVGASSISTS$", UTIL_ToString( DotAPlayerSummary->GetAvgAssists( ), 2 ),
                                                                        "$AVGCREEPKILLS$", UTIL_ToString( DotAPlayerSummary->GetAvgCreepKills( ), 2 ),
                                                                        "$AVGCREEPDENIES$", UTIL_ToString( DotAPlayerSummary->GetAvgCreepDenies( ), 2 ),
                                                                        "$AVGNEUTRALKILLS$", UTIL_ToString( DotAPlayerSummary->GetAvgNeutralKills( ), 2 ) );


				if( i->first.empty( ) )
					SendAllChat( Summary );
				else
				{
					CGamePlayer *Player = GetPlayerFromName( i->first, true );

					if( Player )
						SendChat( Player, Summary );
				}
			}
			else
			{
				if( i->first.empty( ) )
					SendAllChat( tr("lang_0075", i->second->GetName( ) ) ); // HasntPlayedDotAGamesWithThisBot
				else
				{
					CGamePlayer *Player = GetPlayerFromName( i->first, true );

					if( Player )
						SendChat( Player, tr("lang_0075", i->second->GetName( ) ) ); // HasntPlayedDotAGamesWithThisBot
				}
			}

			m_GHost->m_DB->RecoverCallable( i->second );
			delete i->second;
			i = m_PairedDPSChecks.erase( i );
		}
		else
			i++;
	}

	return CBaseGame :: Update( fd, send_fd );
}


// this function is only called when a player leave packet is received, not when there's a socket error, kick, etc...
void CGame :: EventPlayerLeft( CGamePlayer *player, uint32_t reason  )
{
	// Check if leaver is admin/root admin with a loop then set the bool accordingly.
	bool isAdmin = false;
	for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
	{
		if( (*j)->IsAdmin(player->GetName( ) ) || (*j)->IsRootAdmin( player->GetName( )) || (*j)->IsSafe(player->GetName( )) )
		{
			isAdmin = true;
			break;
		}
		if( (*j)->IsClanFullMember(player->GetName( ) )  )
		{
			isAdmin = true;
			break;
		}
		if (m_GHost->m_SafelistedBanImmunity)
		if( (*j)->IsSafe(player->GetName( ) )  )
		{
			isAdmin = true;
			break;
		}
	}

	// Auto Ban (if m_AutoBan is set to 1)
	// Current Triggers : Map is two team, even playered, is not a admin, game ended timer not started, game has loaded, leaver makes game uneven and conditions are met OR game is loading
	// Start with not banning the player (innocent until proven guilty lol)
	m_BanOn = false;
	if (m_GHost->m_AutoBan && !isAdmin) {
		// Check if the game has loaded + is not a admin + has not ended + bot_autoban = 1
		if (m_GameLoaded && !m_GameEnded) {
			// If m_AutoBanAll is on
			if (m_GHost->m_AutoBanAll) { m_BanOn = true; }
			// If there is even amount of players on the two teamed map loaded
			if (m_EvenPlayeredTeams) {
				// first check the teams
				ReCalculateTeams();
				// set up the player's SID
				unsigned char SID = GetSIDFromPID( player->GetPID( ) );
				unsigned char fteam;
				if (SID==255) {
					CBaseGame :: EventPlayerLeft( player, reason );
					return;
				}
				fteam = m_Slots[SID].GetTeam();
				// If team is even then turn on auto ban
				if(m_TeamDiff == 0) { m_BanOn = true; }
				// Else if teams are uneven check which one it is. Then depending on the team that is uneven, check to see if we should ban the leaver for making it uneven or more uneven.
				else if(m_TeamDiff > 0)
				{
					// If leaver is on team one then check if it is the team with less players.
					if (fteam == 0) {
						// If it is then turn on Auto Ban.
						if (m_Team1<m_Team2) { m_BanOn = true; }
					}
					// If leaver is on team two then check if it is the team with less players.
					else if (fteam == 1) {
						// If it is then turn on Auto Ban.
						if (m_Team2<m_Team1) { m_BanOn = true; }
					}
				}
				// If m_AutoBanTeamDiffMax is set to something other than 0 and m_TeamDiff is greater than m_AutoBanTeamDiffMax. All TeamDifference based triggers are overwritten.
				if (m_TeamDiff > m_GHost->m_AutoBanTeamDiffMax && m_GHost->m_AutoBanTeamDiffMax > 0) {	m_BanOn = false; }
			}
			// if m_AutoBanFirstXLeavers is set check if this leaver has exceeded the max number of leavers to ban. If so turn ban off. Overides all but timer.
			if (m_GHost->m_AutoBanFirstXLeavers > 0 && m_PlayersLeft < m_GHost->m_AutoBanFirstXLeavers) { m_BanOn = true; }
			// If m_AutoBanTimer is set to something other than 0. If time is exceeded then turn off ban. Nothing overides this but auto ban being off.
			if (m_GHost->m_AutoBanTimer > 0) {
				float iTime = (float)(GetTime() - m_GameLoadedTime)/60;
				if (m_GetMapType == "dota") { if (iTime>2) { iTime -=2; } else { iTime = 1; } }
				// If the in game time in mins if over the time set in m_AutoBanTimer then overwrite any triggers that turn on Auto Ban.
				if (iTime > m_GHost->m_AutoBanTimer) { m_BanOn = false; }
			}
		} else if (m_GameLoading && m_GHost->m_AutoBanGameLoading) {
			// If game is loading and player is not a admin ban.
			m_BanOn = true;
		} else if ( m_CountDownStarted && !m_GameLoading && !m_GameLoaded && m_GHost->m_AutoBanCountDown ) {
			// If game is loading and player is not a admin ban
			m_BanOn = true;
		}
		// If m_BanOn got turned on for some reason ban.
		if (m_BanOn) {
			string timediff = UTIL_ToString(m_GHost->m_AutoBanGameEndMins);
			// Send info about the leaver
			SendAllChat( "[AUTOBAN: " + m_GameName + "] " + player->GetName( ) + " will be banned if he/she has not left within " + timediff + " mins of game over time." );
			CONSOLE_Print( "[AUTOBAN: " + m_GameName + "] Adding " + player->GetName() + " to the temp banlist in case he/she leaves not within " + timediff + " mins of game over time." );
			// Add player to the temp vector
			m_AutoBanTemp.push_back(player->GetName());
		}
	}
	CBaseGame :: EventPlayerLeft( player, reason );
}

void CGame :: EventPlayerDeleted( CGamePlayer *player )
{
	if ( !m_GameEnded && m_GameLoaded && !player->GetFFVote() )
	{
		player->SetFFVote( true );
		EventDotaGameFastFinishProcess( player, "lang_1212", false );
	}

	CBaseGame :: EventPlayerDeleted( player );

	// record everything we need to know about the player for storing in the database later
	// since we haven't stored the game yet (it's not over yet!) we can't link the gameplayer to the game
	// see the destructor for where these CDBGamePlayers are stored in the database
	// we could have inserted an incomplete record on creation and updated it later but this makes for a cleaner interface

	if( m_GameLoading || m_GameLoaded )
	{
		// todotodo: since we store players that crash during loading it's possible that the stats classes could have no information on them
		// that could result in a DBGamePlayer without a corresponding DBDotAPlayer - just be aware of the possibility

		unsigned char SID = player->GetSID();
//		unsigned char SID = GetSIDFromPID( player->GetPID( ) );
		unsigned char Team = 255;
		unsigned char Colour = 255;

		if( SID < m_Slots.size( ) )
		{
			Team = m_Slots[SID].GetTeam( );
			Colour = m_Slots[SID].GetColour( );
		}

		uint32_t LeftEarly = 0;
//		if( m_DoAutoWarns && GetNumPlayers() >= m_GetMapOnlyAutoWarnIfMoreThanXPlayers && ( m_GameLoaded ) && player->GetLeftReason( ).compare( m_GHost->m_Language->HasLeftVoluntarily( ) ) == 0 )
		if( m_DoAutoWarns && GetNumPlayers() >= m_GetMapOnlyAutoWarnIfMoreThanXPlayers && ( m_GameLoaded ) )
		{
			uint32_t ElapsedTime = ( GetTime() - m_GameLoadedTime ) / 60;
			for( vector<uint32_t> :: iterator i = m_AutoWarnMarks.begin( ); i != m_AutoWarnMarks.end( ); i++ )
			{
				if( ElapsedTime >= *i )
					LeftEarly++;
			}
			// TODO: send a game message saying this player has a potential to get warned?
		}

		if(LeftEarly > 0)
		{
			// check if this guy's an admin...
			for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
			{
				if( (m_GHost->m_SafelistedBanImmunity && IsSafe( player->GetName())) || IsAdmin( player->GetName() ) || (*j)->IsRootAdmin( player->GetName() ) )
				{
					LeftEarly = 0;
					break;
				}
			}
		}

		m_DBGamePlayers.push_back( new CDBGamePlayer( 0, 0, player->GetName( ), player->GetExternalIPString( ), player->GetSpoofed( ) ? 1 : 0, player->GetSpoofedRealm( ), player->GetReserved( ) ? 1 : 0, player->GetFinishedLoading( ) ? player->GetFinishedLoadingTicks( ) - m_StartedLoadingTicks : 0, m_GameTicks / 1000, player->GetLeftReason( ), Team, Colour, SID, player->GetCountry(), GetTime(), m_Team1, m_Team2,LeftEarly ) );

		// also keep track of the last player to leave for the !banlast command

		for( vector<CDBBan *> :: iterator i = m_DBBans.begin( ); i != m_DBBans.end( ); i++ )
		{
			if( (*i)->GetName( ) == player->GetName( ) )
				m_DBBanLast = *i;
		}

		// Add to a player leaver counter
		m_PlayersLeft++;
	}
}

void CGame :: EventPlayerAction( CGamePlayer *player, CIncomingAction *action )
{
	CBaseGame :: EventPlayerAction( player, action );

	// give the stats class a chance to process the action

	if( m_Stats && m_Stats->ProcessAction( action ) && m_GameEndedTime == 0 )
	{
//		CONSOLE_Print( "[GAME: " + m_GameName + "] gameover timer started (stats class reported game over)" );
		SendEndMessage( );
		m_GameEnded = true;
		m_GameEndedTime = GetTime( );
//		m_GameOverTime = GetTime( );
	}
}

bool CGame :: EventPlayerBotCommand( CGamePlayer *player, string command, string payload )
{
	bool HideCommand = CBaseGame :: EventPlayerBotCommand( player, command, payload );

	// todotodo: don't be lazy

	string User = player->GetName( );
	string Command = command;
	string Payload = payload;

	uint32_t AdminAccess = 0;
	bool AdminCheck = false;
	bool BluePlayer = false;

	CGamePlayer *p = NULL;
	unsigned char Nrt;
	unsigned char Nr = 255;
	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		Nrt = GetSIDFromPID((*i)->GetPID());
		if (Nrt<Nr)
		{
			Nr = Nrt;
			p = (*i);
		}
	}

	// this is blue player
	if (p)
		if (p->GetPID()==player->GetPID())
			BluePlayer = true;

	if (BluePlayer && m_GHost->m_BlueIsOwner)
	{
		AdminCheck = true;
		AdminAccess = m_GHost->CMDAccessAddOwner(0);
	}

	if (IsOwner(User))
	{
		AdminCheck = true;
		AdminAccess = m_GHost->CMDAccessAddOwner(0);
	}

	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		if(( (*i)->GetServer( ) == player->GetSpoofedRealm( ) && (*i)->IsAdmin( User )) )
		{
			AdminCheck = true;
			AdminAccess = (*i)->LastAccess();
			if (IsOwner(User))
				AdminAccess = m_GHost->CMDAccessAddOwner(AdminAccess);
			break;
		}
	}

// upgrade LAN players to admins in case bot_lanadmins = 1
// upgrade local players to admins in case bot_localadmins = 1
	if ((m_GHost->m_LanAdmins && player->IsLAN()) || (m_GHost->m_LocalAdmins && player->GetExternalIPString()=="127.0.0.1" ))
	{
		AdminCheck = true;
		AdminAccess = m_GHost->CMDAccessAddOwner(AdminAccess);
	}

	bool RootAdminCheck = false;

	// upgrade LAN players to rootadmins in case bot_lanrootadmins = 1
	if ((m_GHost->m_LanRootAdmins && player->IsLAN()) )
	{
		RootAdminCheck = true;
	}

	for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
	{
		if( (*i)->GetServer( ) == player->GetSpoofedRealm( ) && (*i)->IsRootAdmin( User ) )
		{
			RootAdminCheck = true;
			break;
		}
	}

	if (RootAdminCheck)
	{
		BluePlayer = true;
		AdminAccess = CMDAccessAll();
	}

	if( AdminCheck || RootAdminCheck )
	{
		CONSOLE_Print( "[GAME: " + m_GameName + "] admin [" + User + "] sent command [" + Command + "] with payload [" + Payload + "]" );

		if( !m_Locked || RootAdminCheck || IsOwner( User ) )
		{
			/*****************
			* ADMIN COMMANDS *
			******************/

			//
			// !AUTOBAN
			// !AB
			//

			if( ( Command == "autoban" || Command == "ab" ) ) {
				if (m_GHost->m_AutoBan) {
					m_GHost->m_AutoBan = false;
					SendAllChat( tr("lang_1118") ); //  Auto Ban is OFF"
				} else {
					m_GHost->m_AutoBan = true;
					SendAllChat( tr("lang_1119") ); // "Auto Ban is ON"
				}
			}

			//
			// !AUTOBALANCE
			// !ABC
			//

			else if ( Command == "autobalance" || Command == "abc")
			{
                BalanceSlots();
			}

			//
			// !ABORT (abort countdown)
			// !A
			//

			// we use "!a" as an alias for abort because you don't have much time to abort the countdown so it's useful for the abort command to be easy to type

			else if( ( Command == "abort" || Command == "a" ) && m_CountDownStarted && !m_GameLoading && !m_GameLoaded && !m_NormalCountdown )
			{
				SendAllChat( tr("lang_0049") );
				m_CountDownStarted = false;
			}

			//
			// !CD
			// !COUNTDOWN
			//

			else if( ( Command == "cd" || Command == "countdown" )  )
			{
				if (!RootAdminCheck)
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}

				if (!m_NormalCountdown)
				{
					SendChat(player->GetPID(), tr("lang_1148")); // "Normal countdown active !"
					m_NormalCountdown = true;
				} else
				{
					SendChat(player->GetPID(), tr("lang_1149")); // "Ghost countdown active !"
					m_NormalCountdown = false;
				}
			}

			//
			// !QUERY
			//

			else if( ( Command == "query" || Command == "q"  ) && !Payload.empty() )
			{
				if (!RootAdminCheck)
				{
					SendChat(player->GetPID(),(tr("lang_0005")));
					return HideCommand;
				}

				SendChat(player->GetPID(),tr("lang_1150")); // "Running query..."
				m_RunQueries.push_back(m_GHost->m_DB->ThreadedRunQuery(Payload));
			}

			//
			// !TOPC
			//

			else if( Command == "topc")
			{
				if (!RootAdminCheck)
				{
					SendChat(player->GetPID(),(tr("lang_0005")));
					return HideCommand;
				}

				string formula = m_GHost->m_ScoreFormula;
				string mingames = m_GHost->m_ScoreMinGames;
				bool calculated = false;
				if (m_GHost->m_CalculatingScores)
					return HideCommand;

#ifdef WIN32
				if (m_GHost->m_UpdateDotaEloAfterGame && m_GHost->DBType == "mysql")
				{
					calculated = true;
					SendChat(player->GetPID(), tr("lang_1152")); // "Running update_dota_elo.exe"
					system("update_dota_elo.exe");
				}
#endif
				if (!calculated)
				{
					SendChat(player->GetPID(), tr("lang_1151")); // "Calculating new scores, this may take a while"
					m_GHost->m_CalculatingScores = true;
					m_PairedCalculateScores.push_back( PairedCalculateScores( User, m_GHost->m_DB->ThreadedCalculateScores( formula, mingames ) ) );
				}
			}

			//
			// !top10 !top
			//

			else if( ( Command == "top10" || Command =="top" ) )
			{
				if (m_GHost->m_norank)
				{
					SendChat(player->GetPID(), tr("lang_1121")); // Why compare yourself to others? You are unique! :)
					return HideCommand;
				}
				m_PairedRanks.push_back( PairedRanks( User, m_GHost->m_DB->ThreadedRanks( m_Server) ) );
			}

			//
			// !ACCESS , !ACCLST, !ACC
			//

			else if(( Command == "access" || Command == "acc" || Command == "acclst") && RootAdminCheck )
			{
				// show available commands
				if (Payload.empty() || Command == "acclst")
				{
					string cmds = string();
					for (unsigned int i=0; i<=m_GHost->m_Commands.size()-1; i++)
					{
						if (m_GHost->CommandAllowedToShow(m_GHost->m_Commands[i]))
						{
							cmds +=m_GHost->m_Commands[i]+"["+UTIL_ToString(i)+"] ";

							if (i<m_GHost->m_Commands.size()-1)
								cmds +=" ";
						}
					}
					SendAllChat( cmds );
					return HideCommand;
				}
				uint32_t access = 0;
				bool isAdmin = false;
				string saccess = string();
				string cmd = string();
				string acc = string();
				string suser = Payload;
				uint32_t scmd = 0;
				uint32_t sacc = 0;
				uint32_t Mask = 1;
				uint32_t son = 0;
				bool showonly = true;

				if( Payload.find( " " )!= string :: npos)
				{
					showonly = false;

					stringstream SS;
					SS<<Payload;
					SS>>suser;
					SS>>scmd;
					if( SS.fail( )  )
					{
						SendAllChat( tr("lang_1154") ); //  "bad input #2 to access command"
						return HideCommand;
					}
					SS>>sacc;
					if( SS.fail( )  )
					{
						// acc suser 1 or 0 : set access to all commands or none.
						for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
						{
							if( (*i)->GetServer( ) == m_Server)
							{
								isAdmin = (*i)->IsAdmin( suser);
								break;
							}
						}
						if (!isAdmin)
						{
							SendAllChat( tr("lang_1155", "$USER$", suser, "$SERVER$", m_Server) );  // suser+" is not an admin on "+m_Server
							return HideCommand;
						}
						if (scmd == 1)
							access = CMDAccessAll();
						else
							access = 0;
						m_GHost->m_DB->AdminSetAccess(m_Server, suser, access);
						for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
						{
							if( (*i)->GetServer( ) == m_Server)
							{
								(*i)->UpdateAccess( suser, access);
								break;
							}
						}
						for (unsigned int i=0; i<=m_GHost->m_Commands.size()-1; i++)
						{
							cmd = m_GHost->Commands(i);
							if (access & Mask)
								acc = "1";
							else
								acc = "0";

							bool allowedtoshow = false;
							if (acc =="1" && m_GHost->CommandAllowedToShow(cmd))
							{
								if (saccess.length()>0)
									saccess +=", ";
								saccess +=cmd;
							}
							Mask = Mask * 2;
						}
						SendAllChat( tr("lang_1155", "$USER$", suser, "$ACCESS$", saccess) ); // suser+" can: "+saccess
						return HideCommand;
					}
					if (scmd>=m_GHost->m_Commands.size())
					{
						SendAllChat( tr("lang_1156", UTIL_ToString(m_GHost->m_Commands.size()-1) )); // "cmd must be as high as "+UTIL_ToString(m_GHost->m_Commands.size()-1)
						return HideCommand;
					}
					if (sacc!=0 && sacc!=1)
					{
						SendAllChat( tr("lang_1157")); // "acc must be 0 or 1"
						return HideCommand;
					}
				}
				else
					showonly = true;

				isAdmin = false;
				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
				{
					if( (*i)->GetServer( ) == player->GetSpoofedRealm( ) && (*i)->IsAdmin( suser ) )
					{
						isAdmin = true;
						access = (*i)->LastAccess();
						break;
					}
				}

				if (!isAdmin)
					SendAllChat( tr("lang_1155", "$USER$", suser, "$SERVER$", m_Server) ); // suser+" is not an admin on "+m_Server
				else
					if (showonly)
						// show currently accessable commands
					{
						for (unsigned int i=0; i<=m_GHost->m_Commands.size()-1; i++)
						{
							cmd = m_GHost->Commands(i);
							if (access & Mask)
								acc = "1";
							else
								acc = "0";
							if (acc =="1" && m_GHost->CommandAllowedToShow(cmd))
							{
								if (saccess.length()>0)
									saccess +=", ";
								saccess +=cmd;
							}
							Mask = Mask * 2;
						}
						SendAllChat( tr("lang_1155", "$USER$", suser, "$ACCESS$", saccess) ); // suser+" can: "+saccess
					} else
						// set access
					{
						Mask = 1;
						if (scmd != 0)
							for (unsigned int k=1;k<=scmd;k++)
								Mask = Mask * 2;
						if (Mask > access)
							son = 0;
						else
							son = access & Mask;
						if (son==sacc)
						{
							if (sacc == 0)
							{
								SendAllChat(tr("lang_1158", "$USER$", suser, "$CMD$", m_GHost->m_Commands[scmd])); // "Admin "+suser+ " already doesn't have access to "+m_GHost->m_Commands[scmd]
							}
							else
							{
								SendAllChat(tr("lang_1159", "$USER$", suser, "$CMD$", m_GHost->m_Commands[scmd])); // "Admin "+suser+ " already has access to "+m_GHost->m_Commands[scmd]
							}
							return HideCommand;
						}
						if (sacc == 1)
							access+= Mask;
						else
							access -= Mask;
						m_GHost->m_DB->AdminSetAccess(m_Server, suser, access);
						for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
						{
							if( (*i)->GetServer( ) == m_Server)
							{
								(*i)->UpdateAccess( suser, access);
								break;
							}
						}
						Mask = 1;
						for (unsigned int i=0; i<=m_GHost->m_Commands.size()-1; i++)
						{
							cmd = m_GHost->Commands(i);
							if (access & Mask)
								acc = "1";
							else
								acc = "0";
							if (acc =="1" && m_GHost->CommandAllowedToShow(cmd))
							{
								if (saccess.length()>0)
									saccess +=", ";
								saccess +=cmd;
							}
							Mask = Mask * 2;
						}
						SendAllChat( tr("lang_1155", "$USER$", suser, "$ACCESS$", saccess)); // user+" can: "+saccess
					}
			}

			//
			// !N
			// !NOTE
			//

			else if( ( Command == "n" || Command == "note" ) && !Payload.empty( ) && !m_GHost->m_BNETs.empty( ) )
			{
				string NName;
				string NNote;
				stringstream SS;
				SS << Payload;
				SS >> NName;
				CGamePlayer *LastMatch = NULL;

				uint32_t Matches = GetPlayerFromNamePartial( NName , &LastMatch );
				string srv = GetCreatorServer();

				if( Matches == 1 )
					NName = LastMatch->GetName();

				if( !SS.eof( ) )
				{
					getline( SS, NNote );
					string :: size_type Start = NNote.find_first_not_of( " " );

					if( Start != string :: npos )
						NNote = NNote.substr( Start );
				}

				bool noted = false;
				string note = Note(NName);
				noted = IsNoted( NName);

				if (NNote.empty())
				{
					if (noted)
						SendChat(player->GetPID(), tr("lang_1002", "$USER$", NName, "$NOTE$", note)); // PlayerIsNoted
					else
						SendChat(player->GetPID(), tr("lang_1003", NName)); // PlayerIsNotNoted
				} else
				{
					if (noted)
					{
						SendChat(player->GetPID(), tr("lang_1005", NName)); // ChangedPlayerNote
						m_GHost->m_Callables.push_back(m_GHost->m_DB->ThreadedNoteUpdate(m_Server, NName, NNote));
						AddNote(NName, NNote);
					}
					else
					{
						SendChat(player->GetPID(), tr("lang_1004", NName)); // AddedPlayerToNoteList
						m_GHost->m_Callables.push_back(m_GHost->m_DB->ThreadedNoteAdd(m_Server, NName, NNote));
						AddNote(NName, NNote);
					}
				}
			}

			//
			// !ND
			// !NR
			// !NOTEDEL
			// !NOTEREM
			//

			else if( ( Command == "nd" || Command == "nr" || Command == "notedel" || Command == "noterem" ) && !Payload.empty( ) && !m_GHost->m_BNETs.empty( ) )
			{
				CGamePlayer *LastMatch = NULL;

				uint32_t Matches = GetPlayerFromNamePartial( Payload , &LastMatch );
				string srv = GetCreatorServer();
				string nam = string();

				if( Matches == 0 )
					nam = Payload;
				else if( Matches == 1 )
					nam = LastMatch->GetName();
				else return HideCommand;

				bool noted = false;
				noted = IsNoted( nam);
				if (!noted)
					SendChat(player->GetPID(), tr("lang_1003", nam)); // PlayerIsNotNoted
				else
				{
					SendChat(player->GetPID(), tr("lang_1006", nam)); // RemovedPlayerFromNoteList
					m_GHost->m_Callables.push_back(m_GHost->m_DB->ThreadedNoteRemove(m_Server, nam));
				}
			}

			//
			// !SL
			//

			else if( ( Command == "sl" ) && !Payload.empty( ) && !m_GHost->m_BNETs.empty( ) )
			{

				CGamePlayer *LastMatch = NULL;

				uint32_t Matches = GetPlayerFromNamePartial( Payload , &LastMatch );
				string srv = GetCreatorServer();
				string nam = string();

				if( Matches == 0 )
					nam = Payload;
				else if( Matches == 1 )
					nam = LastMatch->GetName();
				else return HideCommand;

				bool safe = IsSafe(nam);
				string v = Voucher(nam);
				if (safe)
					SendAllChat(tr("lang_0991", "$USER$", nam, "$VOUCHER$", v)); // PlayerIsInTheSafeList
				else
					SendAllChat(tr("lang_0990", nam)); // PlayerIsNotInTheSafeList
			}

			//
			// !SLADD
			// !SLA
			//

			else if( ( Command == "sladd" || Command == "sla" ) && !Payload.empty( ) && !m_GHost->m_BNETs.empty( ) )
			{
				if (!RootAdminCheck)
				{
					SendChat(player->GetPID(),(tr("lang_0005")));
					return HideCommand;
				}


				CGamePlayer *LastMatch = NULL;

				uint32_t Matches = GetPlayerFromNamePartial( Payload , &LastMatch );
				string srv = GetCreatorServer();
				string nam = string();

				if( Matches == 0 )
					nam = Payload;
				else if( Matches == 1 )
					nam = LastMatch->GetName();
				else return HideCommand;

				bool safe = false;
				string v = string();
				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
				{
					if( (*i)->GetServer( ) == m_Server)
					{
						safe = (*i)->IsSafe(nam);
						v = (*i)->Voucher(nam);
						break;
					}
				}
				if (safe)
					SendAllChat(tr("lang_0991", "$USER$", nam, "$VOUCHER$", v)); // PlayerIsInTheSafeList
				else
				{
					m_PairedSafeAdds.push_back( PairedSafeAdd( User, m_GHost->m_DB->ThreadedSafeAdd( m_Server, nam, User ) ) );
					if (Matches <= 1)
					AddToReserved(nam, 255);
				}
			}

			//
			// !SLDEL
			// !SLD
			// !SLR
			//

			else if( ( Command == "sld" || Command == "slr" || Command == "sldel" ) && !Payload.empty( ) && !m_GHost->m_BNETs.empty( ) )
			{
				if (!RootAdminCheck)
				{
					SendChat(player->GetPID(),(tr("lang_0005")));
					return HideCommand;
				}


				CGamePlayer *LastMatch = NULL;

				uint32_t Matches = GetPlayerFromNamePartial( Payload , &LastMatch );
				string srv = GetCreatorServer();
				string nam = string();

				if( Matches == 0 )
					nam = Payload;
				else if( Matches == 1 )
					nam = LastMatch->GetName();
				else return HideCommand;

				bool safe = false;
				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
				{
					if( (*i)->GetServer( ) == m_Server)
					{
						safe = (*i)->IsSafe(nam);
						break;
					}
				}
				if (!safe)
					SendAllChat(tr("lang_0990", nam)); // PlayerIsNotInTheSafeList
				else
				{
					m_PairedSafeRemoves.push_back( PairedSafeRemove( User, m_GHost->m_DB->ThreadedSafeRemove( m_Server, nam ) ) );
					if (Matches == 1)
					LastMatch->SetReserved(false);
				}
			}

			//
			// !DELBAN
			// !UNBAN
			// !DB
			// !UB
			//

			else if( ( Command == "delban" || Command == "unban" || Command == "db" || Command == "ub" ) && !Payload.empty( ) )
			{
				if (!CMDCheck(CMD_delban, AdminAccess) || !RootAdminCheck )
				{
					SendChat(player->GetPID(),(tr("lang_0005")));
					return HideCommand;
				}

				string PlayerName;
				CGamePlayer *LastMatch = NULL;
				uint32_t Matches = GetPlayerFromNamePartial( Payload, &LastMatch );
				string admin = string();

				if (m_GHost->m_AdminsLimitedUnban && !RootAdminCheck)
					admin = player->GetName();

				if (Matches == 0)
					PlayerName = Payload;
				else
					PlayerName = LastMatch->GetName();

				bool banned = false;
				string banadmin = string();
				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
				{
					if ((*i)->GetServer()==m_Server)
					{
						CDBBan *Ban = (*i)->IsBannedName( PlayerName );

						if( Ban )
						{
							banned = true;
							banadmin = Ban->GetAdmin();
						}
					}
				}

				bool BannedByRoot = false;
				if (banned && IsRootAdmin(banadmin))
					BannedByRoot = true;

				if (m_GHost->m_AdminsCantUnbanRootadminBans && BannedByRoot && !RootAdminCheck)
					admin = player->GetName();

				if (!banned)
				{
					SendChat(player->GetPID(), tr("lang_0012", "$SERVER$", m_Server, "$VICTIM$", PlayerName)); // UserIsNotBanned
					return HideCommand;
				}

				if( Matches == 0 )
				{
					if (!admin.empty())
						m_PairedBanRemoves.push_back( PairedBanRemove( User, m_GHost->m_DB->ThreadedBanRemove( string(), Payload, PlayerName, 0 ) ) );
					else
						m_PairedBanRemoves.push_back( PairedBanRemove( User, m_GHost->m_DB->ThreadedBanRemove( Payload, 0 ) ) );
				}
//					SendChat(player->GetPID(), "No match found" );
				else if( Matches == 1 )
				{
					if (!admin.empty())
						m_PairedBanRemoves.push_back( PairedBanRemove( User, m_GHost->m_DB->ThreadedBanRemove( string(), LastMatch->GetName(), User, 0 ) ) );
					else
						m_PairedBanRemoves.push_back( PairedBanRemove( User, m_GHost->m_DB->ThreadedBanRemove( LastMatch->GetName(), 0 ) ) );
				}
				else
					SendChat( player->GetPID(), tr("lang_1139")); // Can't unban. More than one match found
			}

			//
			// !ADDBAN
			// !BAN
			// !B
			//

			//if(!m_GHost->m_ReplaceBanWithWarn)
			else if(!m_GHost->m_ReplaceBanWithWarn && ( Command == "addban" || Command == "ban" || Command == "b" ) && !Payload.empty( ) && !m_GHost->m_BNETs.empty( ) )
			{
				if (!CMDCheck(CMD_ban, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}

				// extract the victim and the reason
				// e.g. "Varlock leaver after dying" -> victim: "Varlock", reason: "leaver after dying"

				string Victim;
				string Reason;
				stringstream SS;
				SS << Payload;
				SS >> Victim;

				if( !SS.eof( ) )
				{
					getline( SS, Reason );
					string :: size_type Start = Reason.find_first_not_of( " " );

					if( Start != string :: npos )
						Reason = Reason.substr( Start );
				}

				if( m_GameLoaded )
				{
					string VictimLower = Victim;
					transform( VictimLower.begin( ), VictimLower.end( ), VictimLower.begin( ), (int(*)(int))tolower );
					uint32_t Matches = 0;
					CDBBan *LastMatch = NULL;

					// try to match each player with the passed string (e.g. "Varlock" would be matched with "lock")
					// we use the m_DBBans vector for this in case the player already left and thus isn't in the m_Players vector anymore

					for( vector<CDBBan *> :: iterator i = m_DBBans.begin( ); i != m_DBBans.end( ); i++ )
					{
						string TestName = (*i)->GetName( );
						transform( TestName.begin( ), TestName.end( ), TestName.begin( ), (int(*)(int))tolower );

						if( TestName.find( VictimLower ) != string :: npos )
						{
							Matches++;
							LastMatch = *i;
						}
					}

					if( Matches == 0 )
						SendAllChat( tr("lang_0051", Victim ) ); // UnableToBanNoMatchesFound
					else if( Matches == 1 )
					{
						bool isAdmin = IsOwner(LastMatch->GetName());
						for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
						{
							if( (*j)->IsAdmin(LastMatch->GetName() ) || (*j)->IsRootAdmin( LastMatch->GetName() ) )
							{
								isAdmin = true;
								break;
							}
						}

						if (isAdmin)
						{
							SendChat( player->GetPID(), tr("lang_1067")); // You can't ban an admin!"
							return HideCommand;
						}

						if (IsSafe(LastMatch->GetName()) && m_GHost->m_SafelistedBanImmunity)
						{
							SendChat( player->GetPID(), tr("lang_1068")); // You can't ban a safelisted player!
							return HideCommand;
						}

						bool isBanned = false;
						for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
						{
							if( (*i)->GetServer( ) == GetCreatorServer( ) )
								if ((*i)->IsBannedName(LastMatch->GetName( )))
									isBanned = true;

						}

						if (isBanned)
						{
							SendChat( player->GetPID(), tr("lang_0006", "$SERVER$", m_Server, "$VICTIM$", LastMatch->GetName() )); // UserIsAlreadyBanned
							return HideCommand;
						}

						Reason = CustomReason(Reason, LastMatch->GetName());

						if (m_GHost->m_ReplaceBanWithWarn)
						{
							return HideCommand;
						}

						uint32_t BanTime = m_GHost->m_BanTime;

						if(!(LastMatch->GetServer().empty()))
							m_PairedBanAdds.push_back( PairedBanAdd( User, m_GHost->m_DB->ThreadedBanAdd( LastMatch->GetServer(), LastMatch->GetName( ), LastMatch->GetIP( ), m_GameName, User, Reason, BanTime, 0 ) ) );
						else
							m_PairedBanAdds.push_back( PairedBanAdd( User, m_GHost->m_DB->ThreadedBanAdd( m_Server, LastMatch->GetName( ), "", m_GameName, User, Reason, BanTime, 0 ) ) );

						uint32_t GameNr = GetGameNr();

						CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + LastMatch->GetName( ) + "] was banned by player [" + User + "]" );

                        string sBan;
                        if (BanTime)
                            sBan = tr("lang_0519", "$SERVER$", LastMatch->GetServer(),
                                                                             "$VICTIM$", LastMatch->GetName( ),
                                                                             "$USER$",  User,
                                                                             "$BANDAYTIME$", UTIL_ToString(BanTime));
                        else

                            sBan = tr("lang_0052", "$SERVER$", LastMatch->GetServer(),
                                                                             "$VICTIM$", LastMatch->GetName( ),
                                                                             "$USER$",  User);

						string sBReason = sBan + ", "+Reason;

						if (Reason.empty())
						{
							SendAllChat( sBan );
						} else
						{
							if (sBReason.length()<220 && !m_GHost->m_TwoLinesBanAnnouncement)
								SendAllChat( sBReason );
							else
							{
								SendAllChat( sBan );
								SendAllChat( tr("lang_1027", Reason));
							}
						}
						if (m_GHost->m_NotifyBannedPlayers)
						{
							sBReason = tr("lang_1142"); // You have been banned
							if (!Reason.empty())
								sBReason = sBReason+", "+Reason;
							for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
							{
								if( (*i)->GetServer( ) == GetCreatorServer( ) )
									(*i)->QueueChatCommand( sBReason, LastMatch->GetName(), true );
							}
						}
					}
					else
						SendAllChat( tr("lang_0053", Victim ) ); // UnableToBanFoundMoreThanOneMatch
				}
				else
				{
					CGamePlayer *LastMatch = NULL;
					uint32_t Matches = GetPlayerFromNamePartial( Victim, &LastMatch );

//					if( Matches == 0 )
//						SendAllChat( m_GHost->m_Language->UnableToBanNoMatchesFound( Victim ) );
					if( Matches <= 1 )
					{
						string BanPlayer = Victim;
						if (Matches == 1)
							BanPlayer = LastMatch->GetName();
						bool isAdmin = IsOwner(BanPlayer);
						for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
						{
							if((*j)->IsAdmin(BanPlayer ) || (*j)->IsRootAdmin( BanPlayer ) )
							{
								isAdmin = true;
								break;
							}
						}

						if (isAdmin)
						{
							SendChat( player->GetPID(), tr("lang_1067")); // "You can't ban an admin!"
							return HideCommand;
						}

						if (IsSafe(BanPlayer) && m_GHost->m_SafelistedBanImmunity)
						{
							SendChat( player->GetPID(), tr("lang_1068")); // "You can't ban a safelisted player!"
							return HideCommand;
						}

						uint32_t BanTime = m_GHost->m_BanTime;

						if(Matches == 1 && !(LastMatch->GetJoinedRealm().empty()))
							m_PairedBanAdds.push_back( PairedBanAdd( User, m_GHost->m_DB->ThreadedBanAdd( LastMatch->GetJoinedRealm( ), BanPlayer, LastMatch->GetExternalIPString( ), m_GameName, User, Reason, BanTime, 0 ) ) );
						else
							m_PairedBanAdds.push_back( PairedBanAdd( User, m_GHost->m_DB->ThreadedBanAdd( m_Server, BanPlayer, "", m_GameName, User, Reason, BanTime, 0 ) ) );

						CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + Victim + "] was banned by player [" + User + "]" );


						string sBan = "";

						if (Matches == 1)
						{
							if (BanTime)
								sBan = tr("lang_0519", "$SERVER$", GetCreatorServer(),
									                                             "$VICTIM$", BanPlayer,
										                                         "$USER$",  User,
											                                     "$BANDAYTIME$", UTIL_ToString(BanTime));
							else
	                            sBan = tr("lang_0052", "$SERVER$", GetCreatorServer(),
																				 "$VICTIM$", BanPlayer,
																				 "$USER$",  User);
						}
                        else
							if (BanTime)
								sBan = tr("lang_0519", "$SERVER$", GetCreatorServer(),
																				 "$VICTIM$", BanPlayer,
																				 "$USER$",  User,
																				 "$BANDAYTIME$", UTIL_ToString(BanTime));
							else
	                            sBan = tr("lang_0052", "$SERVER$", GetCreatorServer(),
																				 "$VICTIM$", BanPlayer,
																				 "$USER$",  User);

						string sBReason = sBan + ", "+Reason;

						if (Reason.empty())
						{
							SendAllChat( sBan );
						} else
						{
							if (sBReason.length()<220 && !m_GHost->m_TwoLinesBanAnnouncement)
								SendAllChat( sBReason );
							else
							{
								SendAllChat( sBan);
								SendAllChat( tr("lang_1027", Reason) );
							}
						}
						if (m_GHost->m_NotifyBannedPlayers)
						{
							sBReason = tr("lang_1142"); // You have been banned
							if (!Reason.empty())
								sBReason = sBReason+", "+Reason;
							for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
							{
								if( (*i)->GetServer( ) == GetCreatorServer( ) )
									(*i)->QueueChatCommand( sBReason, BanPlayer, true );
							}
						}
					}
					else
                        SendAllChat( tr("lang_0053", Victim ) ); // UnableToBanFoundMoreThanOneMatch
				}
			}

			//
			// !TEMPBAN
			// !TBAN
			// !TB
			//

			else if( ( Command == "tempban" || Command == "tban" || Command == "tb" ) && !Payload.empty( ) && !m_GHost->m_BNETs.empty( ) )
			{
				if (!CMDCheck(CMD_ban, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}

				// extract the victim and the reason
				// e.g. "Varlock leaver after dying" -> victim: "Varlock", reason: "leaver after dying"

				string Victim;
				string Reason;
				uint32_t BanTime = 0;
				bool BanInit = false;
				stringstream SS;
				SS << Payload;
				SS >> Victim;

				if( !SS.eof( ) )
				{
					getline( SS, Reason );
					string :: size_type Start = Reason.find_first_not_of( " " );

					if( Start != string :: npos )
					{
						Reason = Reason.substr( Start );

						string :: size_type BreakPoint = Reason.find_first_not_of( "0123456789" );

						if( BreakPoint != string :: npos )
						{
							BanInit = true;
							string Reas = Reason.substr(0, BreakPoint);
							BanTime = UTIL_ToUInt32( Reas );

							if( BreakPoint != string :: npos )
								Reason = Reason.substr(BreakPoint);

							Start = Reason.find_first_not_of( " " );
							if( Start != string :: npos )
								Reason = Reason.substr( Start );
						}
					}
				}

				if(!BanInit)
				{
					// failed to read the ban time, inform the player the syntax was incorrect
					SendAllChat( tr("lang_0511") ); // IncorrectCommandSyntax
					return HideCommand;
				}

				if( m_GameLoaded )
				{
					string VictimLower = Victim;
					transform( VictimLower.begin( ), VictimLower.end( ), VictimLower.begin( ), (int(*)(int))tolower );
					uint32_t Matches = 0;
					CDBBan *LastMatch = NULL;

					// try to match each player with the passed string (e.g. "Varlock" would be matched with "lock")
					// we use the m_DBBans vector for this in case the player already left and thus isn't in the m_Players vector anymore

					for( vector<CDBBan *> :: iterator i = m_DBBans.begin( ); i != m_DBBans.end( ); i++ )
					{
						string TestName = (*i)->GetName( );
						transform( TestName.begin( ), TestName.end( ), TestName.begin( ), (int(*)(int))tolower );

						if( TestName.find( VictimLower ) != string :: npos )
						{
							Matches++;
							LastMatch = *i;
						}
					}

					if( Matches == 0 )
						SendAllChat( tr("lang_0051", Victim ) ); // UnableToBanNoMatchesFound
					else if( Matches == 1 )
					{
						bool isAdmin = IsOwner(LastMatch->GetName());
						for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
						{
							if( (*j)->IsAdmin(LastMatch->GetName() ) || (*j)->IsRootAdmin( LastMatch->GetName() ) )
							{
								isAdmin = true;
								break;
							}
						}

						if (isAdmin)
						{
							SendChat( player->GetPID(), tr("lang_1067")); // "You can't ban an admin!"
							return HideCommand;
						}

						if (IsSafe(LastMatch->GetName()) && m_GHost->m_SafelistedBanImmunity)
						{
							SendChat( player->GetPID(), tr("lang_1068")); // "You can't ban a safelisted player!"
							return HideCommand;
						}

						Reason = CustomReason(Reason, LastMatch->GetName());

//						m_PairedBanAdds.push_back( PairedBanAdd( User, m_GHost->m_DB->ThreadedBanAdd( LastMatch->GetServer(), LastMatch->GetName( ), LastMatch->GetIP( ), m_GameName, User, Reason, BanTime, 0 ) ) );
						m_GHost->m_Callables.push_back(m_GHost->m_DB->ThreadedBanAdd( LastMatch->GetServer(), LastMatch->GetName( ), LastMatch->GetIP( ), m_GameName, User, Reason, BanTime, 0 ));
						for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
						{
							if ((*j)->GetServer() == m_Server)
							{
								string sDate = string();
								if (BanTime>0)
								{
									struct tm * timeinfo;
									char buffer [80];
									time_t Now = time( NULL );
									Now += 3600*24*BanTime;
									timeinfo = localtime( &Now );
									strftime (buffer,80,"%d-%m-%Y",timeinfo);
									sDate = buffer;
								}

								(*j)->AddBan(LastMatch->GetName(), LastMatch->GetIP(), LastMatch->GetGameName(), LastMatch->GetAdmin(), Reason, sDate);
							}
						}
						uint32_t GameNr = GetGameNr();

						CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + LastMatch->GetName( ) + "] was banned by player [" + User + "]" );

                        string sBan;
						
						if (BanTime)
							sBan = tr("lang_0519", "$SERVER$", LastMatch->GetServer(),
																			 "$VICTIM$", LastMatch->GetName( )+" ("+LastMatch->GetIP()+")",
																			 "$USER$",  User,
																			 "$BANDAYTIME$", UTIL_ToString(BanTime));
						else
                            sBan = tr("lang_0052", "$SERVER$", LastMatch->GetServer(),
																			 "$VICTIM$", LastMatch->GetName( )+" ("+LastMatch->GetIP()+")",
																			 "$USER$",  User);

						string sBReason = sBan + ", "+Reason;

						if (Reason.empty())
						{
							SendAllChat( sBan );
						} else
						{
							if (sBReason.length()<220 && !m_GHost->m_TwoLinesBanAnnouncement)
								SendAllChat( sBReason );
							else
							{
								SendAllChat( sBan);
								SendAllChat( tr("lang_1027", Reason));
							}
						}
						if (m_GHost->m_NotifyBannedPlayers)
						{
							sBReason = tr("lang_1142"); // "You have been banned";
							if (!Reason.empty())
								sBReason = sBReason+", "+Reason;
							for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
							{
								if( (*i)->GetServer( ) == GetCreatorServer( ) )
									(*i)->QueueChatCommand( sBReason, LastMatch->GetName(), true );
							}
						}
					}
					else
						SendAllChat( tr("lang_0053", Victim )); //UnableToBanFoundMoreThanOneMatch( Victim ) );
				}
				else
				{
					CGamePlayer *LastMatch = NULL;
					uint32_t Matches = GetPlayerFromNamePartial( Victim, &LastMatch );

//					if( Matches == 0 )
//						SendAllChat( m_GHost->m_Language->UnableToBanNoMatchesFound( Victim ) );
					if( Matches <= 1 )
					{
						string BanPlayer = Victim;
						if (Matches == 1)
							BanPlayer = LastMatch->GetName();
						bool isAdmin = IsOwner(BanPlayer);
						for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
						{
							if((*j)->IsAdmin(BanPlayer ) || (*j)->IsRootAdmin( BanPlayer ) )
							{
								isAdmin = true;
								break;
							}
						}

						if (isAdmin)
						{
							SendChat( player->GetPID(), tr("lang_1067")); // "You can't ban an admin!"
							return HideCommand;
						}

						if (IsSafe(BanPlayer) && m_GHost->m_SafelistedBanImmunity)
						{
							SendChat( player->GetPID(), tr("lang_1068")); // "You can't ban a safelisted player!"
							return HideCommand;
						}

						if (Matches == 1)
						{
							m_GHost->m_Callables.push_back(m_GHost->m_DB->ThreadedBanAdd( LastMatch->GetJoinedRealm( ), LastMatch->GetName( ), LastMatch->GetExternalIPString( ), m_GameName, User, Reason, BanTime, 0 ));
							for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
							{
								if ((*j)->GetServer() == m_Server)
								{
									string sDate = string();
									if (BanTime>0)
									{
										struct tm * timeinfo;
										char buffer [80];
										time_t Now = time( NULL );
										Now += 3600*24*BanTime;
										timeinfo = localtime( &Now );
										strftime (buffer,80,"%d-%m-%Y",timeinfo);
										sDate = buffer;
									}

									(*j)->AddBan(LastMatch->GetName(), LastMatch->GetExternalIPString(), m_GameName, User, Reason, sDate);
								}
							}
						}
//							m_PairedBanAdds.push_back( PairedBanAdd( User, m_GHost->m_DB->ThreadedBanAdd( LastMatch->GetJoinedRealm( ), LastMatch->GetName( ), LastMatch->GetExternalIPString( ), m_GameName, User, Reason, BanTime, 0 ) ) );
						else
						{
							m_GHost->m_Callables.push_back(m_GHost->m_DB->ThreadedBanAdd( m_Server, Victim, "", m_GameName, User, Reason, BanTime, 0 ));
							for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
							{
								if ((*j)->GetServer() == m_Server)
								{
									string sDate = string();
									if (BanTime>0)
									{
										struct tm * timeinfo;
										char buffer [80];
										time_t Now = time( NULL );
										Now += 3600*24*BanTime;
										timeinfo = localtime( &Now );
										strftime (buffer,80,"%d-%m-%Y",timeinfo);
										sDate = buffer;
									}

									(*j)->AddBan(Victim, string(), m_GameName, User, Reason, sDate);
								}
							}
						}
//							m_PairedBanAdds.push_back( PairedBanAdd( User, m_GHost->m_DB->ThreadedBanAdd( m_Server, Victim, "", m_GameName, User, Reason, BanTime, 0 ) ) );
//						m_GHost->m_DB->BanAdd( LastMatch->GetJoinedRealm( ), LastMatch->GetName( ), LastMatch->GetExternalIPString( ), m_GameName, User, Reason );

						CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + Victim + "] was banned by player [" + User + "]" );

						string sBan = "";

						if (Matches == 1)
						{
							if (BanTime)
								sBan = tr("lang_0519", "$SERVER$", GetCreatorServer(),
									                                             "$VICTIM$", BanPlayer,
										                                         "$USER$",  User,
											                                     "$BANDAYTIME$", UTIL_ToString(BanTime));
							else
	                            sBan = tr("lang_0052", "$SERVER$", GetCreatorServer(),
																				 "$VICTIM$", BanPlayer,
																				 "$USER$",  User);
						}
                        else
							if (BanTime)
								sBan = tr("lang_0519", "$SERVER$", GetCreatorServer(),
																				 "$VICTIM$", BanPlayer,
																				 "$USER$",  User,
																				 "$BANDAYTIME$", UTIL_ToString(BanTime));
							else
	                            sBan = tr("lang_0052", "$SERVER$", GetCreatorServer(),
																				 "$VICTIM$", BanPlayer,
																				 "$USER$",  User);


						string sBReason = sBan + ", "+Reason;

						if (Reason.empty())
						{
							SendAllChat( sBan );
						} else
						{
							if (sBReason.length()<220 && !m_GHost->m_TwoLinesBanAnnouncement)
								SendAllChat( sBReason );
							else
							{
								SendAllChat( sBan );
								SendAllChat( tr("lang_1027", Reason));
							}
						}
						if (m_GHost->m_NotifyBannedPlayers)
						{
							sBReason = tr("lang_1142"); // "You have been banned";
							if (!Reason.empty())
								sBReason = sBReason+", "+Reason;
							for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
							{
								if( (*i)->GetServer( ) == GetCreatorServer( ) )
									(*i)->QueueChatCommand( sBReason, BanPlayer, true );
							}
						}
					}
					else
                        SendAllChat( tr("lang_0053", Victim )); //UnableToBanFoundMoreThanOneMatch( Victim ) );
				}
			}

			//
			// !ADDWARN
			// !WARN
			// !W
			//

			else if( ( (Command == "addwarn" || Command == "warn" || Command == "w") || (m_GHost->m_ReplaceBanWithWarn && (Command == "ban" || Command == "addban" || Command == "b") )) && !Payload.empty( ) && !m_GHost->m_BNETs.empty( ) )
			{
				if (!CMDCheck(CMD_ban, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}

				// extract the victim and the reason
				// e.g. "Varlock leaver after dying" -> victim: "Varlock", reason: "leaver after dying"

				string Victim;
				string Reason;
				stringstream SS;
				SS << Payload;
				SS >> Victim;

				if( !SS.eof( ) )
				{
					getline( SS, Reason );
					string :: size_type Start = Reason.find_first_not_of( " " );

					if( Start != string :: npos )
						Reason = Reason.substr( Start );
				}

				if( m_GameLoaded )
				{
					string VictimLower = Victim;
					transform( VictimLower.begin( ), VictimLower.end( ), VictimLower.begin( ), (int(*)(int))tolower );
					uint32_t Matches = 0;
					CDBBan *LastMatch = NULL;

					// try to match each player with the passed string (e.g. "Varlock" would be matched with "lock")
					// we use the m_DBBans vector for this in case the player already left and thus isn't in the m_Players vector anymore

					for( vector<CDBBan *> :: iterator i = m_DBBans.begin( ); i != m_DBBans.end( ); i++ )
					{
						string TestName = (*i)->GetName( );
						transform( TestName.begin( ), TestName.end( ), TestName.begin( ), (int(*)(int))tolower );

						if( TestName.find( VictimLower ) != string :: npos )
						{
							Matches++;
							LastMatch = *i;
						}
					}

					if( Matches == 0 )
						SendAllChat( tr("lang_0504", Victim ) ); // UnableToWarnNoMatchesFound
					else if( Matches == 1 )
					{
						bool isAdmin = IsOwner(LastMatch->GetName());
						for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
						{
							if( (*j)->IsAdmin(LastMatch->GetName() ) || (*j)->IsRootAdmin( LastMatch->GetName() ) )
							{
								isAdmin = true;
								break;
							}
						}

						if (isAdmin && !RootAdminCheck)
						{
							SendChat( player->GetPID(), tr("lang_1069")); // "You can't warn an admin!"
							return HideCommand;
						}

						if ((IsSafe(LastMatch->GetName()) && m_GHost->m_SafelistedBanImmunity) && !RootAdminCheck)
						{
							SendChat( player->GetPID(), tr("lang_1070")); // You can't warn a safelisted player!"
							return HideCommand;
						}

						Reason = CustomReason(Reason, LastMatch->GetName() );

						WarnPlayer(LastMatch, Reason, User);
					}
					else
						SendAllChat( tr("lang_0506", Victim ) ); // UnableToWarnFoundMoreThanOneMatch
				}
				else
				{
					WarnPlayer( Victim, Reason, User);
				}
			}

			//
			// !DELWARN
			// !UNWARN
			// !DW
			// !UW
			//

			else if( ( Command == "delwarn" || Command == "unwarn" || Command =="dw" || Command == "uw" ) && !Payload.empty( ) )
			{
				if (!CMDCheck(CMD_delban, AdminAccess))
				{
					SendChat(player->GetPID(),(tr("lang_0005")));
					return HideCommand;
				}

				CGamePlayer *LastMatch = NULL;
				uint32_t Matches = GetPlayerFromNamePartial( Payload, &LastMatch );

				if( Matches == 0 )
					SendChat(player->GetPID(), tr("lang_1131") ); // no match found!
				else if( Matches == 1 )
				{
					if( m_GHost->m_DB->BanRemove( LastMatch->GetName(), 1 ) )
						SendAllChat( tr("lang_0507", LastMatch->GetName() )); // UnwarnedUser
					else
						SendChat( player->GetPID(), tr("lang_0508", LastMatch->GetName() )); // ErrorUnwarningUser
				}
				else
					SendChat( player->GetPID(), tr("lang_1140")); // "Can't unwarn. More than one match found"
			}

			//
			// !ANNOUNCE
			//

			else if( ( Command == "announce" && !m_CountDownStarted ) )
			{
				if( Payload.empty( ) || Payload == "off" )
				{
					SendAllChat( tr("lang_0133") ); // AnnounceMessageDisabled
					SetAnnounce( 0, string( ) );
				}
				else
				{
					// extract the interval and the message
					// e.g. "30 hello everyone" -> interval: "30", message: "hello everyone"

					uint32_t Interval;
					string Message;
					stringstream SS;
					SS << Payload;
					SS >> Interval;

					if( SS.fail( ) || Interval == 0 )
						CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #1 to announce command" );
					else
					{
						if( SS.eof( ) )
							CONSOLE_Print( "[GAME: " + m_GameName + "] missing input #2 to announce command" );
						else
						{
							getline( SS, Message );
							string :: size_type Start = Message.find_first_not_of( " " );

							if( Start != string :: npos )
								Message = Message.substr( Start );

							SendAllChat( tr("lang_0132") ); // AnnounceMessageEnabled
							SetAnnounce( Interval, Message );
						}
					}
				}
			}

			//
			// !AUTOSAVE
			//

			else if( ( Command == "autosave" ) )
			{
				if( Payload == "on" )
				{
					SendAllChat( tr("lang_0142") ); // AutoSaveEnabled
					m_AutoSave = true;
				}
				else if( Payload == "off" )
				{
					SendAllChat( tr("lang_0143") ); // AutoSaveDisabled
					m_AutoSave = false;
				}
			}

			//
			// !AUTOSTART
			//

			else if( Command == "autostart" && !m_CountDownStarted )
			{
				if (m_GHost->m_onlyownerscanstart)
					if ((!IsOwner( User) && GetPlayerFromName(m_OwnerName, false)) && !RootAdminCheck )
					{
						SendChat( player->GetPID(),  tr("lang_1141") ); //  Only the owner can start the game.
						return HideCommand;
					}

				if( Payload.empty( ) || Payload == "off" )
				{
					SendAllChat( tr("lang_0130") ); // AutoStartDisabled
					m_AutoStartPlayers = 0;
				}
				else
				{
					uint32_t AutoStartPlayers = UTIL_ToUInt32( Payload );

					if( AutoStartPlayers != 0 )
					{
						SendAllChat( tr("lang_0131", UTIL_ToString( AutoStartPlayers ) ) ); // AutoStartEnabled
						m_AutoStartPlayers = AutoStartPlayers;
					}
				}
			}

			//
			// !BANS
			//

			else if( Command == "bans" )
			{
				if( Payload == "on" )
				{
					SendAllChat( tr("lang_1143") ); // "Bans enabled"
					m_Bans = true;
				}
				else if( Payload == "off" )
				{
					SendAllChat( tr("lang_1144") ); // "Bans disabled"
					m_Bans = false;
				}
			}

			//
			// !AUTOWARN
			//
			else if( Command == "autowarn" && !m_CountDownStarted )
			{
				if(m_GHost->m_AutoWarnEarlyLeavers != true)
				{
					SendAllChat( tr("lang_0516") ); // CannotAutoWarn
				}
				else
				{
					
					m_DoAutoWarns = !m_DoAutoWarns;

					if(m_DoAutoWarns)
					{
						SendAllChat( tr("lang_0517") ); // AutoWarnEnabled
					}
					else
					{
						SendAllChat( tr("lang_0518") ); // AutoWarnDisabled
					}
				}
			}

			//
			// !BANLAST
			// !BL
			//

			//if ()
			else if(!m_GHost->m_ReplaceBanWithWarn && ( Command == "banlast" || Command == "bl" || Command == "blast" ) && m_GameLoaded && !m_GHost->m_BNETs.empty( ) && m_DBBanLast )
			{
				if (!CMDCheck(CMD_ban, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}

				bool isAdmin = IsOwner(m_DBBanLast->GetName( ));
				for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
				{
					if( (*j)->IsAdmin(m_DBBanLast->GetName( ) ) || (*j)->IsRootAdmin( m_DBBanLast->GetName( ) ) )
					{
						isAdmin = true;
						break;
					}
				}

				if (isAdmin)
				{
					SendChat( player->GetPID(), tr("lang_1067") ); // You can't ban an admin!
					return HideCommand;
				}

				if (IsSafe(m_DBBanLast->GetName( )) && m_GHost->m_SafelistedBanImmunity)
				{
					SendChat( player->GetPID(), tr("lang_1068") ); // You can't ban a safelisted player!
					return HideCommand;
				}

				bool isBanned = false;
				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
				{
					if( (*i)->GetServer( ) == GetCreatorServer( ) )
						if ((*i)->IsBannedName(m_DBBanLast->GetName()))
					isBanned = true;
				}

				if (isBanned)
				{
					SendChat( player->GetPID(), tr("lang_0006", "$SERVER$", m_Server, "$VICTIM$", m_DBBanLast->GetName() )); // UserIsAlreadyBanned
					return HideCommand;
				}

				string Reason = CustomReason( Payload, m_DBBanLast->GetName() );

				uint32_t BanTime = m_GHost->m_BanLastTime;
				if(!(m_DBBanLast->GetServer().empty()))
					m_PairedBanAdds.push_back( PairedBanAdd( User, m_GHost->m_DB->ThreadedBanAdd( m_DBBanLast->GetServer( ), m_DBBanLast->GetName( ), m_DBBanLast->GetIP( ), m_GameName, User, Reason, BanTime, 0 ) ) );
				else
					m_PairedBanAdds.push_back( PairedBanAdd( User, m_GHost->m_DB->ThreadedBanAdd( m_Server, m_DBBanLast->GetName( ), "", m_GameName, User, Reason, BanTime, 0 ) ) );

				CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + m_DBBanLast->GetName( ) + "] was banned by player [" + User + "]" );

                string sBan = "";
				
				if (BanTime)
					sBan = tr("lang_0519", "$SERVER$", m_DBBanLast->GetServer(),
                                                                     "$VICTIM$", m_DBBanLast->GetName( ),
                                                                     "$USER$",  User,
                                                                     "$BANDAYTIME$", UTIL_ToString(BanTime));
				else

                    sBan = tr("lang_0052", "$SERVER$", m_DBBanLast->GetServer(),
																	 "$VICTIM$",  m_DBBanLast->GetName( ),
																	 "$USER$",  User);

				string sBReason = sBan + ", "+Reason;

				if (Reason.empty())
				{
					SendAllChat( sBan );
				} else
				{
					if (sBReason.length()<220 && !m_GHost->m_TwoLinesBanAnnouncement)
						SendAllChat( sBReason );
					else
					{
						SendAllChat( sBan);
						SendAllChat( tr("lang_1027", Reason)); // "Ban reason: " + Reason
					}
				}
				if (m_GHost->m_NotifyBannedPlayers)
				{
					sBReason =  tr("lang_1142"); // "You have been banned"
					if (!Reason.empty())
						sBReason = sBReason+", "+Reason;
					for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
					{
						if( (*i)->GetServer( ) == GetCreatorServer( ) )
							(*i)->QueueChatCommand( sBReason, m_DBBanLast->GetName(), true );
					}
				}
			}

			//
			// !TBANLAST
			// !TBLAST
			// !TBL
			//

			else if( ( Command == "tbanlast" || Command=="tblast" ||Command == "tbl" ) && m_GameLoaded && !m_GHost->m_BNETs.empty( ) && m_DBBanLast )
			{

				if (!CMDCheck(CMD_ban, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}

				bool isAdmin = IsOwner(m_DBBanLast->GetName( ));
				for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
				{
					if( (*j)->IsAdmin(m_DBBanLast->GetName( ) ) || (*j)->IsRootAdmin( m_DBBanLast->GetName( ) ) )
					{
						isAdmin = true;
						break;
					}
				}

				if (isAdmin)
				{
					SendChat( player->GetPID(), tr("lang_1067")); //
					return HideCommand;
				}

				if (IsSafe(m_DBBanLast->GetName( )) && m_GHost->m_SafelistedBanImmunity)
				{
					SendChat( player->GetPID(), tr("lang_1068")); //
					return HideCommand;
				}

				bool isBanned = false;
				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
				{
					if( (*i)->GetServer( ) == GetCreatorServer( ) )
						if ((*i)->IsBannedName(m_DBBanLast->GetName()))
					isBanned = true;
				}

				if (isBanned)
				{
					SendChat( player->GetPID(), tr("lang_0006", "$SERVER$", m_Server, "$VICTIM$", m_DBBanLast->GetName() )); // UserIsAlreadyBanned
					return HideCommand;
				}

				string Reason = CustomReason( Payload, m_DBBanLast->GetName() );

				uint32_t BanTime = m_GHost->m_TBanLastTime;
				if(!(m_DBBanLast->GetServer().empty()))
					m_PairedBanAdds.push_back( PairedBanAdd( User, m_GHost->m_DB->ThreadedBanAdd( m_DBBanLast->GetServer( ), m_DBBanLast->GetName( ), m_DBBanLast->GetIP( ), m_GameName, User, Reason, BanTime, 0 ) ) );
				else
					m_PairedBanAdds.push_back( PairedBanAdd( User, m_GHost->m_DB->ThreadedBanAdd( m_Server, m_DBBanLast->GetName( ), "", m_GameName, User, Reason, BanTime, 0 ) ) );

				CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + m_DBBanLast->GetName( ) + "] was banned by player [" + User + "]" );

				string sBan = "";
				
				if (BanTime)
					sBan = tr("lang_0519", "$SERVER$", m_DBBanLast->GetServer(),
                                                                     "$VICTIM$", m_DBBanLast->GetName( ),
                                                                     "$USER$",  User,
                                                                     "$BANDAYTIME$", UTIL_ToString(BanTime));
				else

                    sBan = tr("lang_0052", "$SERVER$", m_DBBanLast->GetServer(),
																	 "$VICTIM$",  m_DBBanLast->GetName( ),
																	 "$USER$",  User);

                string sBReason = sBan + ", "+Reason;

				if (Reason.empty())
				{
					SendAllChat( sBan );
				} else
				{
					if (sBReason.length()<220 && !m_GHost->m_TwoLinesBanAnnouncement)
						SendAllChat( sBReason );
					else
					{
						SendAllChat( sBan);
						SendAllChat( tr("lang_1027", Reason)); // "Ban reason: " + Reason
					}
				}
				if (m_GHost->m_NotifyBannedPlayers)
				{
					sBReason =  tr("lang_1142"); // "You have been banned"
					if (!Reason.empty())
						sBReason = sBReason+", "+Reason;
					for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
					{
						if( (*i)->GetServer( ) == GetCreatorServer( ) )
							(*i)->QueueChatCommand( sBReason, m_DBBanLast->GetName(), true );
					}
				}
			}

			//
			// !WARNLAST
			// !WLAST
			// !WL
			//

			else if( ( ((Command == "warnlast" || Command=="wlast" ||Command == "wl") || (m_GHost->m_ReplaceBanWithWarn && (Command == "banlast" || Command == "blast" || Command == "bl"))) ) && m_GameLoaded && !m_GHost->m_BNETs.empty( ) && m_DBBanLast )
			{

				if (!CMDCheck(CMD_ban, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}

				bool isAdmin = IsOwner(m_DBBanLast->GetName( ));
				for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
				{
					if( (*j)->IsAdmin(m_DBBanLast->GetName( ) ) || (*j)->IsRootAdmin( m_DBBanLast->GetName( ) ) )
					{
						isAdmin = true;
						break;
					}
				}

				if (isAdmin && !RootAdminCheck)
				{
					SendChat( player->GetPID(), tr("lang_1069")); // "You can't warn an admin!"
					return HideCommand;
				}

				if ((IsSafe(m_DBBanLast->GetName( )) && m_GHost->m_SafelistedBanImmunity) && !RootAdminCheck)
				{
					SendChat( player->GetPID(), tr("lang_1070")); // "You can't warn a safelisted player!"
					return HideCommand;
				}

				string Reason = CustomReason( Payload, m_DBBanLast->GetName() );

				WarnPlayer(m_DBBanLast, Reason, User);
			}

			//
			// !CHECK
			//

			else if( Command == "check" )
			{
				if( !Payload.empty( ) )
				{
					CGamePlayer *LastMatch = NULL;
					uint32_t Matches = GetPlayerFromNamePartial( Payload, &LastMatch );

					bool isAdmin = false;
					if (Matches==1)
					for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
					{
						if( (*i)->GetServer( ) == LastMatch->GetSpoofedRealm())
						{
							isAdmin = (*i)->IsAdmin( LastMatch->GetName());
							break;
						}
					}

					if( Matches == 0 )
						SendAllChat( tr("lang_0121", Payload ) );  // UnableToCheckPlayerNoMatchesFound
					else if( Matches == 1 )
                        SendAllChat( tr("lang_0122", "$VICTIM$", LastMatch->GetName( ),
                                                                                "$PING$",  LastMatch->GetNumPings( ) > 0 ? UTIL_ToString( LastMatch->GetPing( m_GHost->m_LCPings ) ) + tr("lang_1017") /* ms */ : tr("lang_1018") /* N/A */,
                                                                                "$FROM$",  LastMatch->GetCountry(),
                                                                                "$ADMIN$", isAdmin || IsRootAdmin( LastMatch->GetName() ) ? tr("lang_1145") /* YES */: tr("lang_1146") /* NO */,
                                                                                "$OWNER$", IsOwner( LastMatch->GetName( ) ) ? tr("lang_1145") /* YES */ : tr("lang_1146") /* NO */,
                                                                                "$SPOOFED$", LastMatch->GetSpoofed( ) ? tr("lang_1145") /* YES */ : tr("lang_1146") /* NO */,
                                                                                "$SPOOFEDREALM$", LastMatch->GetSpoofedRealm( ).empty( ) ? tr("lang_1018") /* N/A */ : LastMatch->GetSpoofedRealm( )));

					else
						SendAllChat( tr("lang_0123", Payload ) ); // UnableToCheckPlayerFoundMoreThanOneMatch
				}
				else
				{
					bool isAdmin = false;
					for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
					{
						if( (*i)->GetServer( ) == m_Server)
						{
							isAdmin = (*i)->IsAdmin( User);
							break;
						}
					}


                    SendAllChat( tr("lang_0122", "$VICTIM$", User,
                                                                                "$PING$",  player->GetNumPings( ) > 0 ? UTIL_ToString( player->GetPing( m_GHost->m_LCPings ) ) + tr("lang_1017") /* ms */ : tr("lang_1018") /* N/A */,
                                                                                "$FROM$",  m_GHost->m_DBLocal->FromCheck( UTIL_ByteArrayToUInt32( player->GetExternalIP( ), true ) ),
                                                                                "$ADMIN$", isAdmin || IsRootAdmin( player->GetName() ) ? tr("lang_1145") /* YES */: tr("lang_1146") /* NO */,
                                                                                "$OWNER$", IsOwner( player->GetName( ) ) ? tr("lang_1145") /* YES */ : tr("lang_1146") /* NO */,
                                                                                "$SPOOFED$", player->GetSpoofed( ) ? tr("lang_1145") /* YES */ : tr("lang_1146") /* NO */,
                                                                                "$SPOOFEDREALM$", player->GetSpoofedRealm( ).empty( ) ? tr("lang_1018") /* N/A */ : player->GetSpoofedRealm( )));

				}
			}

			//
			// !CHECKBAN
			//

			else if( (Command == "checkban" || Command == "cb") && !Payload.empty( ) && !m_GHost->m_BNETs.empty( ) )
			{
				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
					m_PairedBanChecks.push_back( PairedBanCheck( User, m_GHost->m_DB->ThreadedBanCheck( (*i)->GetServer( ), Payload, string( ), 0 ) ) );
			}

			//
			// !CLEARHCL
			//

			else if( Command == "clearhcl" && !m_CountDownStarted )
			{
				if (m_GHost->m_onlyownerscanstart)
					if ((!IsOwner( User) && GetPlayerFromName(m_OwnerName, false)) && !RootAdminCheck )
					{
						SendChat( player->GetPID(), tr("lang_1147")); //  "Only the owner can change HCL."
						return HideCommand;
					}

				if (BluePlayer && AdminAccess==m_GHost->m_OwnerAccess)
					if (!m_GHost->m_BlueCanHCL)
						return HideCommand;
				m_HCLCommandString.clear( );
				SendAllChat( tr("lang_0187") ); // ClearingHCL
			}

			//
			// !CHECKWARNS
			// !CHECKWARN
			// !CW
			//

			else if( ( Command == "checkwarn" || Command == "checkwarns" || Command == "cw" ) && !Payload.empty( ) && !m_GHost->m_BNETs.empty( ) )
			{
				uint32_t WarnCount = 0;
				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
				{
					uint32_t WC = m_GHost->m_DB->BanCount( (*i)->GetServer( ), Payload, 1 );
					if (WC==0)
					{
						WC = m_GHost->m_DB->BanCount( (*i)->GetServer( ), Payload, 3 );
					}
					WarnCount += WC;

					if (WC > 0 )
					{
						string reasons = m_GHost->m_DB->WarnReasonsCheck( Payload, 1 );
						string message = tr("lang_0514", "$VICTIM$", Payload, "$WARNNUM$", UTIL_ToString(WarnCount) ); // UserWarnReasons

						message += reasons;
						if( message.length() > 220 )
							message = message.substr(220);

						SendAllChat( message );
					}
					else
					{
						SendAllChat( tr("lang_0513", Payload ) ); // UserIsNotWarned
					}
				}
			}

			//
			// !CLOSE (close slot)
			//

			else if( Command == "close" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded )
			{
				if (!CMDCheck(CMD_close, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}
				// close as many slots as specified, e.g. "5 10" closes slots 5 and 10

				stringstream SS;
				SS << Payload;

				while( !SS.eof( ) )
				{
					uint32_t SID;
					SS >> SID;

					if( SS.fail( ) )
					{
						CONSOLE_Print( "[GAME: " + m_GameName + "] bad input to close command" );
						break;
					}
					else
					{
						bool isAdmin = false;
						CGamePlayer *Player = GetPlayerFromSID( SID - 1 );
						if (Player)
						{
							if (IsAdmin(Player->GetName()) || IsRootAdmin(Player->GetName()))
								isAdmin = true;
						}
						if (isAdmin && !(IsOwner(User) || RootAdminCheck))
						{
							SendChat( player->GetPID(), tr("lang_1160")); // "You can't kick an admin!"
							return HideCommand;
						} else
							CloseSlot( (unsigned char)( SID - 1 ), true );
					}
				}
			}

			//
			// !CLOSEALL
			//

			else if( Command == "closeall" && !m_GameLoading && !m_GameLoaded )
			{
				if (!CMDCheck(CMD_close, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}
				CloseAllSlots( );
			}

			//
			// !COMP (computer slot)
			//

			else if( Command == "comp" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded && !m_SaveGame )
			{
				// extract the slot and the skill
				// e.g. "1 2" -> slot: "1", skill: "2"

				uint32_t Slot;
				uint32_t Skill = 1;
				stringstream SS;
				SS << Payload;
				SS >> Slot;

				if( SS.fail( ) )
					CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #1 to comp command" );
				else
				{
					if( !SS.eof( ) )
						SS >> Skill;

					if( SS.fail( ) )
						CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #2 to comp command" );
					else
						ComputerSlot( (unsigned char)( Slot - 1 ), (unsigned char)Skill, true );
				}
			}

			//
			// !COMPCOLOUR (computer colour change)
			//

			else if( Command == "compcolour" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded && !m_SaveGame )
			{
				// extract the slot and the colour
				// e.g. "1 2" -> slot: "1", colour: "2"

				uint32_t Slot;
				uint32_t Colour;
				stringstream SS;
				SS << Payload;
				SS >> Slot;

				if( SS.fail( ) )
					CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #1 to compcolour command" );
				else
				{
					if( SS.eof( ) )
						CONSOLE_Print( "[GAME: " + m_GameName + "] missing input #2 to compcolour command" );
					else
					{
						SS >> Colour;

						if( SS.fail( ) )
							CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #2 to compcolour command" );
						else
						{
							unsigned char SID = (unsigned char)( Slot - 1 );

							if( !( m_Map->GetMapOptions( ) & MAPOPT_FIXEDPLAYERSETTINGS ) && Colour < 12 && SID < m_Slots.size( ) )
							{
								if( m_Slots[SID].GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && m_Slots[SID].GetComputer( ) == 1 )
									ColourSlot( SID, Colour );
							}
						}
					}
				}
			}

			//
			// !COMPHANDICAP (computer handicap change)
			//

			else if( Command == "comphandicap" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded && !m_SaveGame )
			{
				// extract the slot and the handicap
				// e.g. "1 50" -> slot: "1", handicap: "50"

				uint32_t Slot;
				uint32_t Handicap;
				stringstream SS;
				SS << Payload;
				SS >> Slot;

				if( SS.fail( ) )
					CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #1 to comphandicap command" );
				else
				{
					if( SS.eof( ) )
						CONSOLE_Print( "[GAME: " + m_GameName + "] missing input #2 to comphandicap command" );
					else
					{
						SS >> Handicap;

						if( SS.fail( ) )
							CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #2 to comphandicap command" );
						else
						{
							unsigned char SID = (unsigned char)( Slot - 1 );

							if( !( m_Map->GetMapOptions( ) & MAPOPT_FIXEDPLAYERSETTINGS ) && ( Handicap == 50 || Handicap == 60 || Handicap == 70 || Handicap == 80 || Handicap == 90 || Handicap == 100 ) && SID < m_Slots.size( ) )
							{
								if( m_Slots[SID].GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && m_Slots[SID].GetComputer( ) == 1 )
								{
									m_Slots[SID].SetHandicap( (unsigned char)Handicap );
									SendAllSlotInfo( );
								}
							}
						}
					}
				}
			}

			//
			// !COMPRACE (computer race change)
			//

			else if( Command == "comprace" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded && !m_SaveGame )
			{
				// extract the slot and the race
				// e.g. "1 human" -> slot: "1", race: "human"

				uint32_t Slot;
				string Race;
				stringstream SS;
				SS << Payload;
				SS >> Slot;

				if( SS.fail( ) )
					CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #1 to comprace command" );
				else
				{
					if( SS.eof( ) )
						CONSOLE_Print( "[GAME: " + m_GameName + "] missing input #2 to comprace command" );
					else
					{
						getline( SS, Race );
						string :: size_type Start = Race.find_first_not_of( " " );

						if( Start != string :: npos )
							Race = Race.substr( Start );

						transform( Race.begin( ), Race.end( ), Race.begin( ), (int(*)(int))tolower );
						unsigned char SID = (unsigned char)( Slot - 1 );

						if( !( m_Map->GetMapOptions( ) & MAPOPT_FIXEDPLAYERSETTINGS ) && !( m_Map->GetMapFlags( ) & MAPFLAG_RANDOMRACES ) && SID < m_Slots.size( ) )
						{
							if( m_Slots[SID].GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && m_Slots[SID].GetComputer( ) == 1 )
							{
								if( Race == "human" )
								{
									m_Slots[SID].SetRace( SLOTRACE_HUMAN | SLOTRACE_SELECTABLE );
									SendAllSlotInfo( );
								}
								else if( Race == "orc" )
								{
									m_Slots[SID].SetRace( SLOTRACE_ORC | SLOTRACE_SELECTABLE );
									SendAllSlotInfo( );
								}
								else if( Race == "night elf" )
								{
									m_Slots[SID].SetRace( SLOTRACE_NIGHTELF | SLOTRACE_SELECTABLE );
									SendAllSlotInfo( );
								}
								else if( Race == "undead" )
								{
									m_Slots[SID].SetRace( SLOTRACE_UNDEAD | SLOTRACE_SELECTABLE );
									SendAllSlotInfo( );
								}
								else if( Race == "random" )
								{
									m_Slots[SID].SetRace( SLOTRACE_RANDOM | SLOTRACE_SELECTABLE );
									SendAllSlotInfo( );
								}
								else
									CONSOLE_Print( "[GAME: " + m_GameName + "] unknown race [" + Race + "] sent to comprace command" );
							}
						}
					}
				}
			}

			//
			// !COMPTEAM (computer team change)
			//

			else if( Command == "compteam" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded && !m_SaveGame )
			{
				// extract the slot and the team
				// e.g. "1 2" -> slot: "1", team: "2"

				uint32_t Slot;
				uint32_t Team;
				stringstream SS;
				SS << Payload;
				SS >> Slot;

				if( SS.fail( ) )
					CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #1 to compteam command" );
				else
				{
					if( SS.eof( ) )
						CONSOLE_Print( "[GAME: " + m_GameName + "] missing input #2 to compteam command" );
					else
					{
						SS >> Team;

						if( SS.fail( ) )
							CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #2 to compteam command" );
						else
						{
							unsigned char SID = (unsigned char)( Slot - 1 );

							if( !( m_Map->GetMapOptions( ) & MAPOPT_FIXEDPLAYERSETTINGS ) && Team < 12 && SID < m_Slots.size( ) )
							{
								if( m_Slots[SID].GetSlotStatus( ) == SLOTSTATUS_OCCUPIED && m_Slots[SID].GetComputer( ) == 1 )
								{
									m_Slots[SID].SetTeam( (unsigned char)( Team - 1 ) );
									SendAllSlotInfo( );
								}
							}
						}
					}
				}
			}

			//
			// !DBSTATUS
			//

			else if( Command == "dbstatus" )
				SendAllChat( m_GHost->m_DB->GetStatus( ) );

			//
			// !DOWNLOAD
			// !DL
			//

			else if( ( Command == "download" || Command == "dl" ) && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded )
			{
				CGamePlayer *LastMatch = NULL;
				uint32_t Matches = GetPlayerFromNamePartial( Payload, &LastMatch );

				if( Matches == 0 )
					SendAllChat( tr("lang_0118", Payload ) ); // UnableToStartDownloadNoMatchesFound
				else if( Matches == 1 )
				{
					if( !LastMatch->GetDownloadStarted( ) && !LastMatch->GetDownloadFinished( ) )
					{
						unsigned char SID = GetSIDFromPID( LastMatch->GetPID( ) );

						if( SID < m_Slots.size( ) && m_Slots[SID].GetDownloadStatus( ) != 100 )
						{
							// inform the client that we are willing to send the map

							CONSOLE_Print( "[GAME: " + m_GameName + "] map download started for player [" + LastMatch->GetName( ) + "]" );
							Send( LastMatch, m_Protocol->SEND_W3GS_STARTDOWNLOAD( GetHostPID( ) ) );
							LastMatch->SetDownloadAllowed( true );
							LastMatch->SetDownloadStarted( true );
							LastMatch->SetStartedDownloadingTicks( GetTicks( ) );
						}
					}
				}
				else
					SendAllChat( tr("lang_0119", Payload ) ); // UnableToStartDownloadFoundMoreThanOneMatch
			}

			//
			// !DLINFO
			//

			else if( ( Command == "dlinfo" ) )
			{
				if (Payload.empty())
					m_GHost->m_ShowDownloadsInfo = !m_GHost->m_ShowDownloadsInfo;
				if (!Payload.empty())
				{
					transform( Payload.begin( ), Payload.end( ), Payload.begin( ), (int(*)(int))tolower );
					if (Payload == "on")
						m_GHost->m_ShowDownloadsInfo = true;
					else
						m_GHost->m_ShowDownloadsInfo = false;
				}
				if (m_GHost->m_ShowDownloadsInfo)
					SendAllChat( tr("lang_1161") ); // Show Downloads Info = ON
				else
					SendAllChat( tr("lang_1162") ); // Show Downloads Info = OFF
			}

			//
			// !DLINFOTIME
			//

			else if( ( Command == "dlinfotime" ) && !Payload.empty( ) )
			{
				uint32_t itime = m_GHost->m_ShowDownloadsInfoTime;
				m_GHost->m_ShowDownloadsInfoTime = UTIL_ToUInt32( Payload );
				SendAllChat( tr("lang_1163", "$TIME$", Payload, "$PRE_TIME", UTIL_ToString(itime))); // Show Downloads Info Time = "+ Payload+" s, previously "+UTIL_ToString(itime)+" s"
			}

			//
			// !DLTSPEED
			//

			else if( ( Command == "dltspeed" ) && !Payload.empty( ) )
			{
				uint32_t tspeed = m_GHost->m_totaldownloadspeed;
				m_GHost->m_totaldownloadspeed = UTIL_ToUInt32( Payload );
				if (Payload=="max")
					m_GHost->m_totaldownloadspeed = 0;
				SendAllChat( tr("lang_1164", "$SPEED$", Payload, "$PRE_SPEED", UTIL_ToString(tspeed)) ); // "Total download speed = "+ Payload+" KB/s, previously "+UTIL_ToString(tspeed)+" KB/s"
			}

			//
			// !DLSPEED
			//

			else if( ( Command == "dlspeed" ) && !Payload.empty( ) )
			{
				uint32_t tspeed = m_GHost->m_clientdownloadspeed;
				m_GHost->m_clientdownloadspeed = UTIL_ToUInt32( Payload );
				if (Payload=="max")
					m_GHost->m_clientdownloadspeed = 0;
				SendAllChat( tr("lang_1165", "$SPEED$", Payload, "$PRE_SPEED", UTIL_ToString(tspeed)) ); // "Maximum player download speed = "+ Payload+" KB/s, previously "+UTIL_ToString(tspeed)+" KB/s"
			}

			//
			// !DLMAX
			//

			else if( ( Command == "dlmax" ) && !Payload.empty( ) )
			{
				uint32_t t = m_GHost->m_maxdownloaders;
				m_GHost->m_maxdownloaders = UTIL_ToUInt32( Payload );
				if (Payload=="max")
					m_GHost->m_maxdownloaders = 0;
				SendAllChat( tr("lang_1166", "$COUNT$", Payload, "$PRE_COUNT", UTIL_ToString(t))); // "Maximum concurrent downloads = "+ Payload+", previously "+UTIL_ToString(t)
			}

			//
			// !DROP
			//

			else if( Command == "drop" && m_GameLoaded )
				StopLaggers( tr("lang_1167") ); // lagged out (dropped by admin)

			//
			// !ENDN
			//

			else if( Command == "endn" && m_GameLoaded )
			{
				if (!m_GameEndCountDownStarted)
					if (m_GHost->m_EndReq2ndTeamAccept && m_EndRequested)
						if (m_Slots[GetSIDFromPID(player->GetPID())].GetTeam()!=m_EndRequestedTeam)
						{
							CONSOLE_Print( "[GAME: " + m_GameName + "] is over (admin ended game)" );
							SendAllChat(tr("lang_1168")); // "Game will end in 5 seconds"
							m_GameEndCountDownStarted = true;
							m_GameEndCountDownCounter = 10;
							m_GameEndLastCountDownTicks = GetTicks();
						}

				if (m_GHost->m_EndReq2ndTeamAccept && !RootAdminCheck)
				{
					bool secondTeamPresent = false;

					unsigned char PID = player->GetPID();
					for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
					{
						if (m_Slots[GetSIDFromPID((*i)->GetPID())].GetTeam()!=m_Slots[GetSIDFromPID(PID)].GetTeam())
							secondTeamPresent = true;
					}

					if (m_GetMapNumTeams==2 && secondTeamPresent)
					{
						m_EndRequestedTeam = m_Slots[GetSIDFromPID(player->GetPID())].GetTeam();
						if (!m_EndRequested)
						{
							m_EndRequestedTicks = GetTicks();
							m_EndRequested = true;
							for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
							{
								if (m_Slots[GetSIDFromPID((*i)->GetPID())].GetTeam()!=m_EndRequestedTeam)
									SendChat((*i)->GetPID(), tr("lang_1169", "$USER$", User, "$TRIGGER$", string(1, m_GHost->m_CommandTrigger) )); // " User + wants to end the game, type "+m_GHost->m_CommandTrigger+"end to accept"
								else
									SendChat((*i)->GetPID(), tr("lang_1170", User)); // User + " wants to end the game, waiting for the other team to accept...
							}

						}
						return HideCommand;
					}
				}

				if (!CMDCheck(CMD_end, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}

				CONSOLE_Print( "[GAME: " + m_GameName + "] is over (admin ended game)" );
				StopPlayers( tr("lang_1171") ); // "was disconnected (admin ended game)"
			}

			//
			// !ENDS
			//

			else if( (Command == "ends" || Command == "a") && m_GameLoaded )
			{
				if (!CMDCheck(CMD_end, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}
				CONSOLE_Print( "[GAME: " + m_GameName + "] canceled end game" );
				SendAllChat( tr("lang_1172") ); // Admin stoped end countdown"
				m_GameEndCountDownStarted = false;
			}

			//
			// !END
			//

			else if( Command == "end" && m_GameLoaded )
			{
				bool access = CMDCheck(CMD_end, AdminAccess);

					
				if (!Payload.empty())
				{
					if ( !m_Stats )
					{
						SendChat(player->GetPID(), tr("lang_1208"));
						return HideCommand;
					}
					string winnerString; winnerString.clear();
					uint32_t RequestedWinner = 0;
					
					if ( m_Map->GetMapType( ) == "dota" ) //FIXME, m_Map is a null pointer, why???
					{
						if ( Payload == "1" )
						{
							winnerString = "[SENTINEL]";
							RequestedWinner = 1;
						}
						else if ( Payload == "2" )
						{
							winnerString = "[SCOURGE]";
							RequestedWinner = 2;
						}
						else if ( Payload == "0" )
						{
							winnerString = "[DRAW]";
							RequestedWinner = 0;
						}
					}

					if ( !RequestedWinner )
					{
						SendChat(player->GetPID(), tr("lang_1209"));
						return HideCommand;
					}
					
					if (!m_GameEndCountDownStarted)
					{
						if (m_GHost->m_EndReq2ndTeamAccept && m_RequestedWinner && UTIL_ToUInt32(Payload) == m_RequestedWinner)
						if (m_Slots[GetSIDFromPID(player->GetPID())].GetTeam()!=m_EndRequestedTeam)
						{
							CONSOLE_Print( "[GAME: " + m_GameName + "] is over (admin ended game)" );
							SendAllChat(tr("lang_1168")); // Game will end in 10 seconds"
							m_GameEndCountDownStarted = true;
							m_GameEndCountDownCounter = 10;
							m_GameEndLastCountDownTicks = GetTicks();
							m_Stats->SetWinner(0);
							return HideCommand;
						}
					}

					if (m_GHost->m_EndReq2ndTeamAccept && !RootAdminCheck)
					{
						bool secondTeamPresent = false;
						unsigned char PID = player->GetPID();

						for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
						{
							if (m_Slots[GetSIDFromPID((*i)->GetPID())].GetTeam()!=m_Slots[GetSIDFromPID(PID)].GetTeam())
								secondTeamPresent = true;
						}
	

						if (m_GetMapNumTeams==2 && secondTeamPresent)
						{
							m_EndRequestedTeam = m_Slots[GetSIDFromPID(player->GetPID())].GetTeam();

							if (!m_RequestedWinner)
							{
								m_EndRequestedTicks = GetTicks();
								m_RequestedWinner = RequestedWinner;
								for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
								{
									if (m_Slots[GetSIDFromPID((*i)->GetPID())].GetTeam()!=m_EndRequestedTeam)
										SendChat((*i)->GetPID(), tr("lang_1173", "$USER$", User, "$WINNER$", winnerString, "$TRIGGER$", string(1, m_GHost->m_CommandTrigger)) ); // wants to end the game with winner "+winnerString+", type "+m_GHost->m_CommandTrigger+"end "+UTIL_ToString(m_RequestedWinner)+" to accept");
									else
										SendChat((*i)->GetPID(), tr("lang_1174", "$USER$", User, "$WINNER$", winnerString) ); 
								}


							}
							return HideCommand;
						}
					}
					
					
					CONSOLE_Print( "[GAME: " + m_GameName + "] is over (admin ended game)" );
					SendAllChat(tr("lang_1168")); // "Game will end in 10 seconds"
					m_GameEndCountDownStarted = true;
					m_GameEndCountDownCounter = 10;
					m_GameEndLastCountDownTicks = GetTicks();
					m_Stats->SetWinner(0);
					return HideCommand;
				}

				if (!m_GameEndCountDownStarted)
				{
					if (m_GHost->m_EndReq2ndTeamAccept && m_EndRequested)
					if (m_Slots[GetSIDFromPID(player->GetPID())].GetTeam()!=m_EndRequestedTeam)
					{
						CONSOLE_Print( "[GAME: " + m_GameName + "] is over (admin ended game)" );
						SendAllChat(tr("lang_1168")); // Game will end in 10 seconds"
						m_GameEndCountDownStarted = true;
						m_GameEndCountDownCounter = 10;
						m_GameEndLastCountDownTicks = GetTicks();
						return HideCommand;	
					}
				}

				if (m_GHost->m_EndReq2ndTeamAccept && !RootAdminCheck)
				{
					bool secondTeamPresent = false;
					
					unsigned char PID = player->GetPID();
					for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
					{
						if (m_Slots[GetSIDFromPID((*i)->GetPID())].GetTeam()!=m_Slots[GetSIDFromPID(PID)].GetTeam())
							secondTeamPresent = true;
					}

					if (m_GetMapNumTeams==2 && secondTeamPresent)
					{
						m_EndRequestedTeam = m_Slots[GetSIDFromPID(player->GetPID())].GetTeam();
						if (!m_EndRequested)
						{
							m_EndRequestedTicks = GetTicks();
							m_EndRequested = true;
							for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
							{
								if (m_Slots[GetSIDFromPID((*i)->GetPID())].GetTeam()!=m_EndRequestedTeam)
									SendChat((*i)->GetPID(), tr("lang_1169", "$USER$", User, "$TRIGGER$", string(1, m_GHost->m_CommandTrigger)) ); // User + " wants to end the game, type "+m_GHost->m_CommandTrigger+"end to accept"
								else
									SendChat((*i)->GetPID(), tr("lang_1170", "$USER$", User)); // User + " wants to end the game, waiting for the other team to accept..."
							}
						}
						return HideCommand;
					}
				}

				if (!access)
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}

				CONSOLE_Print( "[GAME: " + m_GameName + "] is over (admin ended game)" );
				SendAllChat(tr("lang_1168")); // "Game will end in 10 seconds"
				m_GameEndCountDownStarted = true;
				m_GameEndCountDownCounter = 10;
				m_GameEndLastCountDownTicks = GetTicks();
//				StopPlayers( "was disconnected (admin ended game)" );
				return HideCommand;
			}

			//
			// !FW
			//

			else if(Command == "fw" && !m_GHost->m_BNETs.empty( ) )
			{
				string sMsg = Payload;
				if (sMsg.empty())
				{
					if (m_GHost->m_CurrentGame)
					{
						string slots = UTIL_ToString(m_GHost->m_CurrentGame->GetSlotsOccupied())+"/"+UTIL_ToString(m_GHost->m_CurrentGame->m_Slots.size());
						sMsg = "Lobby: "+ m_GHost->m_CurrentGame->GetGameName()+" ("+slots+") - "+m_GHost->m_CurrentGame->GetOwnerName();
					} else
					sMsg = GetGameInfo();
				}
				if (!sMsg.empty())
					m_GHost->m_BNETs[0]->QueueChatCommand( "/f msg "+sMsg);
			}

			//
			// !FAKEPLAYER
			//

			else if( Command == "fakeplayer" && !m_CountDownStarted )
			{
				if( m_FakePlayerPID == 255 )
					CreateFakePlayer( );
				else
					DeleteFakePlayer( );
			}

			//
			// !FPPAUSE
			//

			else if( Command == "fppause" && m_FakePlayerPID != 255 && m_GameLoaded )
			{
				BYTEARRAY CRC;
				BYTEARRAY Action;
				Action.push_back( 1 );
				m_Actions.push( new CIncomingAction( m_FakePlayerPID, CRC, Action ) );
			}

			//
			// !FPRESUME
			//

			else if( Command == "fpresume" && m_FakePlayerPID != 255 && m_GameLoaded )
			{
				BYTEARRAY CRC;
				BYTEARRAY Action;
				Action.push_back( 2 );
				m_Actions.push( new CIncomingAction( m_FakePlayerPID, CRC, Action ) );
			}

			//
			// !Spoof
			//

			else if(( Command == "spoof") && !Payload.empty( ) )
			{
				string OwnerLower;
				string Victim;
				string Msg;
				stringstream SS;
				SS << Payload;
				SS >> Victim;

				if( !SS.eof( ) )
				{
					getline( SS, Msg );
					string :: size_type Start = Msg.find_first_not_of( " " );

					if( Start != string :: npos )
						Msg = Msg.substr( Start );
				}

				CGamePlayer *LastMatch = NULL;
				uint32_t Matches = GetPlayerFromNamePartial( Victim , &LastMatch );

				bool isAdmin = false;
				bool isOwner = false;

				if (LastMatch)
				{
					if (LastMatch->GetName() == OwnerLower)
						isOwner = true;

					for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
					{
						if( (*i)->GetServer( ) == m_Server)
						{
							isAdmin = (*i)->IsAdmin( LastMatch->GetName());
							break;
						}
					}
				}
				if( Matches == 0 )
					CONSOLE_Print("Not matches for spoof");

				else if( Matches == 1 && !( isAdmin || isOwner ) )
				{
					SendAllChat(LastMatch->GetPID(), Msg);
				}
				else
					CONSOLE_Print("Found more than one match, or you are trying to spoof an admin");
			}

			//
			// !onlyp
			//

			else if( Command == "onlyp" && !m_GameLoading && !m_GameLoaded )
			{
				string Froms;
				string From;
				string s;
				bool isAdmin = false;
				bool allowed = false;

				if (Payload.empty( ))
				{
					if (m_ProviderCheck)
						SendAllChat(tr("lang_1175") ); // "Provider check disabled, allowing all providers"

					m_ProviderCheck=false;
				} else
				{
					m_ProviderCheck=true;
					m_Providers=Payload;
					transform( m_Providers.begin( ), m_Providers.end( ), m_Providers.begin( ), (int(*)(int))toupper );

					SendAllChat( tr("lang_1085", m_Providers) ); //Provider check enabled, allowed providers: "+m_Providers

					for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
					{
						// we reverse the byte order on the IP because it's stored in network byte order

//						From =m_GHost->UDPChatWhoIs((*i)->GetExternalIPString( ));
						From =(*i)->GetProvider( );
						transform( From.begin( ), From.end( ), From.begin( ), (int(*)(int))toupper );

						isAdmin = IsOwner((*i)->GetName( ));
						allowed = false;

						for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
						{
							if( (*j)->IsAdmin((*i)->GetName( ) ) || (*j)->IsRootAdmin( (*i)->GetName( ) ) )
							{
								isAdmin = true;
								break;
							}
						}

						if (IsReserved ((*i)->GetName()))
							isAdmin=true;

						if (m_ProviderCheck)
						{
							stringstream SS;
							SS << m_Providers;

							while( !SS.eof( ) )
							{
								SS >> s;
								if (From.find(s)!=string :: npos)
									allowed=true;
							}
						}

						if (m_ProviderCheck2)
						{
							stringstream SS;
							SS << m_Providers2;

							while( !SS.eof( ) )
							{
								SS >> s;
								if (From.find(s)!=string :: npos)
									allowed=false;
							}
						}

						if (!allowed)
							if ((*i)->GetName( )!=User)
								if (isAdmin==false)
								{
									SendAllChat( tr("lang_0998", "$VICTIM$", (*i)->GetName( ), "$PROVIDER$", From ) ); // AutokickingPlayerForDeniedProvider
									(*i)->SetDeleteMe( true );
									(*i)->SetLeftReason(  tr("lang_1176", "$PROVIDER$", From, "$FROM$", From )); // "was autokicked," + From + " provider "+From+" not allowed"
									(*i)->SetLeftCode( PLAYERLEAVE_LOBBY );
									OpenSlot( GetSIDFromPID( (*i)->GetPID( ) ), false );
								}

								Froms += (*i)->GetName( );
								Froms += ": (";
								Froms += From;
								Froms += ")";

								if( i != m_Players.end( ) - 1 )
									Froms += ", ";
					}

					//				SendAllChat( Froms );
				}
			}

			//
			// !nop
			//

			else if( Command == "nop" && !m_GameLoading && !m_GameLoaded )
			{
				string Froms;
				string From;
				string s;
				bool isAdmin = false;
				bool allowed = false;

				if (Payload.empty( ))
				{
					if (m_ProviderCheck2)
						SendAllChat( "Provider check disabled, no denied providers");

					m_ProviderCheck2=false;
				} else
				{
					m_ProviderCheck2=true;
					m_Providers2=Payload;
					transform( m_Providers2.begin( ), m_Providers2.end( ), m_Providers2.begin( ), (int(*)(int))toupper );

					SendAllChat( "Provider check enabled, denied providers: "+m_Providers2);

					for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
					{
						// we reverse the byte order on the IP because it's stored in network byte order

//						From =m_GHost->UDPChatWhoIs((*i)->GetExternalIPString( ));
						From =(*i)->GetProvider( );
						transform( From.begin( ), From.end( ), From.begin( ), (int(*)(int))toupper );

						isAdmin = IsOwner((*i)->GetName( ));
						if (m_ProviderCheck)
							allowed = false;
						else
							allowed = true;

						for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
						{
							if((*j)->IsSafe((*i)->GetName( ) ) || (*j)->IsAdmin((*i)->GetName( ) ) || (*j)->IsRootAdmin( (*i)->GetName( ) ) )
							{
								isAdmin = true;
								break;
							}
						}

						if (IsReserved ((*i)->GetName()))
							isAdmin=true;

						if (m_ProviderCheck)
						{
							stringstream SS;
							SS << m_Providers;

							while( !SS.eof( ) )
							{
								SS >> s;
								if (From.find(s)!=string :: npos)
									allowed=true;
							}
						}

						if (m_ProviderCheck2)
						{
							stringstream SS;
							SS << m_Providers2;

							while( !SS.eof( ) )
							{
								SS >> s;
								if (From.find(s)!=string :: npos)
									allowed=false;
							}
						}

						if (!allowed)
							if ((*i)->GetName( )!=User)
								if (isAdmin==false)
								{
									SendAllChat( tr("lang_0998", "$VICTIM$", (*i)->GetName( ), "$PROVIDER$", From ) ); // AutokickingPlayerForDeniedProvider
									(*i)->SetDeleteMe( true );
									(*i)->SetLeftReason( tr("lang_1176", "$PROVIDER$", From, "$FROM$", From ) ); // "was autokicked," + From + " provider "+From+" not allowed"
									(*i)->SetLeftCode( PLAYERLEAVE_LOBBY );
									OpenSlot( GetSIDFromPID( (*i)->GetPID( ) ), false );
								}

								Froms += (*i)->GetName( );
								Froms += ": (";
								Froms += From;
								Froms += ")";

								if( i != m_Players.end( ) - 1 )
									Froms += ", ";
					}

					//				SendAllChat( Froms );
				}
			}

			//
			// !onlys
			//

			else if( Command == "onlys" && !m_GameLoading && !m_GameLoaded )
			{
				double Score;
				string ss;
				string Scores;
				string ScoreS;
				bool isAdmin = false;

				if (Payload.empty( ))
				{
					m_ScoreCheck=false;
				} else

				{
					m_ScoreCheck=true;
					m_Scores = UTIL_ToDouble(Payload);
					Scores = UTIL_ToString(m_Scores, 2);

					SendAllChat( tr("lang_1177", Scores)); // "Score check enabled, allowed score: >="+Scores

					for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
					{
						ss = (*i)->GetScoreS();
						Score = UTIL_ToDouble(ss);
						ScoreS = UTIL_ToString(Score, 2);

						isAdmin = false;

						for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
						{
							if((*j)->IsAdmin((*i)->GetName( ) ) || (*j)->IsRootAdmin( (*i)->GetName( ) ) )
							{
								isAdmin = true;
								break;
							}
						}

						if (IsReserved ((*i)->GetName()))
							isAdmin=true;

						bool allow = false;
						allow = (m_GHost->m_AllowNullScoredPlayers && (Score==0));

						if (!allow)
						if (Score<m_Scores)
							if ((*i)->GetName( )!=User)
								if (isAdmin==false)
								{
									SendAllChat( tr("lang_1001", "$NAME$", (*i)->GetName( ), "$SCORE$", ScoreS, "$REQSCORE$", Scores));  // AutokickingPlayerForDeniedScore( (*i)->GetName( ), ScoreS, Scores )
									(*i)->SetDeleteMe( true );
									(*i)->SetLeftReason( tr("lang_1045",ScoreS) );  // "was autokicked," + ScoreS + " score too low"
									(*i)->SetLeftCode( PLAYERLEAVE_LOBBY );
									OpenSlot( GetSIDFromPID( (*i)->GetPID( ) ), false );
								}
					}

					//				SendAllChat( Froms );
				}
			}

			//
			// !only
			//

			if( Command == "only" && !m_GameLoading && !m_GameLoaded )
			{
				string Froms;
				string From;
				string Fromu;
				bool isAdmin = false;

				if (Payload.empty( ))
				{
					if (m_CountryCheck)
						SendAllChat( tr("lang_1178")); // Country check disabled, allowing all countries

					m_CountryCheck=false;
				} else
				{
					m_CountryCheck=true;
					m_Countries=Payload;
					transform( m_Countries.begin( ), m_Countries.end( ), m_Countries.begin( ), (int(*)(int))toupper );

					SendAllChat( tr("lang_1077", m_Countries)); //  "Country check enabled, allowed countries: "+m_Countries
					m_Countries=m_Countries+" ??";

				for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
				{
					// we reverse the byte order on the IP because it's stored in network byte order

					From =(*i)->GetCountry();
					Fromu = From;
					transform( Fromu.begin( ), Fromu.end( ), Fromu.begin( ), (int(*)(int))toupper );

					//					From =m_GHost->m_DB->FromCheck( UTIL_ByteArrayToUInt32( (*i)->GetExternalIP( ), true ) );

					isAdmin = IsOwner((*i)->GetName( ));

					for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
					{
						if((*j)->IsSafe((*i)->GetName( ) ) || (*j)->IsAdmin((*i)->GetName( ) ) || (*j)->IsRootAdmin( (*i)->GetName( ) ) )
						{
							isAdmin = true;
							break;
						}
					}

					if (IsReserved ((*i)->GetName()))
						isAdmin=true;

					if (m_Countries.find(Fromu)==string :: npos)
						if ((*i)->GetName( )!=User)
						if (isAdmin==false)
					{
						SendAllChat( tr("lang_0999", "$VICTIM$", (*i)->GetName( ), "$COUNTRY$", From)); // AutokickingPlayerForDeniedCountry( (*i)->GetName( ), From )
						(*i)->SetDeleteMe( true );
						(*i)->SetLeftReason( tr("lang_1046", From) ); //  "was autokicked," + From + " not on the allowed countries list"
						(*i)->SetLeftCode( PLAYERLEAVE_LOBBY );
						OpenSlot( GetSIDFromPID( (*i)->GetPID( ) ), false );
					}

					Froms += (*i)->GetName( );
					Froms += ": (";
					Froms += From;
					Froms += ")";

					if( i != m_Players.end( ) - 1 )
						Froms += ", ";
				}

//				SendAllChat( Froms );
				}
			}

			//
			// !no
			//

			else if( Command == "no" && !m_GameLoading && !m_GameLoaded )
			{
				string Froms;
				string From;
				bool isAdmin = false;

				if (Payload.empty( ))
				{
					if (m_CountryCheck2)
						SendAllChat( tr("lang_1179") ); // "Country check disabled, no denied countries"

					m_CountryCheck2=false;
				} else
				{
					m_CountryCheck2=true;
					m_Countries2=Payload;
					transform( m_Countries2.begin( ), m_Countries2.end( ), m_Countries2.begin( ), (int(*)(int))toupper );

					SendAllChat( tr("lang_1180", m_Countries2)); // "Country check enabled, denied countries: "+m_Countries2

					for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
					{
						// we reverse the byte order on the IP because it's stored in network byte order

						From =(*i)->GetCountry();
						transform( From.begin( ), From.end( ), From.begin( ), (int(*)(int))toupper );

//						From =m_GHost->m_DB->FromCheck( UTIL_ByteArrayToUInt32( (*i)->GetExternalIP( ), true ) );

						isAdmin = IsOwner((*i)->GetName( ));

						for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
						{
							if((*j)->IsAdmin((*i)->GetName( ) ) || (*j)->IsRootAdmin( (*i)->GetName( ) ) )
							{
								isAdmin = true;
								break;
							}
						}

						if (IsReserved ((*i)->GetName()))
							isAdmin=true;

						if (m_Countries2.find(From)!=string :: npos)
							if ((*i)->GetName( )!=User)
								if (isAdmin==false)
								{
									SendAllChat( tr("lang_0999", "$VICTIM$", (*i)->GetName( ), "$COUNTRY$", From )); // AutokickingPlayerForDeniedCountry( (*i)->GetName( ), From )
									(*i)->SetDeleteMe( true );
									(*i)->SetLeftReason( tr("lang_1046", From ) );  // "was autokicked," + From + " not on the allowed countries list"
									(*i)->SetLeftCode( PLAYERLEAVE_LOBBY );
									OpenSlot( GetSIDFromPID( (*i)->GetPID( ) ), false );
								}

								Froms += (*i)->GetName( );
								Froms += ": (";
								Froms += From;
								Froms += ")";

								if( i != m_Players.end( ) - 1 )
									Froms += ", ";
					}

					//				SendAllChat( Froms );
				}
			}

			//
			// !IPS
			//

			else if( Command == "ips" )
			{
				string Froms;

				for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
				{
					// we reverse the byte order on the IP because it's stored in network byte order

					Froms += (*i)->GetName( );
					Froms += ": (";
					Froms += (*i)->GetExternalIPString( );
					Froms += ")";

					if( i != m_Players.end( ) - 1 )
						Froms += ", ";
				}

				SendAdminChat( Froms );
			}

			//
			// !FROMP
			// !FP
			//

			else if( Command == "fromp" || Command == "fp" )
			{
				string Froms;
				string Froms2;
				string CNL;
				string CN;
				string P;
				bool samecountry=true;

				if (!Payload.empty())
				{
					CGamePlayer *LastMatch = NULL;
					uint32_t Matches = GetPlayerFromNamePartial( Payload , &LastMatch );

					if( Matches == 0 )
						CONSOLE_Print("No matches");

					else if( Matches == 1 )
					{
						Froms = LastMatch->GetName( );
						Froms += ": (";
						CN = LastMatch->GetCountry();
						P = LastMatch->GetProvider();
						Froms += CN+"-"+P;
						Froms += ")";
						SendAllChat(Froms);
					}
					else
						CONSOLE_Print("Found more than one match");
					return HideCommand;
				}

				for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
				{
					// we reverse the byte order on the IP because it's stored in network byte order

					Froms2 += (*i)->GetName( );
					Froms2 += ": (";
					Froms += (*i)->GetName( );
					Froms += ": (";
//					CN = m_GHost->m_DB->FromCheck( UTIL_ByteArrayToUInt32( (*i)->GetExternalIP( ), true ) );
					CN = (*i)->GetCountry();
//					P = m_GHost->UDPChatWhoIs((*i)->GetExternalIPString());
					P = (*i)->GetProvider();
					Froms += CN+"-"+P;
					Froms2 += P;
					Froms += ")";
					Froms2 += ")";

					if (CNL=="")
						CNL=CN;
					else
						if (CN!=CNL)
							samecountry=false;

					if( i != m_Players.end( ) - 1 )
					{
						Froms += ", ";
						Froms2 += ", ";
					}
				}
				Froms2 += " "+ tr("lang_1181", CNL);   // " are all from ("+CNL+")";

				if (samecountry)
					SendAllChat( Froms2 );
				else
					SendAllChat( Froms );
			}

			//
			// !L
			//

			else if( Command == "l" )
			{
				string Froms;
				string CN;
				uint32_t Ping;
				CGamePlayer *Player = NULL;
				if (m_LastPlayerJoined!=255)
					Player = GetPlayerFromPID(m_LastPlayerJoined);
				if (Player==NULL) return HideCommand;
				Ping = Player->GetPing( m_GHost->m_LCPings );
				Froms = Player->GetName( );
				Froms += ": "+UTIL_ToString(Ping)+" ms (";
				CN = Player->GetCountry();
				Froms += CN;
				Froms += ")";
				SendAllChat(Froms);
			}

			//
			// !WTV
			//

			else if( Command == "wtv" && !m_CountDownStarted && !m_GameLoaded )
			{
				if( Payload.empty( ) )
				{
					if( m_WTVPlayerPID == 255 )
					{
						CloseSlot( m_Slots.size()-2, true );
						CloseSlot( m_Slots.size()-1, true );
						CreateWTVPlayer( m_GHost->m_wtvPlayerName );
					}
					else
					{
						DeleteWTVPlayer( );
						OpenSlot( m_Slots.size()-2, true );
						OpenSlot( m_Slots.size()-1, true );
					}
				}
				else if( Payload == "off" )
				{
					DeleteWTVPlayer( );
					OpenSlot( m_Slots.size()-2, true );
					OpenSlot( m_Slots.size()-1, true );
				}
				else if( Payload == "on" )
				{
					CloseSlot( m_Slots.size()-2, true );
					CloseSlot( m_Slots.size()-1, true );
					CreateWTVPlayer( );
				}
				else
				{
					string CMD1;
					string CMD2;
					stringstream SS;
					SS << Payload;
					SS >> CMD1;

					if( SS.fail( ) )
						CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #1 to wtv command" );
					else
					{
						if( SS.eof( ) )
							CONSOLE_Print( "[GAME: " + m_GameName + "] missing input #2 to wtv command" );
						else
						{
							SS >> CMD2;

							if( SS.fail( ) )
								CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #2 to wtv command" );
							else
							{
								if( CMD1 == "name" )
								{
									DeleteWTVPlayer( );
									m_GHost->m_wtvPlayerName = CMD2;
									OpenSlot( m_Slots.size()-2, true );
									OpenSlot( m_Slots.size()-1, true );
									CreateWTVPlayer( CMD2 );
								}
							}
						}
					}
				}
			}


			//
			// !FROM
			// !F
			//

			else if( ( Command == "from" || Command == "f" ) )
			{
				string Froms;
				string Froms2;
				string CNL;
				string CN;
				bool samecountry=true;

				if (!Payload.empty())
				{
					CGamePlayer *LastMatch = NULL;
					uint32_t Matches = GetPlayerFromNamePartial( Payload , &LastMatch );

					if( Matches == 0 )
						CONSOLE_Print("No matches");

					else if( Matches == 1 )
					{
						Froms = LastMatch->GetName( );
						Froms += ": (";
						CN = LastMatch->GetCountry();
						Froms += CN;
						Froms += ")";
						SendAllChat(Froms);
					}
					else
						CONSOLE_Print("Found more than one match");
					return HideCommand;
				}

				for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
				{
					// we reverse the byte order on the IP because it's stored in network byte order

					Froms2 += (*i)->GetName( );
					Froms += (*i)->GetName( );
					Froms += ": (";
					CN = (*i)->GetCountry();
//					CN = m_GHost->m_DB->FromCheck( UTIL_ByteArrayToUInt32( (*i)->GetExternalIP( ), true ) );
					Froms += CN;
					Froms += ")";

					if (CNL=="")
						CNL=CN;
					else
					if (CN!=CNL)
						samecountry=false;

					if( i != m_Players.end( ) - 1 )
					{
						Froms += ", ";
						Froms2 += ", ";
					}
				}
				Froms2 += " "+ tr("lang_1181", CNL);  // " are all from ("+CNL+")";

				if (samecountry)
					SendAllChat( Froms2 );
				else
					SendAllChat( Froms );
			}

			//
			// !REALM
			// !R
			//
			else if( ( Command == "realm" || Command == "r") )
			{
				string Froms;
				string Froms2;
				string CNL;
				string CN;
				bool samerealm=true;
				if(!Payload.empty())
				{
					CGamePlayer *LastMatch = NULL;
					uint32_t Matches = GetPlayerFromNamePartial( Payload , &LastMatch );

					if( Matches == 0 )
						CONSOLE_Print("No matches");

					else if( Matches == 1 )
					{
						Froms = LastMatch->GetName( );
						Froms += ": (";
						CN = LastMatch->GetShortenedRealm();
						Froms += CN;
						Froms += ")";
						SendAllChat(Froms);
					}
					else
						CONSOLE_Print("Found more than one match");
					return HideCommand;
				}
				
				for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
				{
					// we reverse the byte order on the IP because it's stored in network byte order

					Froms2 += (*i)->GetName( );
					Froms += (*i)->GetName( );
					Froms += ": (";
					CN = (*i)->GetShortenedRealm();
//					CN = m_GHost->m_DB->FromCheck( UTIL_ByteArrayToUInt32( (*i)->GetExternalIP( ), true ) );
					Froms += CN;
					Froms += ")";

					if (CNL=="")
						CNL=CN;
					else
					if (CN!=CNL)
						samerealm=false;

					if( i != m_Players.end( ) - 1 )
					{
						Froms += ", ";
						Froms2 += ", ";
					}
				}
				Froms2 += " "+ tr("lang_1181", CNL);  // " are all from ("+CNL+")";

				if (samerealm)
					SendAllChat( Froms2 );
				else
					SendAllChat( Froms );
			}

			//
			// !SILENCE
			//

			else if( (Command == "silence" || Command == "sil") )
			{
				player->SetSilence(!player->GetSilence());
				if (player->GetSilence())
					SendChat(player->GetPID(), tr("lang_1182")); // Silence ON
				else
					SendChat(player->GetPID(), tr("lang_1183")); // Silence OFF
			}

			//
			// !UNMUTE
			// !UM
			//

			else if( (Command == "unmute" || Command =="um") && !Payload.empty( ) )
			{
				if (!CMDCheck(CMD_mute, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}

				CGamePlayer *LastMatch = NULL;
				uint32_t Matches = GetPlayerFromNamePartial( Payload, &LastMatch );

				if( Matches == 0 )
					SendChat( player,  tr("lang_1131"));  // "no match found!"
				else if( Matches == 1 )
				{
					string MuteName = LastMatch->GetName();
					if (IsMuted(MuteName))
					{
						uint32_t sec = IsCensorMuted(MuteName);
						if (sec>0)
						{
							SendChat ( player->GetPID(), tr("lang_1184", "$USER$", MuteName, "$TIME$", UTIL_ToString(sec))); // MuteName + " is censor muted for "+UTIL_ToString(sec)+ " more seconds"
							return HideCommand;
						}
						SendAllChat( tr("lang_0996", MuteName ) ); // RemovedPlayerFromTheMuteList
						DelFromMuted( MuteName);
					}
					else
						SendChat ( player->GetPID(), tr("lang_1185", MuteName ) ); //  MuteName + " is not muted"
				}
//				else // Unable to unmute, more than one match found
			}

			//
			// !MUTE
			// !M
			//

			else if( ( Command == "mute" || Command == "m" ) && !Payload.empty( ) )
			{
				if (!CMDCheck(CMD_mute, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}
				CGamePlayer *LastMatch = NULL;
				uint32_t Matches = GetPlayerFromNamePartial( Payload, &LastMatch );

				if( Matches == 0 )
					SendChat( player, tr("lang_1131") ); // "no match found!"
				else if( Matches == 1 )
				{
					bool isAdmin = IsOwner(LastMatch->GetName());
					for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
					{
						if( (*j)->IsAdmin(LastMatch->GetName() ) || (*j)->IsRootAdmin( LastMatch->GetName() ) )
						{
							isAdmin = true;
							break;
						}
					}

					if (isAdmin && !RootAdminCheck)
					{
						SendChat( player->GetPID(), tr("lang_1186")); // You can't mute an admin!
						return HideCommand;
					}

					string MuteName = LastMatch->GetName();
					if (IsMuted(MuteName))
						SendChat ( player->GetPID(), tr("lang_1187", MuteName)); // is already muted
					else
					{
						SendAllChat( tr("lang_0997", MuteName ) ); // AddedPlayerToTheMuteList
						AddToMuted( MuteName);
					}
				}
//				else // Unable to mute, more than one match found
			}

			//
			// !HCL
			//

			else if( (Command == "hcl" || Command == "mode" || Command == "gamemode") && !m_CountDownStarted )
			{

				if (m_GHost->m_onlyownerscanstart && !Payload.empty( ))
					if ((!IsOwner( User) && GetPlayerFromName(m_OwnerName, false)) && !RootAdminCheck )
					{
						SendChat( player->GetPID(), tr("lang_1147")); // Only the owner can change HCL.
						return HideCommand;
					}

				if (BluePlayer && AdminAccess==m_GHost->m_OwnerAccess)
					if (!m_GHost->m_BlueCanHCL)
						return HideCommand;
				if( !Payload.empty( ) )
				{
					if( Payload.size( ) <= m_Slots.size( ) )
					{
						string HCLChars = "abcdefghijklmnopqrstuvwxyz0123456789 -=,.";

						if( Payload.find_first_not_of( HCLChars ) == string :: npos )
						{
							m_HCLCommandString = Payload;
							SendAllChat( tr( "lang_0182", m_HCLCommandString ) ); // SettingHCL
						}
						else
							SendAllChat( tr("lang_0183") ); // UnableToSetHCLInvalid
					}
					else
						SendAllChat( tr("lang_0184")); // UnableToSetHCLTooLong
				}
				else
					SendAllChat( tr("lang_0185", m_HCLCommandString ) );  // TheHCLIs( m_HCLCommandString )
			}

			//
			// !HOLDS (hold a specified slot for someone)
			//

			else if( Command == "holds" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded )
			{
				// hold as many players as specified, e.g. "Varlock 2 Kilranin 4" holds players "Varlock" and "Kilranin"

				stringstream SS;
				SS << Payload;

				while( !SS.eof( ) )
				{
					string HoldName;
					SS >> HoldName;

					if( SS.fail( ) )
					{
						CONSOLE_Print( "[GAME: " + m_GameName + "] bad input to holds command" );
						break;
					}
					else
					{
						uint32_t HoldNumber;
						unsigned char HoldNr;
						SS >> HoldNumber;
						if ( SS.fail ( ) )
						{
							CONSOLE_Print( "[GAME: " + m_GameName + "] bad input to holds command" );
							break;
						}
						else
						{
							if (HoldNumber>m_Slots.size())
								HoldNumber = m_Slots.size();
							HoldNr=(unsigned char)( HoldNumber - 1 );
							SendAllChat( tr( "lang_0054", HoldName ) ); // AddedPlayerToTheHoldList
							AddToReserved( HoldName, HoldNr );
						}
					}
				}
			}

			//
			// !HOLD (hold a slot for someone)
			//

			else if( Command == "hold" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded )
			{
				// hold as many players as specified, e.g. "Varlock Kilranin" holds players "Varlock" and "Kilranin"

				stringstream SS;
				SS << Payload;

				while( !SS.eof( ) )
				{
					string HoldName;
					SS >> HoldName;

					if( SS.fail( ) )
					{
						CONSOLE_Print( "[GAME: " + m_GameName + "] bad input to hold command" );
						break;
					}
					else
					{
						SendAllChat( tr( "lang_0054", HoldName ) ); // AddedPlayerToTheHoldList
						AddToReserved( HoldName, 255 );
					}
				}
			}

			//
			// !UNHOLD (unhold a slot for someone)
			//

			else if( Command == "unhold" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded )
			{
				// unhold as many players as specified, e.g. "Varlock Kilranin" unholds players "Varlock" and "Kilranin"

				stringstream SS;
				SS << Payload;

				while( !SS.eof( ) )
				{
					string HoldName;
					SS >> HoldName;

					if( SS.fail( ) )
					{
						CONSOLE_Print( "[GAME: " + m_GameName + "] bad input to hold command" );
						break;
					}
					else
					{
						SendAllChat( tr( "lang_1188", HoldName ) ); // "Removed " + HoldName + " from hold list"
						DelFromReserved( HoldName);
					}
				}
			}

			//
			// !KICK (kick a player)
			// !K
			//

			else if( ( Command == "kick" || Command == "k" ) && !Payload.empty( ) )
			{
				if (!CMDCheck(CMD_kick, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}
				CGamePlayer *LastMatch = NULL;
				uint32_t Matches = GetPlayerFromNamePartial( Payload, &LastMatch );

				if( Matches == 0 )
					SendChat( player->GetPID(), tr("lang_0055", Payload ) ); // UnableToKickNoMatchesFound
				else if( Matches == 1 )
				{
					bool isAdmin = IsOwner( LastMatch->GetName() );
					bool isRootAdmin = IsOwner( LastMatch->GetName() );
					for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
					{
						if((*j)->IsAdmin(LastMatch->GetName()) || (*j)->IsRootAdmin( LastMatch->GetName() ) )
						{
							isAdmin = true;
						}
						if( (*j)->IsRootAdmin( LastMatch->GetName() ) )
						{
							isRootAdmin = true;
						}
					}

	// admins can't kick another admin
					if (isAdmin && !RootAdminCheck)
					{
						SendChat( player->GetPID(),  tr("lang_1160")); // "You can't kick an admin!"
						return HideCommand;
					}


	// nobody can kick a rootadmin
					if (isRootAdmin)
					{
						SendChat( player->GetPID(), tr("lang_1190"));  // You can't kick an owner or rootadmin!
						return HideCommand;
					}

					LastMatch->SetDeleteMe( true );
					LastMatch->SetLeftReason( tr( "lang_0078", User ) );  // WasKickedByPlayer

					if( !m_GameLoading && !m_GameLoaded )
						LastMatch->SetLeftCode( PLAYERLEAVE_LOBBY );
					else
						LastMatch->SetLeftCode( PLAYERLEAVE_LOST );

					if( !m_GameLoading && !m_GameLoaded )
						OpenSlot( GetSIDFromPID( LastMatch->GetPID( ) ), false );
				}
				else
					SendAllChat( tr( "lang_0056", Payload ) ); // UnableToKickFoundMoreThanOneMatch
			}

			//
			// !DRD (turn dynamic latency on or off)
			//

			else if( ( Command == "drd" || Command == "dlatency" || Command == "ddr") )
			{
				if (Payload.empty())
				{
					if (m_UseDynamicLatency)
						SendAllChat( tr( "lang_1191") ); // Dynamic latency disabled
					else
						SendAllChat( tr( "lang_1192") ); // Dynamic latency enabled
					m_UseDynamicLatency = !m_UseDynamicLatency;

					return HideCommand;
				}
				if( Payload == "on" )
				{
					SendAllChat( tr( "lang_1192") ); // "Dynamic latency enabled"
					m_UseDynamicLatency = true;
				}
				else if( Payload == "off" )
				{
					SendAllChat( tr( "lang_1191") ); // "Dynamic latency disabled"
					m_UseDynamicLatency = false;
				}
			}

			//
			// !LATENCY (set game latency)
			//

			else if( ( Command == "latency" || Command == "dr") )
			{
				if( Payload.empty( ) )
				{
					if (m_UseDynamicLatency)
						SendAllChat( tr( "lang_1193", "$LATENCY$", UTIL_ToString( m_Latency ), "$DLATENCY$", UTIL_ToString( m_DynamicLatency ))); // "Latency set at [" + UTIL_ToString( m_Latency ) + "], dynamic latency is ["+ UTIL_ToString( m_DynamicLatency )+"]"
					else
						SendAllChat( tr( "lang_0097", UTIL_ToString( m_Latency ) ) ); // LatencyIs
				}
				else
				{
					m_Latency = UTIL_ToUInt32( Payload );

					uint32_t minLatency = 20;
					if (!m_GHost->m_newLatency)
						minLatency = 50;

					if( m_Latency <= minLatency )
					{
						m_Latency = minLatency;
						SendAllChat( tr( "lang_0057", UTIL_ToString(minLatency) ) ); // SettingLatencyToMinimum
					}
					else if( m_Latency >= 500 )
					{
						m_Latency = 500;
						SendAllChat( tr( "lang_0058", "500" ) ); // SettingLatencyToMaximum
					}
					else
						SendAllChat( tr( "lang_0059", UTIL_ToString( m_Latency ) ) ); // SettingLatencyTo
				}
			}

			//
			// !LOCK
            //

			else if( Command == "lock" && ( RootAdminCheck || IsOwner( User ) ) )
			{
				SendAllChat( tr("lang_0116") ); // GameLocked
				m_Locked = true;
			}

			if ( Command == "unlock" && RootAdminCheck)
			{
                SendAllChat( tr("lang_0117") ); // GameUnlocked
				m_Locked = false;
			}

			//
			// !MESSAGES
			//

			else if( Command == "messages" )
			{
				if( Payload == "on" )
				{
					SendAllChat( tr("lang_0200") ); // LocalAdminMessagesEnabled
					m_LocalAdminMessages = true;
				}
				else if( Payload == "off" )
				{
					SendAllChat( tr("lang_0201") ); // LocalAdminMessagesDisabled
					m_LocalAdminMessages = false;
				}
			}

			//
			// !MUTEALL
			//

			else if( Command == "muteall" && m_GameLoaded )
			{
				SendAllChat( tr("lang_0085") ); // GlobalChatMuted
				m_MuteAll = true;
			}

			//
			// !OPEN (open slot)
			//

			else if( Command == "open" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded )
			{
				if (!CMDCheck(CMD_open, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}

				// open as many slots as specified, e.g. "5 10" opens slots 5 and 10

				stringstream SS;
				SS << Payload;

				while( !SS.eof( ) )
				{
					uint32_t SID;
					SS >> SID;

					if( SS.fail( ) )
					{
						CONSOLE_Print( "[GAME: " + m_GameName + "] bad input to open command" );
						break;
					}
					else
					{
						bool isAdmin = false;
						bool isRootAdmin = false;
						CGamePlayer *Player = GetPlayerFromSID( SID - 1 );
						if (Player)
						{
							if (IsAdmin(Player->GetName()) || IsRootAdmin(Player->GetName()))
								isAdmin = true;
							if (IsRootAdmin(Player->GetName()))
								isRootAdmin = true;
						}
						if (isRootAdmin)
						{
							SendChat( player->GetPID(), tr("lang_1194") ); // You can't kick a rootadmin!
							return HideCommand;
						}
						if (isAdmin && !(IsOwner(User) || RootAdminCheck))
						{
							SendChat( player->GetPID(), tr("lang_1160") ); // You can't kick an admin!
							return HideCommand;
						} else
						OpenSlot( (unsigned char)( SID - 1 ), true );
					}
				}
			}

			//
			// !OPENALL
			//

			else if( Command == "openall" && !m_GameLoading && !m_GameLoaded )
			{
				if (!CMDCheck(CMD_open, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}

				OpenAllSlots( );
			}

			//
			// !OWNER (set game owner)
			//

			else if( Command == "owner" )
			{
				if( RootAdminCheck || IsOwner( User ) || (!GetPlayerFromName( m_OwnerName, false ) && m_OwnerJoined))
				{
					if( !Payload.empty( ) )
					{
						string sUser = Payload;
						CGamePlayer *LastMatch = NULL;
						uint32_t Matches = GetPlayerFromNamePartial( Payload , &LastMatch );
						if (Matches == 1)
							sUser = LastMatch->GetName();
						SendAllChat( tr("lang_0114", sUser ) ); // SettingGameOwnerTo
						m_OwnerName = sUser;
					}
					else
					{
						SendAllChat( tr("lang_0114", User ) ); // SettingGameOwnerTo
						m_OwnerName = User;
					}
				}
				else
					SendAllChat( tr( "lang_0120", m_OwnerName ) ); // UnableToSetGameOwner
			}
			
			//
			// !PING
			//

			else if( Command == "ping" )
			SendChat( player,  player->GetNumPings( ) > 0 ? UTIL_ToString( player->GetPing( m_GHost->m_LCPings ) ) + tr("lang_1017") : tr("lang_1018")); // "ms" : "N/A"

			//
			// !PINGALL
			// 

			else if( Command == "pingall" )
			{
				// kick players with ping higher than payload if payload isn't empty
				// we only do this if the game hasn't started since we don't want to kick players from a game in progress

				uint32_t Kicked = 0;
				uint32_t KickPing = 0;
				string Pings;
				string CN = string();

				if (!Payload.empty())
				{
					CGamePlayer *LastMatch = NULL;
					uint32_t Matches = GetPlayerFromNamePartial( Payload , &LastMatch );

					if( Matches == 0 )
						CONSOLE_Print("No matches");

					else if( Matches == 1 )
					{
						Pings = LastMatch->GetName( );
						Pings +=": ";
						if( LastMatch->GetNumPings( ) > 0 )
						{
							Pings += UTIL_ToString( LastMatch->GetPing( m_GHost->m_LCPings ) );
							Pings += " "+tr("lang_1017"); //" ms";
						} else
							Pings += " "+tr("lang_1018"); // N/A";

						Pings += " (";
						CN = LastMatch->GetCountry();
						Pings += CN;
						Pings += ")";
						SendAllChat(Pings);
						return HideCommand;
					}
					else
						CONSOLE_Print("Found more than one match");
				}

				if( !m_GameLoading && !m_GameLoaded && !Payload.empty( ) )
					KickPing = UTIL_ToUInt32( Payload );

				// copy the m_Players vector so we can sort by descending ping so it's easier to find players with high pings

				vector<CGamePlayer *> SortedPlayers = m_Players;
				sort( SortedPlayers.begin( ), SortedPlayers.end( ), CGamePlayerSortDescByPing( ) );

				for( vector<CGamePlayer *> :: iterator i = SortedPlayers.begin( ); i != SortedPlayers.end( ); i++ )
				{
					Pings += (*i)->GetNameTerminated( );
					Pings += ": ";

					if( (*i)->GetNumPings( ) > 0 )
					{
						Pings += UTIL_ToString( (*i)->GetPing( m_GHost->m_LCPings ) );

						if( !m_GameLoading && !m_GameLoaded && !(*i)->GetReserved( ) && KickPing > 0 && (*i)->GetPing( m_GHost->m_LCPings ) > KickPing )
						{
							(*i)->SetDeleteMe( true );
							(*i)->SetLeftReason( tr("lang_1189", "$PING$", UTIL_ToString( (*i)->GetPing( m_GHost->m_LCPings ) ), "$PINGMAX$", UTIL_ToString( KickPing ) )); // "was kicked for excessive ping " + UTIL_ToString( (*i)->GetPing( m_GHost->m_LCPings ) ) + " > " + UTIL_ToString( KickPing )
							(*i)->SetLeftCode( PLAYERLEAVE_LOBBY );
							OpenSlot( GetSIDFromPID( (*i)->GetPID( ) ), false );
							Kicked++;
						}

						Pings += tr("lang_1017"); //  "ms"
					}
					else
						Pings += tr("lang_1018"); // "N/A";

					if( i != SortedPlayers.end( ) - 1 )
						Pings += ", ";
				}

				SendAllChat( Pings );

				if( Kicked > 0 )
					SendAllChat( tr("lang_0060", "$TOTAL$", UTIL_ToString( Kicked ), "$PING$", UTIL_ToString( KickPing ) ) ); // KickingPlayersWithPingsGreaterThan
				return HideCommand;
			}

			//
			// !PRIV (rehost as private game)
			// !PRI
			//

			else if( (Command == "priv" || Command == "pri") && !Payload.empty( ) && !m_CountDownStarted && !m_SaveGame )
			{
				if (!CMDCheck(CMD_host, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}

				if (m_GHost->m_onlyownerscanstart && !Payload.empty())
					if ((!IsOwner( User) && GetPlayerFromName(m_OwnerName, false)) && !RootAdminCheck )
					{
						SendChat( player->GetPID(), tr("lang_1048")); // "Only the owner can change the gamename."
						return HideCommand;
					}

			if( Payload.length() < 31 )
			{
				if (Payload.size()>29)
					Payload = Payload.substr(0,29);

				string GameName = Payload;
				string GameNr = string();
				uint32_t idx = 0;
				uint32_t Nr = 0;
				if (!GameName.empty() && GameName==m_GameName)
				{
					SendAllChat( tr("lang_1047")); // "You can't rehost with the same name"
						return HideCommand;
				}
				if (GameName.empty())
				{
					GameName = m_GameName;

					idx = GameName.length()-1;
					if (idx>=2)
					if (GameName.at(idx-2)=='#')
						idx = idx-1;
					else
						if (GameName.at(idx-1)=='#')
							idx = idx;
						else
							idx = 0;

					// idx = 0, no Game Nr found in gamename
					if (idx == 0)
					{
						GameNr = "0";
						GameName = m_GameName + " #";
					}
					else
					{
						GameNr = GameName.substr(idx,GameName.length()-idx);
						GameName = GameName.substr(0,idx);
					}
					stringstream SS;
					SS << GameNr;
					SS >> Nr;
					Nr ++;
					if (Nr>20)
						Nr = 1;
					GameNr = UTIL_ToString(Nr);
					GameName = GameName + GameNr;
				}
				string s;
				if (m_GameState == GAME_PRIVATE)
					s = tr("lang_1051"); // "private";
				else
					s = tr("lang_1052"); // "public";

				if ( Command == "pri" )
				{
					m_GameState = m_GHost->m_gamestateinhouse;
					CONSOLE_Print( "[GAME: " + m_GameName + "] trying to rehost as inhouse game [" + GameName + "]" );
				}
				else
				{
					m_GameState = GAME_PRIVATE;
					CONSOLE_Print( "[GAME: " + m_GameName + "] trying to rehost as private game [" + GameName + "]" );
				}

				m_GameName = GameName;
				AutoSetHCL();
				AddGameName(GameName);
				m_GHost->m_HostCounter++;
				m_GHost->SaveHostCounter();
				if (m_GHost->m_MaxHostCounter>0)
				if (m_GHost->m_HostCounter>m_GHost->m_MaxHostCounter)
					m_GHost->m_HostCounter = 1;
				m_HostCounter = m_GHost->m_HostCounter;
				m_RefreshError = false;
				m_GHost->m_QuietRehost = true;
				m_Rehost = true;
//				m_GHost->UDPChatSend("|rehost");
				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
				{
					// unqueue any existing game refreshes because we're going to assume the next successful game refresh indicates that the rehost worked
					// this ignores the fact that it's possible a game refresh was just sent and no response has been received yet
					// we assume this won't happen very often since the only downside is a potential false positive

					(*i)->UnqueueGameRefreshes( );
					(*i)->QueueGameUncreate( );
					(*i)->QueueEnterChat( );

					// we need to send the game creation message now because private games are not refreshed
					MILLISLEEP(5);
					(*i)->QueueGameCreate( m_GameState, m_GameName, string( ), m_Map.get(), NULL, m_HostCounter );

					if( (*i)->GetPasswordHashType( ) != "pvpgn" )
						(*i)->QueueEnterChat( );
				}

				m_CreationTime = GetTime( );
				m_LastRefreshTime = GetTime( );
			} else
				SendAllChat( tr("lang_0113", Payload ) );
			}

			// !REHOST

			else if ( Command == "rehost" && !m_CountDownStarted && !m_SaveGame )
			{
				if (!CMDCheck(CMD_host, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}

			if( Payload.length() < 31 )
			{
					string GameName = Payload;
					string GameNr = string();
					uint32_t idx = 0;
					uint32_t Nr = 0;
					if (!GameName.empty() && GameName==m_GameName)
					{
						SendAllChat(tr("lang_1047")); // "You can't rehost with the same name"
							return HideCommand;
					}
					if (GameName.empty())
					{
						GameName = m_GameName;

						idx = GameName.length()-1;
						if (idx>=2)
						if (GameName.at(idx-2)=='#')
							idx = idx-1;
						else
						if (GameName.at(idx-1)=='#')
							idx = idx;
						else
							idx = 0;

		// idx = 0, no Game Nr found in gamename
						if (idx == 0)
						{
							GameNr = "0";
							GameName = m_GameName + " #";
						}
						else
						{
							GameNr = GameName.substr(idx,GameName.length()-idx);
							GameName = GameName.substr(0,idx);
						}
						stringstream SS;
						SS << GameNr;
						SS >> Nr;
						Nr ++;
						if (Nr>20)
							Nr = 1;
						GameNr = UTIL_ToString(Nr);
						GameName = GameName + GameNr;
					}
					string s;
					if (m_GameState == GAME_PRIVATE)
						s = tr("lang_1051"); // "private";
					else
						s = tr("lang_1052"); // "public";
					CONSOLE_Print( "[GAME: " + m_GameName + "] trying to rehost as "+s+" game [" + GameName + "]" );

					SendAllChat( tr("lang_1049")); //  "Rehosting in 3 seconds ... "
					SendAllChat( tr("lang_1050", "$TYPE$", s, "$NAME$", GameName)); // "Please join the new "+s+" game \""+GameName+"\""

					m_EndGameTime = GetTime();
	//				m_Exiting = true;
	//				m_GHost->newGame = true;
					m_GHost->newGameCountry2Check = m_CountryCheck2;
					m_GHost->newGameCountryCheck = m_CountryCheck;
					m_GHost->newGameProvidersCheck = m_ProviderCheck;
					m_GHost->newGameProviders2Check = m_ProviderCheck2;
					m_GHost->newGameCountries = m_Countries;
					m_GHost->newGameCountries2 = m_Countries2;
					m_GHost->newGameProviders = m_Providers;
					m_GHost->newGameProviders2 = m_Providers2;
					m_GHost->newGameGameState = m_GameState;
					m_GHost->newGameServer = m_Server;
					m_GHost->newGameUser = User;
					m_GHost->newGameName = GameName;
					m_GHost->newGameGArena = m_GarenaOnly;

				} else
					SendAllChat( tr("lang_0113", Payload ) );
			}

			//
			// !PUB (rehost as public game)
			//

			else if( Command == "pub" && !m_CountDownStarted && !m_SaveGame )
			{
				if (!CMDCheck(CMD_host, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}

				if (m_GHost->m_onlyownerscanstart && !Payload.empty())
					if ((!IsOwner( User) && GetPlayerFromName(m_OwnerName, false)) && !RootAdminCheck )
					{
						SendChat( player->GetPID(), tr("lang_1048")); // Only the owner can change the gamename.
						return HideCommand;
					}

				if( Payload.length() < 31 )
				{
					if (Payload.size()>29)
						Payload = Payload.substr(0,29);

					string GameName = Payload;
					string GameNr = string();
					uint32_t idx = 0;
					uint32_t Nr = 0;
					if (!GameName.empty() && GameName==m_GameName)
					{
						SendAllChat(tr("lang_1047")); // You can't rehost with the same name
						return HideCommand;
					}
					if (GameName.empty())
					{
						GameName = m_GameName;

						idx = GameName.length()-1;
						if (idx>=2)
						if (GameName.at(idx-2)=='#')
							idx = idx-1;
						else
							if (GameName.at(idx-1)=='#')
								idx = idx;
							else
								idx = 0;

						// idx = 0, no Game Nr found in gamename
						if (idx == 0)
						{
							GameNr = "0";
							GameName = m_GameName + " #";
						}
						else
						{
							GameNr = GameName.substr(idx,GameName.length()-idx);
							GameName = GameName.substr(0,idx);
						}
						stringstream SS;
						SS << GameNr;
						SS >> Nr;
						Nr ++;
						if (Nr>20)
							Nr = 1;
						GameNr = UTIL_ToString(Nr);
						GameName = GameName + GameNr;
					}
					string s;
					if (m_GameState == GAME_PRIVATE)
						s = tr("lang_1051"); // "private";
					else
						s = tr("lang_1052"); // "public";

					CONSOLE_Print( "[GAME: " + m_GameName + "] trying to rehost as public game [" + GameName + "]" );
					m_GameState = GAME_PUBLIC;
					m_GameName = GameName;
					m_GHost->m_HostCounter++;
					m_GHost->SaveHostCounter();
					if (m_GHost->m_MaxHostCounter>0)
					if (m_GHost->m_HostCounter>m_GHost->m_MaxHostCounter)
						m_GHost->m_HostCounter = 1;
					m_HostCounter = m_GHost->m_HostCounter;
					m_GHost->m_QuietRehost = true;
					m_RefreshError = false;
					m_Rehost = true;
					AutoSetHCL();
					AddGameName(GameName);
	//				m_GHost->UDPChatSend("|rehost "+GameName);

					//SendAllChat("Rehosting ...");
					for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
					{
						// unqueue any existing game refreshes because we're going to assume the next successful game refresh indicates that the rehost worked
						// this ignores the fact that it's possible a game refresh was just sent and no response has been received yet
						// we assume this won't happen very often since the only downside is a potential false positive

						(*i)->UnqueueGameRefreshes( );
						(*i)->QueueGameUncreate( );
						(*i)->QueueEnterChat( );

						// the game creation message will be sent on the next refresh
					}

					m_CreationTime = GetTime( );
					m_LastRefreshTime = GetTime( );
				} else
					SendAllChat( tr("lang_0113", Payload ) );

			}

			//
			// !REFRESH (turn on or off refresh messages)
			//

			else if( Command == "refresh" && !m_CountDownStarted )
			{
				if( Payload == "on" )
				{
					SendAllChat( tr("lang_0090") ); // RefreshMessagesEnabled
					m_RefreshMessages = true;
				}
				else if( Payload == "off" )
				{
					SendAllChat( tr("lang_0091") ); // RefreshMessagesDisabled
					m_RefreshMessages = false;
				}
			}

			//
			// !SAY
			//

			else if( Command == "say" && !Payload.empty( ) && RootAdminCheck )
			{
				if (!CMDCheck(CMD_say, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}

				for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
					(*i)->QueueChatCommand( Payload );

				HideCommand = true;
			}

			//
			// !SENDLAN
			//

			else if( Command == "sendlan" && !Payload.empty( ) && !m_CountDownStarted )
			{
				// extract the ip and the port
				// e.g. "1.2.3.4 6112" -> ip: "1.2.3.4", port: "6112"

				string IP;
				uint32_t Port = 6112;
				stringstream SS;
				SS << Payload;
				SS >> IP;

				if( !SS.eof( ) )
					SS >> Port;

				if( SS.fail( ) )
					CONSOLE_Print( "[GAME: " + m_GameName + "] bad inputs to sendlan command" );
				else
				{
					// we send 12 for SlotsTotal because this determines how many PID's Warcraft 3 allocates
					// we need to make sure Warcraft 3 allocates at least SlotsTotal + 1 but at most 12 PID's
					// this is because we need an extra PID for the virtual host player (but we always delete the virtual host player when the 12th person joins)
					// however, we can't send 13 for SlotsTotal because this causes Warcraft 3 to crash when sharing control of units
					// nor can we send SlotsTotal because then Warcraft 3 crashes when playing maps with less than 12 PID's (because of the virtual host player taking an extra PID)
					// we also send 12 for SlotsOpen because Warcraft 3 assumes there's always at least one player in the game (the host)
					// so if we try to send accurate numbers it'll always be off by one and results in Warcraft 3 assuming the game is full when it still needs one more player
					// the easiest solution is to simply send 12 for both so the game will always show up as (1/12) players

			uint32_t slotstotal = m_Slots.size( );
			uint32_t slotsopen = GetSlotsOpen();
			if (slotsopen<2) slotsopen = 2;
			if(slotstotal > 12) slotstotal = 12;

					// construct the correct W3GS_GAMEINFO packet

					if( m_SaveGame )
					{
						// note: the PrivateGame flag is not set when broadcasting to LAN (as you might expect)
						uint32_t MapGameType = MAPGAMETYPE_SAVEDGAME;

						BYTEARRAY MapWidth;
						MapWidth.push_back( 0 );
						MapWidth.push_back( 0 );
						BYTEARRAY MapHeight;
						MapHeight.push_back( 0 );
						MapHeight.push_back( 0 );

						m_GHost->m_UDPSocket->SendTo( IP, Port, m_Protocol->SEND_W3GS_GAMEINFO( m_GHost->m_TFT, m_GHost->m_LANWar3Version, UTIL_CreateByteArray( MapGameType, false ), m_Map->GetMapGameFlags( ), MapWidth, MapHeight, m_GameName, m_CreatorName, GetTime( ) - m_CreationTime, "Save\\Multiplayer\\" + m_SaveGame->GetFileNameNoPath( ), m_SaveGame->GetMagicNumber( ), slotstotal, slotsopen, m_HostPort, m_HostCounter ) );
					}
					else
					{
						uint32_t MapGameType = MAPGAMETYPE_UNKNOWN0;

						m_GHost->m_UDPSocket->SendTo( IP, Port, m_Protocol->SEND_W3GS_GAMEINFO( m_GHost->m_TFT, m_GHost->m_LANWar3Version, UTIL_CreateByteArray( MapGameType, false ), m_Map->GetMapGameFlags( ), m_Map->GetMapWidth( ), m_Map->GetMapHeight( ), m_GameName, m_CreatorName, GetTime( ) - m_CreationTime, m_Map->GetMapPath( ), m_Map->GetMapCRC( ), slotstotal, slotsopen, m_HostPort, m_HostCounter ) );
					}
				}
			}
/*
			//
			// !Marsauto
			//

			else if( Command == "marsauto" )
			{
				string msg = "Auto insult ";
				m_GHost->m_autoinsultlobby = !m_GHost->m_autoinsultlobby;
				if (m_GHost->m_autoinsultlobby)
					msg += "ON";
				else
					msg += "OFF";

				SendChat(player->GetPID(), msg );
				return HideCommand;
			}

			//
			// !Mars
			//

			else if( Command == "mars" && (GetTime()-m_LastMars>=10) )
			{
				if (m_GHost->m_Mars.size()==0)
					return HideCommand;

				srand((unsigned)time(0));

				vector<string> randoms; randoms.clear();
				string nam2 = Payload;
				string nam1 = User;
				// if no user is specified, randomize one != with the user giving the command
				if (Payload.empty())
				{
					for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
					{
						if ((*i)->GetName()!=nam1)
							randoms.push_back((*i)->GetName());
					}
					random_shuffle(randoms.begin(), randoms.end());
					// if no user has been randomized, return
					if (randoms.size()>0)
						nam2 = randoms[0];
					else
						return HideCommand;
					randoms.clear();
				} else
				{
					CGamePlayer *LastMatch = NULL;
					uint32_t Matches = GetPlayerFromNamePartial( Payload , &LastMatch );
					if (Matches !=1)
						return HideCommand;
					nam2 = LastMatch->GetName();
				}
				string srv = m_Server;
				bool safe1 = false;
				bool safe2 = false;
				bool rootadmin = false;
				safe1 = IsSafe(nam1) || AdminCheck || RootAdminCheck;
				safe2 = IsSafe(nam2) || IsAdmin(nam2) || IsRootAdmin(nam2);
				rootadmin = IsRootAdmin(nam2);
				if (rootadmin)
					return HideCommand;
//				if (((safe1 && safe2) || Payload.empty()) || RootAdminCheck)
				if (((safe1) || Payload.empty()) || RootAdminCheck)
				{
					m_LastMars = GetTime();
					string msg = m_GHost->GetMars();

					randoms.push_back(m_GHost->m_RootAdmin);
					if (!m_GHost->m_VirtualHostName.empty())
						randoms.push_back(m_GHost->m_VirtualHostName);
					for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
					{
						if ((*i)->GetName()!=nam2 && (*i)->GetName()!=nam1)
							randoms.push_back((*i)->GetName());
					}
					random_shuffle(randoms.begin(), randoms.end());

					while (msg.find("$VICTIM$") != string :: npos)
						UTIL_Replace( msg, "$VICTIM$", nam2 );       

					while (msg.find("$RANDOM$") != string :: npos)
						UTIL_Replace( msg, "$RANDOM$", randoms[ rand() % randoms.size() ] ); 

					while (msg.find("$USER$") != string :: npos)
						UTIL_Replace( msg, "$USER$", nam1 );        

					while (msg.find("$RANDNUMBER$") != string ::npos)
					{
						int RandomNumber;
						RandomNumber = (rand()%99)+1;

						UTIL_Replace( msg, "$RANDNUMBER$", UTIL_ToString(RandomNumber));
					}

					SendAllChat( msg );
				}
			}

			//
			// !SLAP
			//

			else if ( Command == "slap" && !Payload.empty( ) )
			{
				CGamePlayer *LastMatch = NULL;
				uint32_t Matches = GetPlayerFromNamePartial( Payload , &LastMatch );
				if (Matches !=1)
					return HideCommand;
				string Victim = LastMatch->GetName();


				string Object;
				stringstream SS;
				SS << Payload;
				SS >> Victim;
				if( !SS.eof( ) )
				{
					getline( SS, Object );
					string :: size_type Start = Object.find_first_not_of( " " );
					if( Start != string :: npos )
						Object = Object.substr( Start );
				}
				
				int RandomNumber = 5;
				srand((unsigned)time(0));
				RandomNumber = (rand()%8);
				if ( Victim != User )
				{
					if ( RandomNumber == 0 )
						SendAllChat( User + " slaps " + Victim + " with a large trout." );
					else if ( RandomNumber == 1 )
						SendAllChat( User + " slaps " + Victim + " with a pink Macintosh." );
					else if ( RandomNumber == 2 )
						SendAllChat( User + " throws a Playstation 3 at " + Victim + "." );
					else if ( RandomNumber == 3 )
						SendAllChat( User + " drives a car over " + Victim + "." );
					else if ( RandomNumber == 4 )
						SendAllChat( User + " steals " + Victim + "'s cookies. mwahahah!" );
					else if ( RandomNumber == 5 )
					{
						SendAllChat( User + " washes " + Victim + "'s car.  Oh, the irony!" );
					}
					else if ( RandomNumber == 6 )
						SendAllChat( User + " burns " + Victim + "'s house." );
					else if ( RandomNumber == 7 )
						SendAllChat( User + " finds " + Victim + "'s picture on uglypeople.com." );
				}
				else
				{
					if ( RandomNumber == 0 )
						SendAllChat( User + " slaps himself with a large trout." );
					else if ( RandomNumber == 1 )
						SendAllChat( User + " slaps himself with a pink Macintosh." );
					else if ( RandomNumber == 2 )
						SendAllChat( User + " throws a Playstation 3 at himself." );
					else if ( RandomNumber == 3 )
						SendAllChat( User + " drives a car over himself." );
					else if ( RandomNumber == 4 )
						SendAllChat( User + " steals his cookies. mwahahah!" );
					else if ( RandomNumber == 5 )
					{
						int Goatsex = rand();
						string s;
						stringstream out;
						out << Goatsex;
						s = out.str();
						SendAllChat( User + " searches yahoo.com for goatsex + " + User + ". " + s + " hits WEEE!" );
					}
					else if ( RandomNumber == 6 )
						SendAllChat( User + " burns his house." );
					else if ( RandomNumber == 7 )
						SendAllChat( User + " finds his picture on uglypeople.com." );
				}
			}

			//
			// !SP
			//

			else if( Command == "sp" && !m_CountDownStarted )
			{
				if (!CMDCheck(CMD_sp, AdminAccess))
				{
					SendChat(player->GetPID(), tr("lang_0005"));
					return HideCommand;
				}

				SendAllChat( tr("lang_0087") ); // ShufflingPlayers
				ShuffleSlots( );
			}
*/
			//
			// !safeimmune
			//

			else if( ( Command == "safeimmune" || Command =="si" || Command == "sk" ) )
			{
				string mess;
				if (!m_GHost->m_SafeLobbyImmunity)
				{
					m_GHost->m_SafeLobbyImmunity = true;
					mess = tr("lang_1195"); // "Safe are immune to lobby kicking"
				} else
				{
					m_GHost->m_SafeLobbyImmunity = false;
					mess = tr("lang_1196"); //"Safe are no longer immune to lobby kicking"
				}
				SendChat( player->GetPID(), mess);
			}

			//
			// !STARTN
			//

			else if( ( Command == "startn" && !m_CountDownStarted ) )
			{
				if (m_GHost->m_onlyownerscanstart)
				if ((!IsOwner( User) && GetPlayerFromName(m_OwnerName, false)) && !RootAdminCheck )
				{
					SendChat( player->GetPID(), tr("lang_1197")); // "Only the owner can start the game."
					return HideCommand;
				}

				// skip checks and start the game right now

				if (GetTicks()-m_LastLeaverTicks<1000)
				{
					SendAllChat( tr("lang_1198") ); // "Sure you want to start right now? Someone just left"
					return HideCommand;
				}

				m_CountDownStarted = true;
				m_CountDownCounter = 0;
				if (m_NormalCountdown)
				{
					m_CountDownCounter = 10;
					SendAll( m_Protocol->SEND_W3GS_COUNTDOWN_START( ));
				}
			}

			//
			// !START
			//

			else if( Command == "start" && !m_CountDownStarted )
			{
				if (m_GHost->m_onlyownerscanstart)
				if ((!IsOwner( User) && GetPlayerFromName(m_OwnerName, false)) && !RootAdminCheck )
				{
					SendChat( player->GetPID(), tr("lang_1197")); //"Only the owner can start the game."
					return HideCommand;
				}

				// if the player sent "!start force" skip the checks and start the countdown
				// otherwise check that the game is ready to start

				if( Payload == "force" )
					StartCountDown( true );
				else
				{
					if( GetTicks( ) - m_LastPlayerLeaveTicks >= 2000 )
						StartCountDown( false );
					else
						SendAllChat( tr("lang_0206") ); // CountDownAbortedSomeoneLeftRecently
				}
			}

			//
			// !SWAP (swap slots for admin)
			//

			else if( Command == "swap" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded )
			{
				uint32_t SID1;
				uint32_t SID2;
				stringstream SS;
				SS << Payload;
				SS >> SID1;

				if ( !SS.fail( ) && !SS.eof() )
				{
					if (!CMDCheck(CMD_swap, AdminAccess))
					{
						SendChat(player->GetPID(), tr("lang_0005"));
						return HideCommand;
					}

					if( SS.fail( ) )
						CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #1 to swap command" );
					else
					{
						if( SS.eof( ) )
							CONSOLE_Print( "[GAME: " + m_GameName + "] missing input #2 to swap command" );
						else
						{
							SS >> SID2;

							if( SS.fail( ) )
								CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #2 to swap command" );
							else
							{
								bool isAdmin = false;
								bool isRootAdmin = false;
								bool sameteam = false;
								if (SID1-1<m_Slots.size() && SID2-1<m_Slots.size())
									sameteam = m_Slots[SID1-1].GetTeam() == m_Slots[SID2-1].GetTeam();
								CGamePlayer *Player = GetPlayerFromSID( SID1 - 1 );
								CGamePlayer *Player2 = GetPlayerFromSID( SID2 - 1 );
								if (Player)
									if (Player->GetName()!=User)
										if (IsRootAdmin(Player->GetName()))
											isRootAdmin = true;
								if (Player2)
									if (Player2->GetName()!=User)
										if (IsRootAdmin(Player2->GetName()))
											isRootAdmin = true;
								if (m_GHost->m_onlyownerscanswapadmins && !sameteam)
								{
									CGamePlayer *Player = GetPlayerFromSID( SID1 - 1 );
									CGamePlayer *Player2 = GetPlayerFromSID( SID2 - 1 );
									if (Player)
									{
										if (IsAdmin(Player->GetName()) || IsRootAdmin(Player->GetName()))
										if (Player->GetName()!=User)
											isAdmin = true;
									}
									if (Player2)
									{
										if (IsAdmin(Player2->GetName()) || IsRootAdmin(Player2->GetName()))
										if (Player2->GetName()!=User)
											isAdmin = true;
									}
								}

								if (isRootAdmin)
								{
									SendChat( player->GetPID(), tr("lang_1199"));  // "You can't swap a rootadmin!"
									return HideCommand;
								}

								if (isAdmin && !(IsOwner(User) || RootAdminCheck))
								{
									SendChat( player->GetPID(), tr("lang_1200") ); // "You can't swap an admin!"
									return HideCommand;
								} else
								SwapSlots( (unsigned char)( SID1 - 1 ), (unsigned char)( SID2 - 1 ) );
							}
						}
					}
				}
			}

			//
			// !SYNCLIMIT
			//

			else if( ( Command == "synclimit" || Command == "s" ) )
			{
				if( Payload.empty( ) )
					SendAllChat( tr("lang_0098", UTIL_ToString( m_SyncLimit ) ) ); // SyncLimitIs
				else
				{
					m_SyncLimit = UTIL_ToUInt32( Payload );

					if( m_SyncLimit <= 10 )
					{
						m_SyncLimit = 10;
						SendAllChat( tr("lang_0099", "10" ) ); // SettingSyncLimitToMinimum
					}
					else if( m_SyncLimit >= 10000 )
					{
						m_SyncLimit = 10000;
						SendAllChat( tr("lang_0100", "10000" ) ); // SettingSyncLimitToMaximum
					}
					else
						SendAllChat( tr("lang_0101", UTIL_ToString( m_SyncLimit ) ) ); //  SettingSyncLimitTo
				}
			}

			//
			// !UNHOST
			//

			else if( Command == "unhost" && !m_CountDownStarted )
				m_Exiting = true;

			//
			// !UNLOCK
			//

			else if( Command == "unlock" && ( RootAdminCheck || IsOwner( User ) ) )
			{
				SendAllChat( tr("lang_0117") ); // GameUnlocked
				m_Locked = false;
			}

			//
			// !UNMUTEALL
			//

			else if( Command == "unmuteall" && m_GameLoaded )
			{
				SendAllChat( tr("lang_0086") ); // GlobalChatUnmuted
				m_MuteAll = false;
			}

			//
			// !DOWNLOADS
			//

			else if( Command == "downloads" )
			{
				m_GHost->m_AllowDownloads = !m_GHost->m_AllowDownloads;
				if (m_GHost->m_AllowDownloads)
					SendAllChat( tr("lang_1105"));  // "Downloads allowed"
				else
					SendAllChat( tr("lang_1106") );  // "Downloads denied"
			}

			//
			// !COMMANDS
			//

			else if( Command == "commands" )
			{
				m_GHost->m_NonAdminCommands = !m_GHost->m_NonAdminCommands;
				if (m_GHost->m_NonAdminCommands)
					SendAllChat( tr("lang_1093") );  // "Non-admin commands ON"
				else
					SendAllChat( tr("lang_1094") );  // "Non-admin commands OFF"
			}

			//
			// !OVERRIDE
			//

			else if( Command == "override" )
			{
				m_GameOverCanceled = !m_GameOverCanceled;
				if (m_GameOverCanceled)
				{
					SendAllChat( tr("lang_1201") ); // "Game over timer canceled"
					m_GameOverTime = 0;
				}
			}

			//
			// !ROLL
			//

			else if( Command == "roll" )
			{
				int RandomNumber;
				srand((unsigned)time(0));
				RandomNumber = (rand()%99)+1;
				SendAllChat( tr("lang_1202", "$USER$", User, "$NUMBER$", UTIL_ToString(RandomNumber)) ); // User + " rolled "+UTIL_ToString(RandomNumber)
			}

			//
			// !VERBOSE
			// !VB
			//

			else if( ( Command == "verbose" || Command == "vb" ) )
			{
				m_GHost->m_Verbose = !m_GHost->m_Verbose;
				if (m_GHost->m_Verbose)
					SendAllChat( tr("lang_1102")); // "Verbose ON"
				else
					SendAllChat( tr("lang_1103")); // "Verbose OFF"
			}

			//
			// !recalc
			//

			else if( Command == "recalc" )
			{
				ReCalculateTeams();
			}

			//
			// !GARENA
			//

			else if( Command == "garena" )
			{
				m_GarenaOnly = !m_GarenaOnly;
				if (m_GarenaOnly)
					SendAllChat( tr("lang_1203")); // "Garena only!"
				else
					SendAllChat( tr("lang_1204")); // "Unrestricted"
			}

			//
			// !NOGARENA
			// !NG
			//

			else if( ( Command == "nogarena" || Command == "ng" ) )
			{
				m_NoGarena = !m_NoGarena;
				if(m_NoGarena)
					SendAllChat( tr("lang_1500"));
				else
					SendAllChat( tr("lang_1501"));
			}

			//
			// !GN
			//

			else if( Command == "gn" )
			{
				SendAllChat( tr("lang_1205", m_GameName)); // "Current game name is \""+m_GameName+"\""
				return HideCommand;
			}

			//
			// !VIRTUALHOST
			//

			else if( Command == "virtualhost" && !Payload.empty( ) && Payload.size( ) <= 15 && !m_CountDownStarted )
			{
				DeleteVirtualHost( );
				m_VirtualHostName = Payload;
			}

			//
			// !RMK
			//

			else if( Command == "rmk" && !player->GetRmkVote( ) && m_GameLoaded )
			{
				if (m_RmkVotePlayer.empty())
				{
					for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
						(*i)->SetRmkVote( false );
					m_RmkVotePlayer = player->GetName();
					m_StartedRmkVoteTime = GetTime();
				}
				player->SetRmkVote( true );
				uint32_t VotesNeeded = (uint32_t)ceil( ( GetNumHumanPlayers( ) - 1 ) * (float)100 );
				if (VotesNeeded>GetNumHumanPlayers()-1)
					VotesNeeded = GetNumHumanPlayers()-1;
				uint32_t Votes = 0;

				for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
				{
					if( (*i)->GetRmkVote( ) )
						Votes++;
				}

				if( Votes >= VotesNeeded )
				{
					m_Stats->SetWinner(0);				
					SendAllChat(tr("lang_1053")); // "Game will end in 10 seconds"
					m_GameEndCountDownStarted = true;
					m_GameEndCountDownCounter = 10;
					m_GameEndLastCountDownTicks = GetTicks();
					m_RmkVotePlayer.clear( );
					m_StartedRmkVoteTime = 0;
				}
				else
					SendAllChat( tr("lang_1054", "$USER$", User, "$VOTE$", UTIL_ToString( Votes ), "$MAXVOTE$", UTIL_ToString(VotesNeeded), "$TRIGGER$", string( 1, m_GHost->m_CommandTrigger ) )); // SendAllChat( User+" voted for rmk [" + UTIL_ToString( Votes )+"/"+ UTIL_ToString(VotesNeeded)+"] "+string( 1, m_GHost->m_CommandTrigger )+"rmk to accept");
			}

			//
			// !VOTECANCEL
			//

			else if( Command == "votecancel" && !m_KickVotePlayer.empty( ) )
			{
				SendAllChat( tr("lang_0162", m_KickVotePlayer) ); //  VoteKickCancelled( m_KickVotePlayer )
				m_KickVotePlayer.clear( );
				m_StartedKickVoteTime = 0;
			}

			//
			// !W
			//

			else if( Command == "w" && !Payload.empty( ) )
			{
				// extract the name and the message
				// e.g. "Varlock hello there!" -> name: "Varlock", message: "hello there!"

				string Name;
				string Message;
				string :: size_type MessageStart = Payload.find( " " );

				if( MessageStart != string :: npos )
				{
					Name = Payload.substr( 0, MessageStart );
					Message = Payload.substr( MessageStart + 1 );

					for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
						(*i)->QueueChatCommand( Message, Name, true );
				}

				HideCommand = true;
			}
		}
		else
		{
			CONSOLE_Print( "[GAME: " + m_GameName + "] admin command ignored, the game is locked" );
			SendChat( player, tr("lang_0115") ); // TheGameIsLocked
		}
	}
	else
	{
		if( !player->GetSpoofed( ) )
			CONSOLE_Print( "[GAME: " + m_GameName + "] non-spoofchecked user [" + User + "] sent command [" + Command + "] with payload [" + Payload + "]" );
		else
			CONSOLE_Print( "[GAME: " + m_GameName + "] non-admin [" + User + "] sent command [" + Command + "] with payload [" + Payload + "]" );

		if (Command != "yes" && Command !="votekick" && Command != "rmk")
		if (!m_GHost->m_NonAdminCommands)
			return HideCommand;
	}

	/*********************
	* NON ADMIN COMMANDS *
	*********************/

	// 
	// !HELP
	//

	if ( Command == "help" )
		SendChat( player, tr("lang_1215"));

	//
	// !PING
	//

	else if( Command == "ping" )
		SendChat( player,  player->GetNumPings( ) > 0 ? UTIL_ToString( player->GetPing( m_GHost->m_LCPings ) ) + tr("lang_1017") : tr("lang_1018")); // "ms" : "N/A"

	//
	// !CHECKME
	//

	else if( Command == "checkme" ) // CheckedPlayer( User, player->GetNumPings( ) > 0 ? UTIL_ToString( player->GetPing( m_GHost->m_LCPings ) ) + "ms" : "N/A", m_GHost->m_DBLocal->FromCheck( UTIL_ByteArrayToUInt32( player->GetExternalIP( ), true ) ), AdminCheck || RootAdminCheck ? "Yes" : "No", IsOwner( User ) ? "Yes" : "No", player->GetSpoofed( ) ? "Yes" : "No", player->GetSpoofedRealm( ).empty( ) ? "N/A" : player->GetSpoofedRealm( ), player->GetReserved( ) ? "Yes" : "No" ) );
		//SendChat( player, m_GHost->m_Language->CheckedPlayer( User, player->GetNumPings( ) > 0 ? UTIL_ToString( player->GetPing( m_GHost->m_LCPings ) ) + "ms" : "N/A", m_GHost->m_DBLocal->FromCheck( UTIL_ByteArrayToUInt32( player->GetExternalIP( ), true ) ), AdminCheck || RootAdminCheck ? "Yes" : "No", IsOwner( User ) ? "Yes" : "No", player->GetSpoofed( ) ? "Yes" : "No", player->GetSpoofedRealm( ).empty( ) ? "N/A" : player->GetSpoofedRealm( ), player->GetReserved( ) ? "Yes" : "No" ) );
          SendChat( player, tr("lang_0122", "$VICTIM$", User,
                                                                      "$PING$", player->GetNumPings( ) > 0 ? UTIL_ToString( player->GetPing( m_GHost->m_LCPings ) ) + tr("lang_1017")/*"ms"*/ : tr("lang_1018")/*"N/A"*/,
                                                                      "$FROM$", m_GHost->m_DBLocal->FromCheck( UTIL_ByteArrayToUInt32( player->GetExternalIP( ), true ) ),
                                                                      "$ADMIN$", AdminCheck || RootAdminCheck ? tr("lang_1145")/*"Yes"*/ : tr("lang_1146")/*"No"*/,
                                                                      "$OWNER$", IsOwner( User ) ? tr("lang_1145")/*"Yes"*/ : tr("lang_1146")/*"No"*/,
                                                                      "$SPOOFED$", player->GetSpoofed( ) ? tr("lang_1145")/*"Yes"*/ : tr("lang_1146")/*"No"*/,
                                                                      "$SPOOFEDREALM$", player->GetSpoofedRealm( ).empty( ) ? tr("lang_1018")/*"N/A"*/ : player->GetSpoofedRealm( ),
                                                                      "$RESERVED$", player->GetReserved( ) ? tr("lang_1145")/*"Yes"*/ : tr("lang_1146")/*"No"*/ ) );


	//
	// !END
	//

	else if( Command == "end" )
	{
		if (!m_GameEndCountDownStarted && m_GHost->m_EndReq2ndTeamAccept)
		{
			bool do_end_command = false;
			if(Payload.empty( ))
			{
				if (m_EndRequested)
				if (m_Slots[GetSIDFromPID(player->GetPID())].GetTeam()!=m_EndRequestedTeam)
					do_end_command = true;
			}
			else if( Payload.find_first_not_of( "12" ) == string :: npos )
			{
				if (m_RequestedWinner && UTIL_ToUInt32(Payload) == m_RequestedWinner)
				if (m_Slots[GetSIDFromPID(player->GetPID())].GetTeam()!=m_EndRequestedTeam)
				{
					do_end_command = true;
					m_Stats->SetWinner(0);
				}
			}
			if (do_end_command)
			{
				CONSOLE_Print( "[GAME: " + m_GameName + "] is over (admin ended game)" );
				SendAllChat(tr("lang_1168")); // Game will end in 10 seconds"
				m_GameEndCountDownStarted = true;
				m_GameEndCountDownCounter = 10;
				m_GameEndLastCountDownTicks = GetTicks();
			}
		}
		return HideCommand;
	}
	//
	//	!SWAP
	//

	else if ( Command == "swap" && !Payload.empty( ) && !m_GameLoading && !m_GameLoaded )
	{
			uint32_t SID1;
			stringstream SS;
			SS << Payload;
			SS >> SID1;

			if( SS.fail( ) )
				CONSOLE_Print( "[GAME: " + m_GameName + "] bad input #1 to swap command" );
			else
			{ 
				if ( player->GetSID() != ( SID1 - 1 ) && SID1 < 11 && SID1 > 0 )
				{
					player->SetSwapTo( SID1 ); 

					CGamePlayer *Player2 = GetPlayerFromSID( SID1 - 1 );

					if ( !Player2 && m_Slots[ SID1 - 1 ].GetSlotStatus() == 0 )
						SwapSlots( (unsigned char)( player->GetSID() ), (unsigned char)( SID1 - 1 ) );

					else if ( Player2 && player-> GetSID() == ( Player2->GetSwapTo() - 1 ) && 
										 Player2->GetSID() == ( player->GetSwapTo() - 1 )
									  && GetTime() - player ->GetSwapToTime() < 30 
									  && GetTime() - Player2->GetSwapToTime() < 30)
					{
						SwapSlots( (unsigned char)( player->GetSID() ), (unsigned char)( Player2->GetSID() ) );

						CONSOLE_Print( "[GAME: " + m_GameName + "] swap accepted from " + player->GetName() + " to "+ Player2->GetName() );
					} else if (Player2)
						SendChat(Player2, tr("1506", "$PLAYER$", player->GetName(), "$COMMAND$", string( 1, m_GHost->m_CommandTrigger ) + "swap " + UTIL_ToString( player->GetSID() + 1 ) ));
				}
			}
		
		}
/*
		//
		//	!POINTS
		//

		else if ( Command == "points" && Payload.empty( ) )
		{
			string msg = string();

			map<int, int> team_points;
			team_points[0] = 0;
			team_points[1] = 0;

			for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i)
			{
				team_points[ m_Slots[ (*i)->GetSID() ].GetTeam() ] += (int)(*i)->GetScore();

				msg += (*i)->GetName() + " (" + (*i)->GetScoreS() + ") ";
			}

			if (!msg.empty())
				SendAllChat( msg );

			msg = string();

			for (map<int, int>::iterator i = team_points.begin(); i != team_points.end(); ++i)
				msg += tr( "1507", "$TEAM$", UTIL_ToString( (*i).first + 1), "$SCORE$", UTIL_ToString((*i).second) ) + " ";

			if (!msg.empty())
				SendAllChat( msg );
		}
*/
	//
	// !VOTEMODE or VOTEMOD
	//

	else if ( (Command == "votemode" || Command == "votemod") && !Payload.empty( ) && !m_GameLoaded && m_Map->GetMapType( ) == "dota")
	{
		if (Payload[0] == '-')
			Payload.erase(Payload.begin());

		if ( Payload == "ap" ||
			 Payload == "ar" ||
			 Payload == "rd" ||
			 Payload == "sd" ||
			 Payload == "cm"
		   )
			{
				player->SetVoteDotaMode(Payload);

				SendAllChat( tr("1508", "$USER$", player->GetName(), "$MOD$", Payload ) );

				map<string, int> votes;

				votes["ap"] = 0;
				votes["ar"] = 0;
				votes["rd"] = 0;
				votes["sd"] = 0;
				votes["cm"] = 0;

				for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i)
					if ( !(*i)->GetVoteDotaMode().empty() )
						++votes[ (*i)->GetVoteDotaMode() ];
			
				string new_mode = string();
				int votes_need = 1;

				for (map<string, int>::iterator i = votes.begin(); i != votes.end(); ++i)
					if ( (*i).second > votes_need )
					{
						votes_need = (*i).second;
						new_mode = (*i).first;
					}

			if (votes_need > 5)
				SetHCL(new_mode);

		}

	}

	//
	// !VOTEABC !BALANCE
	//

	else if ( (Command == "voteabc" || Command == "balance") && !Payload.empty() && !m_GameLoading && !m_GameLoaded )
	{
		player->SetABCVote(true);

		int ABCvote_count = 0;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); i++)
			if ((*i)->GetABCVote())
				++ABCvote_count;

		if ( m_Players.size() > 2 && ABCvote_count > (int)(m_Players.size() / 2) )
		{
			for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); i++)
				(*i)->SetABCVote(false);

			BalanceSlots();
		} else
			SendAllChat( tr("1509", "$USER$", player->GetName(), "$VOTES$", UTIL_ToString( (int)(m_Players.size() / 2) -  ABCvote_count) ));
	}
/*
	//
	// !VOTESTART
	//

	else if ( Command == "votestart" && !player->GetStartVote() && Payload.empty( ) )
	{
		player->SetStartVote( true );

		SendAllChat( tr("1510", player->GetName()));

		int start_voted = 0;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i)
			if ((*i)->GetStartVote())
				++start_voted;

		if (start_voted == m_Players.size() && !m_CountDownStarted)
			StartCountDown( false );
	}

	//
	// !NOTVOTED for start !NV
	//

	else if ( Command == "notvoted" || Command == "nv" && Payload.empty( ) )
	{
		string not_voted_players;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i)
			if (!(*i)->GetStartVote())
			{
				not_voted_players += (*i)->GetName();

				if (i != m_Players.end() - 1)
					not_voted_players += ",";
			}

		if (!not_voted_players.empty())
			SendAllChat( tr("lang_1220", not_voted_players));
	}
*/	
	//
	// !STATS
	//

	else if( Command == "stats" && GetTime( ) >= player->GetStatsSentTime( ) + 5 )
	{
		string StatsUser = User;

		if( !Payload.empty( ) )
		{
			StatsUser = Payload;

			CGamePlayer *LastMatch = NULL;

			uint32_t Matches = GetPlayerFromNamePartial( Payload , &LastMatch );
			if (Matches == 1)
				StatsUser = LastMatch->GetName();
		}

		if( player->GetSpoofed( ) && ( AdminCheck || RootAdminCheck || IsOwner( User ) ) )
			m_PairedGPSChecks.push_back( PairedGPSCheck( string( ), m_GHost->m_DB->ThreadedGamePlayerSummaryCheck( StatsUser ) ) );
		else
			m_PairedGPSChecks.push_back( PairedGPSCheck( User, m_GHost->m_DB->ThreadedGamePlayerSummaryCheck( StatsUser ) ) );

		player->SetStatsSentTime( GetTime( ) );
	}

	//
	// !STATSDOTA
	//

	else if( Command == "statsdota" && GetTime( ) >= player->GetStatsDotASentTime( ) + 5 && !m_GHost->m_nostatsdota )
	{
		string StatsUser = User;

		if( !Payload.empty( ) )
		{
			StatsUser = Payload;

			CGamePlayer *LastMatch = NULL;

			uint32_t Matches = GetPlayerFromNamePartial( Payload , &LastMatch );
			if (Matches == 1)
				StatsUser = LastMatch->GetName();
		}

		if( player->GetSpoofed( ) && ( AdminCheck || RootAdminCheck || IsOwner( User ) ) )
			m_PairedDPSChecks.push_back( PairedDPSCheck( string( ), m_GHost->m_DB->ThreadedDotAPlayerSummaryCheck( StatsUser, m_GHost->m_ScoreFormula, m_GHost->m_ScoreMinGames, string() ) ) );
		else
			m_PairedDPSChecks.push_back( PairedDPSCheck( User, m_GHost->m_DB->ThreadedDotAPlayerSummaryCheck( StatsUser, m_GHost->m_ScoreFormula, m_GHost->m_ScoreMinGames, string() ) ) );

		player->SetStatsDotASentTime( GetTime( ) );
	}

	//
	// !VERSION
	//
/*
	else if( Command == "version" || Command == "v" )
	{
	    SendChat( player, tr("lang_0036", m_GHost->m_Version ) ); // VersionAdmin

	    
		if( player->GetSpoofed( ) && ( AdminCheck || RootAdminCheck || IsOwner( User ) ) )
			SendChat( player, tr("lang_0036", m_GHost->m_Version ) ); // VersionAdmin
		else
//			SendChat( player, m_GHost->m_Language->VersionNotAdmin( m_GHost->m_Version ) );
			SendChat( player, tr("lang_0036", m_GHost->m_Version ) ); // VersionAdmin
        
	}
*/
	
	//
	// !VOTESTART
	//
	
	else if( Command == "votestart" )
	{
		bool votestartAuth = player->GetSpoofed( ) && ( AdminCheck || RootAdminCheck || IsOwner( User ) );
		bool votestartAutohost = m_GameState == GAME_PUBLIC && !m_GHost->m_AutoHostGameName.empty( ) && m_GHost->m_AutoHostMaximumGames != 0 && m_GHost->m_AutoHostAutoStartPlayers != 0 && m_AutoStartPlayers != 0;

		if( m_GHost->m_VoteStartAllowed && !m_CountDownStarted && (votestartAuth || votestartAutohost || !m_GHost->m_VoteStartAutohostOnly) )
		{
			if( m_GHost->m_CurrentGame->GetLocked( ) )
			{
				SendChat( player, "Error: cannot votestart because the game is locked. Owner is " + m_OwnerName );
				return HideCommand;
			}


			if(m_StartedVoteStartTime == 0)
			{ //need >minplayers or admin to START a votestart
				if (GetNumHumanPlayers() < m_GHost->m_VoteStartMinPlayers && !votestartAuth)
				{ //need at least eight players to votestart
					SendChat( player, "You cannot use !votestart until there " + UTIL_ToString(m_GHost->m_VoteStartMinPlayers) + " or more players!" );
					return HideCommand;
				}

				for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); ++i )
					(*i)->SetStartVote( false );

				m_StartedVoteStartTime = GetTime();
			
				CONSOLE_Print( "[GAME: " + m_GameName + "] votestart started by player [" + User + "]" );
			}

			player->SetStartVote(true);
				
			uint32_t VotesNeeded;
			uint32_t Votes = 0;


			for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); ++i )
			{
				if( (*i)->GetStartVote( ) )
					++Votes;
			}


			if( m_GHost->m_VoteStartPercentalVoting)
			{
				VotesNeeded = ((uint32_t) (GetNumHumanPlayers() *  (m_GHost->m_VoteStartPercent - 1) / 100)) + 1;
			}
			else
			{
				VotesNeeded = m_GHost->m_VoteStartMinPlayers;
			}
		
			if( Votes < VotesNeeded )
			{
				SendAllChat( UTIL_ToString(VotesNeeded - Votes) + " more votes needed to votestart.");
			}
			else
			{
				StartCountDown( true );
			}

		}
	}

	//
	// !NOTVOTED for start !NV
	//

	else if ( Command == "notvoted" || Command == "nv" && Payload.empty( ) )
	{
		string not_voted_players;

		for (vector<CGamePlayer*>::iterator i = m_Players.begin(); i != m_Players.end(); ++i)
			if (!(*i)->GetStartVote())
			{
				not_voted_players += (*i)->GetName();

				if (i != m_Players.end() - 1)
					not_voted_players += ",";
			}

		if (!not_voted_players.empty())
			SendAllChat( tr("lang_1220", not_voted_players));
	}
	
	//
	// !VOTEKICK
	//

	else if( Command == "votekick" && m_GHost->m_VoteKickAllowed && !Payload.empty( ) )
	{
		if( !m_KickVotePlayer.empty( ) )
			SendChat( player, tr("lang_0153") ); // UnableToVoteKickAlreadyInProgress
		else if( m_Players.size( ) == 2 )
			SendChat( player, tr("lang_0154") ); // UnableToVoteKickNotEnoughPlayers
		else
		{
			CGamePlayer *LastMatch = NULL;
			uint32_t Matches = GetPlayerFromNamePartial( Payload, &LastMatch );

			if( Matches == 0 )
				SendChat( player, tr("lang_0155", Payload ) ); // UnableToVoteKickNoMatchesFound
			else if( Matches == 1 )
			{
				if( LastMatch->GetReserved( ) )
					SendChat( player, tr("lang_0156", LastMatch->GetName( ) ) ); // UnableToVoteKickPlayerIsReserved
				else
				{
					m_KickVotePlayer = LastMatch->GetName( );
					m_StartedKickVoteTime = GetTime( );

					for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
						(*i)->SetKickVote( false );

					player->SetKickVote( true );
					CONSOLE_Print( "[GAME: " + m_GameName + "] votekick against player [" + m_KickVotePlayer + "] started by player [" + User + "]" );

					SendAllChat( tr("lang_0157",
                                               "$VICTIM$", LastMatch->GetName( ),
                                               "$USER$", User,
                                               "$VOTESNEEDED$", UTIL_ToString( (uint32_t)ceil( ( GetNumHumanPlayers( ) - 1 ) * (float)m_GHost->m_VoteKickPercentage / 100 ) - 1 ) ) ); // StartedVoteKick

					SendAllChat( tr("lang_0165", string( 1, m_GHost->m_CommandTrigger ) ) );  // TypeYesToVote
				}
			}
			else
				SendChat( player, tr("lang_0158", Payload ) ); // UnableToVoteKickFoundMoreThanOneMatch
		}
	}

	//
	// !YES
	//

	if( Command == "yes" && !m_KickVotePlayer.empty( ) && player->GetName( ) != m_KickVotePlayer && !player->GetKickVote( ) )
	{
		player->SetKickVote( true );
		uint32_t VotesNeeded = (uint32_t)ceil( ( GetNumHumanPlayers( ) - 1 ) * (float)m_GHost->m_VoteKickPercentage / 100 );
		uint32_t Votes = 0;

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( (*i)->GetKickVote( ) )
				Votes++;
		}

		if( Votes >= VotesNeeded )
		{
			CGamePlayer *Victim = GetPlayerFromName( m_KickVotePlayer, true );

			if( Victim )
			{
				Victim->SetDeleteMe( true );
				Victim->SetLeftReason( tr("lang_0164") ); // WasKickedByVote

				if( !m_GameLoading && !m_GameLoaded )
					Victim->SetLeftCode( PLAYERLEAVE_LOBBY );
				else
					Victim->SetLeftCode( PLAYERLEAVE_LOST );

				if( !m_GameLoading && !m_GameLoaded )
					OpenSlot( GetSIDFromPID( Victim->GetPID( ) ), false );

				CONSOLE_Print( "[GAME: " + m_GameName + "] votekick against player [" + m_KickVotePlayer + "] passed with " + UTIL_ToString( Votes ) + "/" + UTIL_ToString( GetNumHumanPlayers( ) ) + " votes" );
				SendAllChat( tr("lang_0159", m_KickVotePlayer ) ); // VoteKickPassed
			}
			else
				SendAllChat( tr("lang_0160", m_KickVotePlayer ) ); // ErrorVoteKickingPlayer

			m_KickVotePlayer.clear( );
			m_StartedKickVoteTime = 0;
		}
		else
			SendAllChat( tr( "lang_0161",
                                                "$VICTIM$", m_KickVotePlayer,
                                                "$USER$", User,
                                                "$VOTES$", UTIL_ToString( VotesNeeded - Votes ) ) ); // VoteKickAcceptedNeedMoreVotes
	}

	//
	// !FF
	//

	else if( Command == "ff" && !player->GetFFVote( ) && m_GameLoaded && m_Map->GetMapType( ) == "dota" )
	{
		if ( !m_Stats )
		{
			SendChat(player->GetPID(), tr("lang_1208"));
			return HideCommand;
		}
/*
		if ( m_Stats->GetSwitchMode() )
		{
			SendChat(player->GetPID(), tr("lang_1218"));
			return HideCommand;
		}
*/
		if ( m_GHost->m_minFFtime && ((GetTime() - GetGameLoadedTime()) < m_GHost->m_minFFtime) )
		{
			SendChat(player->GetPID(), tr("lang_1217", UTIL_ToString(m_GHost->m_minFFtime)));
			return HideCommand;
		}
		
		player->SetFFVote( true );
		EventDotaGameFastFinishProcess( player, "lang_1211" );

		SendChat(player->GetPID(), tr("lang_1210", "$TRIGGER$", string(1, m_GHost->m_CommandTrigger)));
	}
	
	//
	// !NOFF
	//

	else if( Command == "noff" && player->GetFFVote( ) && m_GameLoaded && m_Map->GetMapType( ) == "dota" )
	{
		player->SetFFVote( false );
		EventDotaGameFastFinishProcess( player, "lang_1212" );
	}
	
	
	//
	// !RMK
	//

	else if( Command == "rmk" && !player->GetRmkVote( ) && m_GameLoaded )
	{
		if (m_RmkVotePlayer.empty())
		{
			for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
				(*i)->SetRmkVote( false );
			m_RmkVotePlayer = player->GetName();
			m_StartedRmkVoteTime = GetTime();
		}
		player->SetRmkVote( true );
		uint32_t VotesNeeded = (uint32_t)ceil( ( GetNumHumanPlayers( ) - 1 ) * (float)100 );
		if (VotesNeeded>GetNumHumanPlayers()-1)
			VotesNeeded = GetNumHumanPlayers()-1;

		uint32_t Votes = 0;

		for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		{
			if( (*i)->GetRmkVote( ) )
				Votes++;
		}

		if( Votes >= VotesNeeded )
		{
			m_Stats->SetWinner(0);
			SendAllChat(tr("lang_1053")); // Game will end in 10 seconds
			m_GameEndCountDownStarted = true;
			m_GameEndCountDownCounter = 10;
			m_GameEndLastCountDownTicks = GetTicks();
			m_RmkVotePlayer.clear( );
			m_StartedRmkVoteTime = 0;
		}
		else
			SendAllChat( User+" voted for rmk [" + UTIL_ToString( Votes )+"/"+ UTIL_ToString(VotesNeeded)+"] "+string( 1, m_GHost->m_CommandTrigger )+"rmk to accept");
	}

	//
	// !SD
	// !SDI
	// !SDPRIV
	// !SDPUB
	//

	else if( (Command == "sd" || Command == "sdi" || Command == "sdpub" || Command == "sdpriv") && GetTime( ) >= player->GetStatsDotASentTime( ) + 5 )
	{
		string StatsUser = User;
		string GameState = string();

		if (Command == "sdi")
			GameState = UTIL_ToString(m_GHost->m_gamestateinhouse);

		if (Command == "sdpub")
			GameState = "16";

		if (Command == "sdpriv")
			GameState = "17";

		if( !Payload.empty( ) )
		{
			StatsUser = Payload;

			CGamePlayer *LastMatch = NULL;

			uint32_t Matches = GetPlayerFromNamePartial( Payload , &LastMatch );
			if (Matches == 1)
				StatsUser = LastMatch->GetName();
		}

		if (m_GHost->m_CalculatingScores)
			return HideCommand;

		bool nonadmin = !(( player->GetSpoofed( ) &&  AdminCheck) || RootAdminCheck || IsOwner( User ) ) ;

		if (!nonadmin)
			m_PairedDPSChecks.push_back( PairedDPSCheck( "%", m_GHost->m_DB->ThreadedDotAPlayerSummaryCheck( StatsUser, m_GHost->m_ScoreFormula, m_GHost->m_ScoreMinGames, GameState ) ) );
		else
			m_PairedDPSChecks.push_back( PairedDPSCheck( "%"+User, m_GHost->m_DB->ThreadedDotAPlayerSummaryCheck( StatsUser, m_GHost->m_ScoreFormula, m_GHost->m_ScoreMinGames, GameState ) ) );

		player->SetStatsDotASentTime( GetTime( ) );
	}

	//
	// !COUNTBANS
	// !CBS
	//

	else if( (Command == "countbans" || Command == "cbs") && Payload.empty() )
	{
		uint32_t Count = m_GHost->m_DB->BanCount( m_Server );

		if( Count == 0 )
			SendAllChat( tr("lang_0016", m_Server ) ); // ThereAreNoBannedUsers
		else if( Count == 1 )
			SendAllChat( tr("lang_0017", m_Server ) ); // ThereIsBannedUser
		else
			SendAllChat( tr("lang_0018", "$SERVER$", m_Server, "$COUNT$", UTIL_ToString( Count ) ) ); // ThereAreBannedUsers
	}

	//
	// !START
	//

	else if( (Command == "start") && !m_CountDownStarted && m_GHost->m_AutoHostAllowStart && m_AutoStartPlayers>0 )
	{
		// if the player sent "!start force" skip the checks and start the countdown
		// otherwise check that the game is ready to start

		ReCalculateTeams();
		if (m_Team1<1 || m_Team2<1)
		{
			SendAllChat(tr("lang_1206")); // "Both teams must contain at least one player!"
			return HideCommand;
		}

		if( Payload == "force" )
			StartCountDown( true );
		else
			StartCountDown( false );
	}

	//
	// !STARTN
	//

	else if( (Command == "startn") && !m_CountDownStarted && m_GHost->m_AutoHostAllowStart && m_AutoStartPlayers>0 )
	{
		// skip checks and start the game right now

		ReCalculateTeams();
		if (m_Team1<1 || m_Team2<1)
		{
			SendAllChat(tr("lang_1206")); // "Both teams must contain at least one player!"
			return HideCommand;
		}

		if (GetTicks()-m_LastLeaverTicks<1000)
		{
			SendAllChat(tr("lang_1198") ); // "Sure you want to start right now? Someone just left"
			return HideCommand;
		}

		m_CountDownStarted = true;
		m_CountDownCounter = 0;
	}

	return HideCommand;
}

void CGame :: EventDotaGameFastFinishProcess( CGamePlayer* player, const string& nLangId, bool nShowMsg )
{
	uint32_t votes = 0;
	uint32_t votesmax = 0;
	unsigned char playerTeam = m_Slots[GetSIDFromPID(player->GetPID())].GetTeam();

	if ( !EventDotaGameFastFinishProcess(votes, votesmax, playerTeam) && nShowMsg )
		SendAllChat( tr(nLangId, "$USER$", player->GetName(), "$TEAMNAME$", (playerTeam == 0 ? "SENTINEL" : "SCOURGE"), "$VOTES$", UTIL_ToString(votes), "$COUNT$", UTIL_ToString(votesmax) ));
}

int CGame :: EventDotaGameFastFinishProcess( uint32_t &votes, uint32_t &votesmax, unsigned char playerTeam )
{
	int count1 = 0;
	int count2 = 0; //count of players in the team.
	int votes1 = 0;
	int votes2 = 0; //count of votes in the team.
		
	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
	{
		if( !(*i)->GetLeftMessageSent( ) )
		if( m_Slots[GetSIDFromPID((*i)->GetPID())].GetTeam() == (unsigned char)0)
		{
			count1++;
			if( (*i)->GetFFVote( ) )
				votes1++;
		}
		else
		{
			count2++;
			if( (*i)->GetFFVote( ) )
				votes2++;
		}
	}

	if ( count1 == 0 )
		count1 = -1;
	
	if ( count2 == 0 )
		count2 = -1;
		
	bool end1 = (votes1 == count1); 
	bool end2 = (votes2 == count2);

	if ( playerTeam == 0 )
	{
		votes = votes1;
		votesmax = count1;
	}
	else if ( playerTeam == 1 )
	{
		votes = votes2;
		votesmax = count2;
	}

	if( end1 || end2 )
	{
		if( end1 )
			m_Stats->SetWinner(2);
		else if ( end2 )
			m_Stats->SetWinner(1);
		else
			m_Stats->SetWinner(0);	
			
//		SendAllChat(tr("lang_1213", end1 ? "SENTINEL" : "SCOURGE" ));

		SendAllChat(tr("lang_1053")); // "Game will end in 10 seconds"
		m_GameEndCountDownStarted = true;
		m_GameEndCountDownCounter = 10;
		m_GameEndLastCountDownTicks = GetTicks();

		return end1 ? 2 : 1;
	}
	else
	{
		m_GameEndCountDownStarted = false;
		m_GameOverTime = 0;
		return 0;
	}
}


void CGame :: EventGameStarted( )
{
	CBaseGame :: EventGameStarted( );

	// record everything we need to ban each player in case we decide to do so later
	// this is because when a player leaves the game an admin might want to ban that player
	// but since the player has already left the game we don't have access to their information anymore
	// so we create a "potential ban" for each player and only store it in the database if requested to by an admin

	for( vector<CGamePlayer *> :: iterator i = m_Players.begin( ); i != m_Players.end( ); i++ )
		m_DBBans.push_back( new CDBBan( (*i)->GetJoinedRealm( ), (*i)->GetName( ), (*i)->GetExternalIPString( ), string( ), string( ), string( ), string( ), string( ) ) );
}

bool CGame :: IsGameDataSaved( )
{
	return m_CallableGameAdd && m_CallableGameAdd->GetReady( );
}

void CGame :: SaveGameData( )
{
	CONSOLE_Print( "[GAME: " + m_GameName + "] saving game data to database" );
	m_CallableGameAdd = m_GHost->m_DB->ThreadedGameAdd( m_GHost->m_BNETs.size( ) == 1 ? m_GHost->m_BNETs[0]->GetServer( ) : string( ), m_DBGame->GetMap( ), m_GameName, m_OwnerName, m_GameTicks / 1000, m_GameStateS, m_CreatorName, m_CreatorServer );
}

void CGame :: WarnPlayer( CDBBan *LastMatch, string Reason, string User)
{
	m_GHost->m_Callables.push_back(m_GHost->m_DB->ThreadedBanAdd( LastMatch->GetServer(), LastMatch->GetName(), LastMatch->GetIP( ), m_GameName, User, Reason, m_GHost->m_WarnTimeOfWarnedPlayer, 1 ));
	string BanPlayer = LastMatch->GetName();

	bool isAdmin = IsOwner(BanPlayer);
	for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
	{
		if((*j)->IsAdmin(BanPlayer ) || (*j)->IsRootAdmin( BanPlayer ) )
		{
			isAdmin = true;
			break;
		}
	}
	uint32_t GameNr = GetGameNr();

	CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + LastMatch->GetName( ) + "] was warned by player [" + User + "]" );

	uint32_t WarnCount = 0;
	for(int i = 0; i < 3 && WarnCount == 0; i++)
	{
		WarnCount = m_GHost->m_DB->BanCount( LastMatch->GetName( ), 1 );
	}

	string sBan = tr("lang_0505", "$VICTIM$", LastMatch->GetName( ),
                                                            "$USER$", User,
                                                            "$WARNNUM$", UTIL_ToString(WarnCount)); // PlayerWasWarnedByPlayer

	string sBReason = sBan + ", "+Reason;

	if(WarnCount >= m_GHost->m_BanTheWarnedPlayerQuota)
	{
	    if (m_GHost->m_BanTimeOfWarnedPlayer)
            SendAllChat( tr("lang_0509", "$VICTIM$", LastMatch->GetName( ), "$BANDAYTIME$", UTIL_ToString( m_GHost->m_BanTimeOfWarnedPlayer )));
        else
            SendAllChat( tr("lang_0510", LastMatch->GetName( ))); // UserReachedWarnQuota

		if (!isAdmin)
			m_GHost->m_Callables.push_back(m_GHost->m_DB->ThreadedBanAdd( LastMatch->GetServer( ), LastMatch->GetName( ), LastMatch->GetIP( ), m_GameName, User, "Reached the warn quota", m_GHost->m_BanTimeOfWarnedPlayer, 0 ));
		else
		{
			m_GHost->m_Callables.push_back(m_GHost->m_DB->ThreadedAdminRemove( LastMatch->GetServer( ), LastMatch->GetName( )));
			for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
			{
				if ((*j)->GetServer() == m_Server)
				{
					(*j)->RemoveAdmin( LastMatch->GetName( ) );
				}
			}

		}
		m_GHost->m_Callables.push_back(m_GHost->m_DB->ThreadedWarnUpdate( LastMatch->GetName( ), 3, 2));
		m_GHost->m_Callables.push_back(m_GHost->m_DB->ThreadedWarnUpdate( LastMatch->GetName( ), 1, 3));

		if (!isAdmin)
		for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
		{
			if ((*j)->GetServer() == m_Server)
			{
				string sDate = string();
				if (m_GHost->m_BanTimeOfWarnedPlayer>0)
				{
					struct tm * timeinfo;
					char buffer [80];
					time_t Now = time( NULL );
					Now += 3600*24*m_GHost->m_BanTimeOfWarnedPlayer;
					timeinfo = localtime( &Now );
					strftime (buffer,80,"%d-%m-%Y",timeinfo);
					sDate = buffer;
				}

				(*j)->AddBan(LastMatch->GetName(), LastMatch->GetIP(), LastMatch->GetGameName(), LastMatch->GetAdmin(), Reason, sDate);
			}
		}
	} else
		if (Reason=="")
		{
			SendAllChat( sBan );
		} else
		{
			if (sBReason.length()<220 && !m_GHost->m_TwoLinesBanAnnouncement)
				SendAllChat( sBReason );
			else
			{
				SendAllChat( sBan);
				SendAllChat( "Warn reason: " + Reason);
			}
		}

		if (m_GHost->m_NotifyBannedPlayers)
		{
			sBReason = "You have been warned";
			if (Reason!="")
				sBReason = sBReason+", "+Reason;
			for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
			{
				if( (*i)->GetServer( ) == GetCreatorServer( ) )
				{
					(*i)->QueueChatCommand( sBReason, LastMatch->GetName(), true );
					if(WarnCount >= m_GHost->m_BanTheWarnedPlayerQuota)
					{
						sBReason = "You have been banned for ";
						sBReason += UTIL_ToString(m_GHost->m_BanTimeOfWarnedPlayer);
						sBReason += " days for reaching the warn quota. ";

						if (isAdmin)
							sBReason = "You are no longer an admin!";
						(*i)->QueueChatCommand( sBReason, LastMatch->GetName(), true );
					}
				}
			}
		}
}

void CGame :: WarnPlayer( string Victim, string Reason, string User)
{
	bool RootAdminCheck = IsRootAdmin(User);
	CGamePlayer *player = GetPlayerFromName(User, false);
	CGamePlayer *LastMatch = NULL;
	uint32_t Matches = GetPlayerFromNamePartial( Victim, &LastMatch );

	//					if( Matches == 0 )
	//						SendAllChat( m_GHost->m_Language->UnableToWarnNoMatchesFound( Victim ) );
	if( Matches <= 1 )
	{
		string BanPlayer = Victim;
		if (Matches == 1)
			BanPlayer = LastMatch->GetName();
		bool isAdmin = IsOwner(BanPlayer);
		for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
		{
			if((*j)->IsAdmin(BanPlayer ) || (*j)->IsRootAdmin( BanPlayer ) )
			{
				isAdmin = true;
				break;
			}
		}

		if (isAdmin && !RootAdminCheck)
		{
			SendChat( player->GetPID(), "You can't warn an admin!");
			return;
		}

		if ((IsSafe(BanPlayer) && m_GHost->m_SafelistedBanImmunity) && !RootAdminCheck)
		{
			SendChat( player->GetPID(), "You can't warn a safelisted player!");
			return;
		}

		if (Matches == 1)
			m_PairedBanAdds.push_back( PairedBanAdd( User, m_GHost->m_DB->ThreadedBanAdd( LastMatch->GetJoinedRealm( ), LastMatch->GetName( ), LastMatch->GetExternalIPString( ), m_GameName, User, Reason, m_GHost->m_WarnTimeOfWarnedPlayer, 1 ) ) );
		else
			m_PairedBanAdds.push_back( PairedBanAdd( User, m_GHost->m_DB->ThreadedBanAdd( m_Server, Victim, string(), m_GameName, User, Reason, m_GHost->m_WarnTimeOfWarnedPlayer, 1 ) ) );
		//						m_GHost->m_DB->BanAdd( LastMatch->GetJoinedRealm( ), LastMatch->GetName( ), LastMatch->GetExternalIPString( ), m_GameName, User, Reason );

		CONSOLE_Print( "[GAME: " + m_GameName + "] player [" + BanPlayer + "] was warned by player [" + User + "]" );

		uint32_t WarnCount = 0;
		for(int i = 0; i < 3 && WarnCount == 0; i++)
		{
			WarnCount = m_GHost->m_DB->BanCount( BanPlayer, 1 );
		}


		/* WTF??

		m_GHost->m_Language->PlayerWasWarnedByPlayer(
			GetCreatorServer(),
			BanPlayer, User);
		if (Matches == 1)
			sBan = m_GHost->m_Language->PlayerWasWarnedByPlayer(
			GetCreatorServer(),
			BanPlayer+" ("+LastMatch->GetExternalIPString()+")", User);

        */

         /*   string sBan = tr("lang_0505", "$VICTIM$", BanPlayer,
                                                            "$USER$", BanPlayer,
                                                            "$WARNNUM$", "0"); // PlayerWasWarnedByPlayer
*/
        string sBan = "";
		string sBReason = sBan + ", "+Reason;

		if (Reason=="")
		{
			SendAllChat( sBan );
		} else
		{
			if (sBReason.length()<220 && !m_GHost->m_TwoLinesBanAnnouncement)
				SendAllChat( sBReason );
			else
			{
				SendAllChat( sBan);
				SendAllChat( "Warn reason: " + Reason);
			}
		}

		if(WarnCount >= m_GHost->m_BanTheWarnedPlayerQuota)
		{

        if (m_GHost->m_BanTimeOfWarnedPlayer)
            SendAllChat( tr("lang_0509", "$VICTIM$", BanPlayer, "$BANDAYTIME$", UTIL_ToString( m_GHost->m_BanTimeOfWarnedPlayer )));
        else
            SendAllChat( tr("lang_0510", BanPlayer)); // UserReachedWarnQuota

			if (isAdmin)
			{
				for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
				{
					if ((*j)->GetServer() == m_Server)
					{
						(*j)->m_PairedAdminRemoves.push_back( PairedAdminRemove( User, m_GHost->m_DB->ThreadedAdminRemove( m_Server, BanPlayer ) ) );
					}
				}
			} else
			if (Matches==1)
				m_GHost->m_Callables.push_back(m_GHost->m_DB->ThreadedBanAdd( LastMatch->GetSpoofedRealm( ), BanPlayer, LastMatch->GetExternalIPString( ), m_GameName, User, "Reached the warn quota", m_GHost->m_BanTimeOfWarnedPlayer, 0 ));
			else
				m_GHost->m_Callables.push_back(m_GHost->m_DB->ThreadedBanAdd( m_Server, BanPlayer, string(), m_GameName, User, "Reached the warn quota", m_GHost->m_BanTimeOfWarnedPlayer, 0 ));
			m_GHost->m_Callables.push_back(m_GHost->m_DB->ThreadedWarnUpdate( BanPlayer, 3, 2));
			m_GHost->m_Callables.push_back(m_GHost->m_DB->ThreadedWarnUpdate( BanPlayer, 1, 3));

			if (!isAdmin)
			for( vector<CBNET *> :: iterator j = m_GHost->m_BNETs.begin( ); j != m_GHost->m_BNETs.end( ); j++ )
			{
				if ((*j)->GetServer() == m_Server)
				{
					string sDate = string();
					if (m_GHost->m_BanTimeOfWarnedPlayer>0)
					{
						struct tm * timeinfo;
						char buffer [80];
						time_t Now = time( NULL );
						Now += 3600*24*m_GHost->m_BanTimeOfWarnedPlayer;
						timeinfo = localtime( &Now );
						strftime (buffer,80,"%d-%m-%Y",timeinfo);
						sDate = buffer;
					}

					if (Matches==1)
						(*j)->AddBan(LastMatch->GetName(), LastMatch->GetExternalIPString(), m_GameName, User, Reason, sDate);
					else
						(*j)->AddBan(BanPlayer, string(), m_GameName, User, Reason, sDate);
				}
			}
		}

		if (m_GHost->m_NotifyBannedPlayers)
		{
			sBReason = "You have been warned";
			if (Reason!="")
				sBReason = sBReason+", "+Reason;
			for( vector<CBNET *> :: iterator i = m_GHost->m_BNETs.begin( ); i != m_GHost->m_BNETs.end( ); i++ )
			{
				if( (*i)->GetServer( ) == GetCreatorServer( ) )
				{
					(*i)->QueueChatCommand( sBReason, BanPlayer, true );
					if(WarnCount >= m_GHost->m_BanTheWarnedPlayerQuota)
					{
						sBReason = "You have been banned for ";
						sBReason += UTIL_ToString(m_GHost->m_BanTimeOfWarnedPlayer);
						sBReason += " days for reaching the warn quota. ";
						if (isAdmin)
							sBReason = "You are no longer an admin!";

						(*i)->QueueChatCommand( sBReason, BanPlayer, true );
					}
				}
			}
		}
	}
	else
		SendAllChat( tr("lang_0506", Victim ) ); // UnableToWarnFoundMoreThanOneMatch
}
