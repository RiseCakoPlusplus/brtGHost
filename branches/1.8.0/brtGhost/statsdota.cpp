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

#include "ghost.h"
#include "util.h"
#include "ghostdb.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "game_base.h"
#include "stats.h"
#include "statsdota.h"
#include "items.h"

//
// CStatsDOTA
//

CStatsDOTA :: CStatsDOTA( CBaseGame *nGame ) : CStats( nGame )
{
	CONSOLE_Print( "[STATSDOTA] using dota stats" );
	
	m_AllItems = new CDotaAllItems();
	
	for( unsigned int i = 0; i < 12; i++ )
		m_Players[i] = NULL;
	
	for( unsigned int i = 0; i < 12; i++ )
		m_DotaItems[i] = new CDotaItems(m_AllItems);

	m_Winner = 0;
	m_Min = 0;
	m_Sec = 0;
	m_Swap = false;
	m_SwitchOff = true;
}

CStatsDOTA :: ~CStatsDOTA( )
{
	for( unsigned int i = 0; i < 12; i++ )
	{
		if( m_Players[i] )
			delete m_Players[i];
		if( m_DotaItems[i] )
			delete m_DotaItems[i];
	}
	delete m_AllItems;
}

bool CStatsDOTA :: ProcessAction( CIncomingAction *Action )
{
	unsigned int i = 0;
	BYTEARRAY *ActionData = Action->GetAction( );
	BYTEARRAY Data;
	BYTEARRAY Key;
	BYTEARRAY Value;

	// dota actions with real time replay data start with 0x6b then the null terminated string "dr.x"
	// unfortunately more than one action can be sent in a single packet and the length of each action isn't explicitly represented in the packet
	// so we have to either parse all the actions and calculate the length based on the type or we can search for an identifying sequence
	// parsing the actions would be more correct but would be a lot more difficult to write for relatively little gain
	// so we take the easy route (which isn't always guaranteed to work) and search the data for the sequence "6b 64 72 2e 78 00" and hope it identifies an action

	while( ActionData->size( ) >= i + 6 )
	{


		if( (*ActionData)[i] == 0x6b && (*ActionData)[i + 1] == 0x64 && (*ActionData)[i + 2] == 0x72 && (*ActionData)[i + 3] == 0x2e && (*ActionData)[i + 4] == 0x78 && (*ActionData)[i + 5] == 0x00 )
		{
			// we think we've found an action with real time replay data (but we can't be 100% sure)
			// next we parse out two null terminated strings and a 4 byte integer

			if( ActionData->size( ) >= i + 7 )
			{
				// the first null terminated string should either be the strings "Data" or "Global" or a player id in ASCII representation, e.g. "1" or "2"

				Data = UTIL_ExtractCString( *ActionData, i + 6 );

				if( ActionData->size( ) >= i + 8 + Data.size( ) )
				{
					// the second null terminated string should be the key

					Key = UTIL_ExtractCString( *ActionData, i + 7 + Data.size( ) );

					if( ActionData->size( ) >= i + 12 + Data.size( ) + Key.size( ) )
					{
						// the 4 byte integer should be the value

						Value = BYTEARRAY( ActionData->begin( ) + i + 8 + Data.size( ) + Key.size( ), ActionData->begin( ) + i + 12 + Data.size( ) + Key.size( ) );
						string DataString = string( Data.begin( ), Data.end( ) );
						string KeyString = string( Key.begin( ), Key.end( ) );
						uint32_t ValueInt = UTIL_ByteArrayToUInt32( Value, false );
						// CONSOLE_Print( "[DOTA STATS] " + DataString + ", " + KeyString + ", " + UTIL_ToString( ValueInt ) );
						
						if( DataString == "Data" )
						{
							// these are received during the game
							// you could use these to calculate killing sprees and double or triple kills (you'd have to make up your own time restrictions though)
							// you could also build a table of "who killed who" data

							if( KeyString.size( ) >= 5 && KeyString.substr( 0, 4 ) == "Hero" )
							{
								// a hero died

								string VictimColourString = KeyString.substr( 4 );
								uint32_t VictimColour = UTIL_ToUInt32( VictimColourString );
								CGamePlayer *Killer = m_Game->GetPlayerFromColour( ValueInt );
								CGamePlayer *Victim = m_Game->GetPlayerFromColour( VictimColour );

								if( !m_Players[VictimColour] )
									m_Players[VictimColour] = new CDBDotAPlayer( );

								uint32_t GameNr = m_Game->GetGameNr();

								if (Victim)
								{
									Victim->SetDOTADeaths( Victim->GetDOTADeaths() + 1 );
									if(m_SwitchOff) m_Players[VictimColour]->SetDeaths(m_Players[VictimColour]->GetDeaths() + 1);
								}
								
								if (Killer && Killer == Victim ) // Just whant to check this ficher. Griffon
									CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player [" + Killer->GetName( ) + "] killed himself");

								if( Killer && Victim )
								{
									if ( (ValueInt < 6 && VictimColour < 6) || (ValueInt > 6 && VictimColour > 6) )
										CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player [" + Killer->GetName( ) + "] denied player [" + Victim->GetName( ) + "]" );
									else
									{
										Killer->SetDOTAKills(Killer->GetDOTAKills()+1);
										if(m_SwitchOff) m_Players[ValueInt]->SetKills(m_Players[ValueInt]->GetKills() + 1);
										CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player [" + Killer->GetName( ) + "] killed player [" + Victim->GetName( ) + "]" );
									}
								}
								else if( Victim )
								{
									if( ValueInt == 0 )
										CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] the Sentinel killed player [" + Victim->GetName( ) + "]" );
									else if( ValueInt == 6 )
										CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] the Scourge killed player [" + Victim->GetName( ) + "]" );
								}
							}
							else if( KeyString.size( ) >= 4 && KeyString.substr( 0, 6 ) == "Assist" )
							{
								// assist

								string AssistentColourString = KeyString.substr( 6 );
								uint32_t AssistentColour = UTIL_ToUInt32( AssistentColourString );
								CGamePlayer *Assistent = m_Game->GetPlayerFromColour( AssistentColour );
								CGamePlayer *Victim = m_Game->GetPlayerFromColour( ValueInt );

								if( !m_Players[AssistentColour] )
									m_Players[AssistentColour] = new CDBDotAPlayer( );
								
								if ( Assistent && Victim )
								{
									m_Players[AssistentColour]->SetAssists(m_Players[AssistentColour]->GetAssists() + 1);
									CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player [" + Assistent->GetName( ) + "] assist to kill player [" + Victim->GetName( ) + "]" );
								}
							}
							else if( KeyString.size( ) >= 4 && KeyString.substr( 0, 3 ) == "CSK" )
							{
								// kreep kills. Ñreep kills by the period in valueInt. Pereodic.

								string PlayerColourString = KeyString.substr( 3 );
								uint32_t PlayerColour = UTIL_ToUInt32( PlayerColourString );
								
								if ( m_SwitchOff )
								if( ( PlayerColour >= 1 && PlayerColour <= 5 ) || ( PlayerColour >= 7 && PlayerColour <= 11 ) )
									m_Players[PlayerColour]->SetCreepKills( ValueInt );

								
								//CGamePlayer *Player = m_Game->GetPlayerFromColour( PlayerColour );
								//string playerName = PlayerColourString; 
								//if ( Player )
								//	playerName = Player->GetName( );
								//It spams. Don't uncomment.
								//CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player [" + playerName + "] kills [" + UTIL_ToString(ValueInt) + "] kreeps.");
							}
							else if( KeyString.size( ) >= 3 && KeyString.substr( 0, 2 ) == "NK" )
							{
								// neutral kills. Neutral creep kills by the period in valueInt. Pereodic.

							//	CONSOLE_Print( "[STATSDOTA: debug neutral kills 1" );
								
								string PlayerColourString = KeyString.substr( 2 );
								uint32_t PlayerColour = UTIL_ToUInt32( PlayerColourString );
								
								if ( m_SwitchOff )
								if( ( PlayerColour >= 1 && PlayerColour <= 5 ) || ( PlayerColour >= 7 && PlayerColour <= 11 ) )
									m_Players[PlayerColour]->SetNeutralKills( ValueInt );

								
								//CGamePlayer *Player = m_Game->GetPlayerFromColour( PlayerColour );
								//string playerName = PlayerColourString; 
								//if ( Player )
								//	playerName = Player->GetName( );
								//It spams. Don't uncomment.
								//CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player [" + playerName + "] kills [" + UTIL_ToString(ValueInt) + "] neutral kreeps.");
							}
							else if( KeyString.size( ) >= 4 && KeyString.substr( 0, 3 ) == "CSD" )
							{
								// creep denies. Creep denies by the period in valueInt. Pereodic.

								string PlayerColourString = KeyString.substr( 3 );
								uint32_t PlayerColour = UTIL_ToUInt32( PlayerColourString );
								
								if ( m_SwitchOff )
								if( ( PlayerColour >= 1 && PlayerColour <= 5 ) || ( PlayerColour >= 7 && PlayerColour <= 11 ) )
									m_Players[PlayerColour]->SetCreepDenies( ValueInt );

								
								//CGamePlayer *Player = m_Game->GetPlayerFromColour( PlayerColour );
								//string playerName = PlayerColourString; 
								//if ( Player )
								//	playerName = Player->GetName( );
								//It spams. Don't uncomment.
								//CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player [" + playerName + "] denies [" + UTIL_ToString(ValueInt) + "] kreeps.");
							}
							else if( KeyString.size( ) >= 3 && KeyString.substr( 0, 4 ) == "PUI_" )
							{
/*								// Hero pick up an item.
								string PlayerColourString = KeyString.substr( 4 );
								uint32_t PlayerColour = UTIL_ToUInt32( PlayerColourString );
								CGamePlayer *Player = m_Game->GetPlayerFromColour( PlayerColour );
								string item = string( Value.rbegin(), Value.rend());
								string playerName = PlayerColourString; 
								if ( Player )
									playerName = Player->GetName( );

							//	CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player [" + playerName + "] pick up an item [" + UTIL_ToString(ValueInt) + "," + item + "]");
								
								if ( m_SwitchOff )
								{
									/*debug*/
/*									CDotaItem *it = m_AllItems->find(ValueInt);
									CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player [" + playerName + "] pick up an item ["+it->name+","+item+"].");

									/*end debug*/
/*									m_DotaItems[PlayerColour]->PickUpItem(ValueInt);
									vector<string> items = m_DotaItems[PlayerColour]->GetItems();
									bool error = true;
									for ( int i = 0; i < 6; i++ )
									{
										m_Players[PlayerColour]->SetItem( i,  items[i]);
										//CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player [" + playerName + "] slot ["+UTIL_ToString(i)+"] item ["+items[i]+"].");
									}
								}*/
								//It spams. Don't uncomment.
								//CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player [" + playerName + "] pick up an item [" + UTIL_ToString(ValueInt) + "].");
							}
							else if( KeyString.size( ) >= 3 && KeyString.substr( 0, 4 ) == "DRI_" )
							{
								// Hero drop an item.
/*
								string PlayerColourString = KeyString.substr( 4 );
								uint32_t PlayerColour = UTIL_ToUInt32( PlayerColourString );
								string item = string( Value.rbegin( ), Value.rend( ) );
								CGamePlayer *Player = m_Game->GetPlayerFromColour( PlayerColour );
								string playerName = PlayerColourString;
								if ( Player )
									playerName = Player->GetName( );

							//	CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player [" + playerName + "] pick up an item [" + UTIL_ToString(ValueInt) + "," + item + "]");
								
								if ( m_SwitchOff )
								{
									/*debug*/
/*									CDotaItem *it = m_AllItems->find(ValueInt);
									CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player [" + playerName + "] drop an item ["+it->name+","+item+"].");

									/*end debug*/
/*									m_DotaItems[PlayerColour]->DropItem(ValueInt);
									vector<string> items = m_DotaItems[PlayerColour]->GetItems();

									for ( int i = 0; i < 6; i++ )
									{
										m_Players[PlayerColour]->SetItem( i,  items[i]);
										//CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player [" + playerName + "] slot ["+UTIL_ToString(i)+"] item ["+items[i]+"].");
									}
								}*/
								//It spams. Don't uncomment.
								//CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player [" + playerName + "] drop an item [" + UTIL_ToString(ValueInt) + "].");
							}
							else if( KeyString.size( ) >= 8 && KeyString.substr( 0, 7 ) == "Courier" )
							{
								// a courier died

								if( ( ValueInt >= 1 && ValueInt <= 5 ) || ( ValueInt >= 7 && ValueInt <= 11 ) )
								{
									if( !m_Players[ValueInt] )
										m_Players[ValueInt] = new CDBDotAPlayer( );

									m_Players[ValueInt]->SetCourierKills( m_Players[ValueInt]->GetCourierKills( ) + 1 );
								}

								string VictimColourString = KeyString.substr( 7 );
								uint32_t VictimColour = UTIL_ToUInt32( VictimColourString );
								CGamePlayer *Killer = m_Game->GetPlayerFromColour( ValueInt );
								CGamePlayer *Victim = m_Game->GetPlayerFromColour( VictimColour );

								if( Killer && Victim )
									CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player [" + Killer->GetName( ) + "] killed a courier owned by player [" + Victim->GetName( ) + "]" );
								else if( Victim )
								{
									if( ValueInt == 0 )
										CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] the Sentinel killed a courier owned by player [" + Victim->GetName( ) + "]" );
									else if( ValueInt == 6 )
										CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] the Scourge killed a courier owned by player [" + Victim->GetName( ) + "]" );
								}
							}
							else if( KeyString.size( ) >= 8 && KeyString.substr( 0, 5 ) == "Tower" )
							{
								// a tower died

								if( ( ValueInt >= 1 && ValueInt <= 5 ) || ( ValueInt >= 7 && ValueInt <= 11 ) )
								{
									if( !m_Players[ValueInt] )
										m_Players[ValueInt] = new CDBDotAPlayer( );

									m_Players[ValueInt]->SetTowerKills( m_Players[ValueInt]->GetTowerKills( ) + 1 );
								}

								string Alliance = KeyString.substr( 5, 1 );
								string Level = KeyString.substr( 6, 1 );
								string Side = KeyString.substr( 7, 1 );
								CGamePlayer *Killer = m_Game->GetPlayerFromColour( ValueInt );
								string AllianceString;
								string SideString;
								string code_killed_tower = "";
								uint32_t place_code = 1;

								if( Alliance == "0" )
								{
									AllianceString = "Sentinel";
									place_code = 0;
								}
								else if( Alliance == "1" )
								{
									AllianceString = "Scourge";
									place_code = 3;
								}
								else
									AllianceString = "unknown";

								if( Side == "0" )
								{
									SideString = "top";
									code_killed_tower = UTIL_ToString(0 + place_code) + Level;
								}
								else if( Side == "1" )
								{
									SideString = "mid";
									code_killed_tower = UTIL_ToString(1 + place_code) + Level;
								}
								else if( Side == "2" )
								{
									SideString = "bottom";
									code_killed_tower = UTIL_ToString(2 + place_code) + Level;
								}
								else
									SideString = "unknown";

								if( AllianceString == "unknown" ||  SideString == "unknown")
									code_killed_tower = "";

								m_Game->m_KilledTowers = m_Game->m_KilledTowers + code_killed_tower;

								if( Killer )
									CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player [" + Killer->GetName( ) + "] destroyed a level [" + Level + "] " + AllianceString + " tower (" + SideString + ")" );
								else
								{
									if( ValueInt == 0 )
										CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] the Sentinel destroyed a level [" + Level + "] " + AllianceString + " tower (" + SideString + ")" );
									else if( ValueInt == 6 )
										CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] the Scourge destroyed a level [" + Level + "] " + AllianceString + " tower (" + SideString + ")" );
								}
							}
							else if( KeyString.size( ) >= 6 && KeyString.substr( 0, 3 ) == "Rax" )
							{
								// a rax died

								if( ( ValueInt >= 1 && ValueInt <= 5 ) || ( ValueInt >= 7 && ValueInt <= 11 ) )
								{
									if( !m_Players[ValueInt] ) // It must exist here in cause of "9" command sending in the start of the game. So why it here? @gpm
									{
										m_Players[ValueInt] = new CDBDotAPlayer( );
										m_Players[ValueInt]->SetColour( ValueInt );
									}
									m_Players[ValueInt]->SetRaxKills( m_Players[ValueInt]->GetRaxKills( ) + 1 );
								}

								string Alliance = KeyString.substr( 3, 1 );
								string Side = KeyString.substr( 4, 1 );
								string Type = KeyString.substr( 5, 1 );
								CGamePlayer *Killer = m_Game->GetPlayerFromColour( ValueInt );
								string AllianceString;
								string SideString;
								string TypeString;

								if( Alliance == "0" )
									AllianceString = "Sentinel";
								else if( Alliance == "1" )
									AllianceString = "Scourge";
								else
									AllianceString = "unknown";

								if( Side == "0" )
									SideString = "top";
								else if( Side == "1" )
									SideString = "mid";
								else if( Side == "2" )
									SideString = "bottom";
								else
									SideString = "unknown";

								if( Type == "0" )
									TypeString = "melee";
								else if( Type == "1" )
									TypeString = "ranged";
								else
									TypeString = "unknown";

								if( Killer )
									CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player [" + Killer->GetName( ) + "] destroyed a " + TypeString + " " + AllianceString + " rax (" + SideString + ")" );
								else
								{
									if( ValueInt == 0 )
										CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] the Sentinel destroyed a " + TypeString + " " + AllianceString + " rax (" + SideString + ")" );
									else if( ValueInt == 6 )
										CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] the Scourge destroyed a " + TypeString + " " + AllianceString + " rax (" + SideString + ")" );
								}
							}
							else if( KeyString.size( ) >= 6 && KeyString.substr( 0, 6 ) == "Throne" )
							{
								// the frozen throne got hurt

								CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] the Frozen Throne is now at " + UTIL_ToString( ValueInt ) + "% HP" );
							}
							else if( KeyString.size( ) >= 4 && KeyString.substr( 0, 4 ) == "Tree" )
							{
								// the world tree got hurt

								CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] the World Tree is now at " + UTIL_ToString( ValueInt ) + "% HP" );
							}
							else if( KeyString.size( ) >= 5 && KeyString.substr( 0, 2 ) == "CK")
							{
								// a player disconnected

								if ((ValueInt >= 1 && ValueInt <= 5 ) || ( ValueInt >= 7 && ValueInt <= 11 ))
								{
									uint32_t i = KeyString.find("D");
									uint32_t y = KeyString.find("N");

									string CreepKillsString = KeyString.substr( 2, i-2 );
									string CreepDeniesString = KeyString.substr( i+1, y-i-1 );
									string NeutralKillsString = KeyString.substr( y+1 );
									uint32_t CreepKills = UTIL_ToUInt32( CreepKillsString );
									uint32_t CreepDenies = UTIL_ToUInt32( CreepDeniesString );
									uint32_t NeutralKills = UTIL_ToUInt32( NeutralKillsString );
									
									if (!m_Players[ValueInt])
									{
										m_Players[ValueInt] = new CDBDotAPlayer( );
										m_Players[ValueInt]->SetColour( ValueInt );
									}

									m_Players[ValueInt]->SetCreepKills( CreepKills );
									m_Players[ValueInt]->SetCreepDenies( CreepDenies );
									m_Players[ValueInt]->SetNeutralKills( NeutralKills );
								}
								
								CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] player disconnected." );
							}
							else if( KeyString.size( ) >= 9 && KeyString == "GameStart" )
							{
								// Zero time in the game, creeps spawn.
								if (ValueInt == 1)
								{
									m_Game->SetCreepSpawnTime(GetTime( ));
									CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] creeps spawned." );
								}
							}
							else if( KeyString.size( ) >= 8 && KeyString.substr( 0, 4 ) == "SWAP" )
							{

							// swap players
								int i = KeyString.find( "_") + 1;
								int y = KeyString.find( "_", i );
								string FromString = KeyString.substr( i, y-i );
								uint32_t FromColour = UTIL_ToUInt32( FromString );
								CGamePlayer *FromPlayer = m_Game->GetPlayerFromColour( FromColour );
								string ToString = KeyString.substr( y + 1 );
								uint32_t ToColour = UTIL_ToUInt32( ToString );
								CGamePlayer *ToPlayer = m_Game->GetPlayerFromColour( ToColour );
								
								if( m_SwitchOff)
								if ((FromColour >= 1 && FromColour <= 5 ) || ( FromColour >= 7 && FromColour <= 11 ))
								if ((ToColour >= 1 && ToColour <= 5 ) || ( ToColour >= 7 && ToColour <= 11 ))
								{
									m_Players[ToColour]->SetNewColour( FromColour );
									m_Players[FromColour]->SetNewColour( ToColour );
									
									CDBDotAPlayer* bufferPlayer = m_Players[ToColour];
									m_Players[ToColour] = m_Players[FromColour];
									m_Players[FromColour] = bufferPlayer;
									
									if ( FromPlayer ) FromString = FromPlayer->GetName( );
									if ( ToPlayer ) ToString = ToPlayer->GetName( );
									
									CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] swap players from ["+FromString+"] to ["+ToString+"]." );
								}
								
							}
							else if( KeyString.size( ) >= 4 && KeyString.substr( 0, 4 ) == "Mode" ) //Modearso Modensom
							{
								// Game mode
								string gameMode = KeyString.substr(4);
								// If SO mod, we must know about that.
								if (gameMode.find("nsom") != string::npos )
									gameMode.erase(gameMode.find("nsom"), 4);
								if (gameMode.find("so") != string::npos )
										m_SwitchOff = false;

								m_SwitchOff = true;
							
							}
						}
						else if( DataString == "Global" )
						{
							// these are only received at the end of the game

							if( KeyString == "Winner" )
							{
								// Value 1 -> sentinel
								// Value 2 -> scourge

								m_Winner = ValueInt;

								if( m_Winner == 1 )
									CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] detected winner: Sentinel" );
								else if( m_Winner == 2 )
									CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] detected winner: Scourge" );
								else
									CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] detected winner: " + UTIL_ToString( ValueInt ) );
							}
							else if( KeyString == "m" )
								m_Min = ValueInt;
							else if( KeyString == "s" )
								m_Sec = ValueInt;
						}
						else if( DataString.size( ) <= 2 && DataString.find_first_not_of( "1234567890" ) == string :: npos )
						{
							// these are only received at the end of the game

							uint32_t ID = UTIL_ToUInt32( DataString );

							if( ( ID >= 1 && ID <= 5 ) || ( ID >= 7 && ID <= 11 ) )
							{
								if( !m_Players[ID] )
								{
									m_Players[ID] = new CDBDotAPlayer( );
									m_Players[ID]->SetColour( ID );
								}
								// Key "1"		-> Kills
								// Key "2"		-> Deaths
								// Key "3"		-> Creep Kills
								// Key "4"		-> Creep Denies
								// Key "5"		-> Assists
								// Key "6"		-> Current Gold
								// Key "7"		-> Neutral Kills
								// Key "8_0"	-> Item 1
								// Key "8_1"	-> Item 2
								// Key "8_2"	-> Item 3
								// Key "8_3"	-> Item 4
								// Key "8_4"	-> Item 5
								// Key "8_5"	-> Item 6
								// Key "id"		-> ID (1-5 for sentinel, 6-10 for scourge, accurate after using -sp and/or -switch)

								if( KeyString == "1" )
									m_Players[ID]->SetKills( ValueInt );
								else if( KeyString == "2" )
									m_Players[ID]->SetDeaths( ValueInt );
								else if( KeyString == "3" )
									m_Players[ID]->SetCreepKills( ValueInt );
								else if( KeyString == "4" )
									m_Players[ID]->SetCreepDenies( ValueInt );
								else if( KeyString == "5" )
									m_Players[ID]->SetAssists( ValueInt );
								else if( KeyString == "6" )
									m_Players[ID]->SetGold( ValueInt );
								else if( KeyString == "7" )
									m_Players[ID]->SetNeutralKills( ValueInt );
								else if( KeyString == "8_0" )
									m_Players[ID]->SetItem( 0, string( Value.rbegin( ), Value.rend( ) ) );
								else if( KeyString == "8_1" )
									m_Players[ID]->SetItem( 1, string( Value.rbegin( ), Value.rend( ) ) );
								else if( KeyString == "8_2" )
									m_Players[ID]->SetItem( 2, string( Value.rbegin( ), Value.rend( ) ) );
								else if( KeyString == "8_3" )
									m_Players[ID]->SetItem( 3, string( Value.rbegin( ), Value.rend( ) ) );
								else if( KeyString == "8_4" )
									m_Players[ID]->SetItem( 4, string( Value.rbegin( ), Value.rend( ) ) );
								else if( KeyString == "8_5" )
									m_Players[ID]->SetItem( 5, string( Value.rbegin( ), Value.rend( ) ) );
								else if( KeyString == "9" )
									m_Players[ID]->SetHero( string( Value.rbegin( ), Value.rend( ) ) );
								else if( KeyString == "id" )
								{
									// DotA sends id values from 1-10 with 1-5 being sentinel players and 6-10 being scourge players
									// unfortunately the actual player colours are from 1-5 and from 7-11 so we need to deal with this case here

									if( ValueInt >= 6 )
										m_Players[ID]->SetNewColour( ValueInt + 1 );
									else
										m_Players[ID]->SetNewColour( ValueInt );
								}
							}
						}

						i += 12 + Data.size( ) + Key.size( );
					}
					else
						i++;
				}
				else
					i++;
			}
			else
				i++;
		}
		else
			i++;
	}

	return m_Winner != 0;
}

void CStatsDOTA :: Save( CGHost *GHost, CGHostDB *DB, uint32_t GameID )
{
	if( DB->Begin( ) )
	{
		// since we only record the end game information it's possible we haven't recorded anything yet if the game didn't end with a tree/throne death
		// this will happen if all the players leave before properly finishing the game
		// the dotagame stats are always saved (with winner = 0 if the game didn't properly finish)
		// the dotaplayer stats are only saved if the game is properly finished

		unsigned int Players = 0;

		// save the dotagame

		GHost->m_Callables.push_back( DB->ThreadedDotAGameAdd( GameID, m_Winner, m_Min, m_Sec ) );

		// check for invalid colours and duplicates
		// this can only happen if DotA sends us garbage in the "id" value but we should check anyway

		for( unsigned int i = 0; i < 12; i++ )
		{
			if( m_Players[i] )
			{
				uint32_t Colour = m_Players[i]->GetNewColour( );

				if( !( ( Colour >= 1 && Colour <= 5 ) || ( Colour >= 7 && Colour <= 11 ) ) )
				{
					CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] discarding player data, invalid colour found" );
					DB->Commit( );
					return;
				}

				for( unsigned int j = i + 1; j < 12; j++ )
				{
					if( m_Players[j] && Colour == m_Players[j]->GetNewColour( ) )
					{
						CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] discarding player data, duplicate colour found" );
						DB->Commit( );
						return;
					}
				}
			}
		}

		// save the dotaplayers

		for( unsigned int i = 0; i < 12; i++ )
		{
			if( m_Players[i] && m_Winner )
			{
				GHost->m_Callables.push_back( DB->ThreadedDotAPlayerAdd( GameID, m_Players[i]->GetColour( ), m_Players[i]->GetKills( ), m_Players[i]->GetDeaths( ), m_Players[i]->GetCreepKills( ), m_Players[i]->GetCreepDenies( ), m_Players[i]->GetAssists( ), m_Players[i]->GetGold( ), m_Players[i]->GetNeutralKills( ), m_Players[i]->GetItem( 0 ), m_Players[i]->GetItem( 1 ), m_Players[i]->GetItem( 2 ), m_Players[i]->GetItem( 3 ), m_Players[i]->GetItem( 4 ), m_Players[i]->GetItem( 5 ), m_Players[i]->GetHero( ), m_Players[i]->GetNewColour( ), m_Players[i]->GetTowerKills( ), m_Players[i]->GetRaxKills( ), m_Players[i]->GetCourierKills( ) ) );
				Players++;
			}
		}

		if( DB->Commit( ) )
			CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] saving " + UTIL_ToString( Players ) + " players" );
		else
			CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] unable to commit database transaction, data not saved" );
	}
	else
		CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] unable to begin database transaction, data not saved" );
}

void CStatsDOTA :: SetWinner( uint32_t winner )
{
	m_Winner = winner;
}

void CStatsDOTA :: SwitchProcess( uint32_t FromColour, uint32_t ToColour )
{
	CGamePlayer *FromPlayer = m_Game->GetPlayerFromColour( FromColour );
	CGamePlayer *ToPlayer = m_Game->GetPlayerFromColour( ToColour );

	string FromString, ToString; FromString.clear(); ToString.clear();

	if ( FromPlayer ) FromString = FromPlayer->GetName( );
	if ( ToPlayer ) ToString = ToPlayer->GetName( );

	CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] Switch FromColor " + UTIL_ToString(FromColour)+ " ToColour "+UTIL_ToString(ToColour) +" FromName " + FromString + " ToName "+ ToString );
		
	if ((FromColour >= 1 && FromColour <= 5 ) || ( FromColour >= 7 && FromColour <= 11 ))
	if ((ToColour >= 1 && ToColour <= 5 ) || ( ToColour >= 7 && ToColour <= 11 ))
	{
		CDBDotAPlayer* bufferPlayer = m_Players[ToColour];
		m_Players[ToColour] = m_Players[FromColour];
		m_Players[FromColour] = bufferPlayer;

		string FromString, ToString; FromString.clear(); ToString.clear();

		if ( FromPlayer ) FromString = FromPlayer->GetName( );
		if ( ToPlayer ) ToString = ToPlayer->GetName( );
								

		CONSOLE_Print( "[STATSDOTA: " + m_Game->GetGameName( ) + "] Switch complete From " + FromString + " To " + ToString );
	}
								
}	