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
#include "packed.h"
#include "savegame.h"

//
// CSaveGame
//

CSaveGame :: CSaveGame( ) : CPacked( )
{
	m_NumSlots = 0;
	m_RandomSeed = 0;
}

CSaveGame :: CSaveGame( string nFileName, string nFileNameNoPath, string nMapPath, string nGameName, unsigned char nNumSlots, vector<CGameSlot> nSlots, vector<CGamePlayer*> nPIDs, uint32_t nRandomSeed, BYTEARRAY nCRC ) : CPacked( )
{
	m_FileName = nFileName;
	m_FileNameNoPath = nFileNameNoPath;
	m_MapPath = nMapPath;
	m_GameName = nGameName;
	m_NumSlots = nNumSlots;
	m_Slots = nSlots;
	m_PIDs = nPIDs;
	m_RandomSeed = nRandomSeed;
	m_MagicNumber = nCRC;
}

CSaveGame :: ~CSaveGame( )
{

}

#define READB( x, y, z )	(x).read( (char *)(y), (z) )
#define READSTR( x, y )		getline( (x), (y), '\0' )

string customMarkFileType = "none";

void CSaveGame :: PrepareForSave()
{
	// Custom W3Z file format type header
	// by freed

	BYTEARRAY packet;

	UTIL_AppendByteArray( packet, m_MapPath );
	UTIL_AppendByteArray( packet, customMarkFileType, true );
	UTIL_AppendByteArray( packet, m_GameName );
	UTIL_AppendByteArray( packet, customMarkFileType, true );
	UTIL_AppendByteArray( packet, customMarkFileType, true );

	UTIL_AppendByteArray( packet, (uint32_t)0, false );
	UTIL_AppendByteArray( packet, (uint32_t)0, false );
	UTIL_AppendByteArray( packet, (uint16_t)0, false );

	packet.push_back ( m_Slots.size() );

	for( unsigned char i = 0; i < m_Slots.size(); i++ )
	{
		packet.push_back( m_Slots[i].GetPID() );
		packet.push_back( m_Slots[i].GetDownloadStatus() );
		packet.push_back( m_Slots[i].GetSlotStatus() );
		packet.push_back( m_Slots[i].GetComputer() );
		packet.push_back( m_Slots[i].GetTeam() );
		packet.push_back( m_Slots[i].GetColour() );
		packet.push_back( m_Slots[i].GetRace() );
		packet.push_back( m_Slots[i].GetComputerType() );
		packet.push_back( m_Slots[i].GetHandicap() );
	}
	
	UTIL_AppendByteArray( packet, (uint32_t)m_RandomSeed, false );

	packet.push_back( 0 ); // GameType
	packet.push_back( m_PIDs.size() ); // number of player slots (non observer)

	UTIL_AppendByteArray( packet, m_MagicNumber );

	packet.push_back( m_PIDs.size() );

	for( vector<CGamePlayer*>::iterator i = m_PIDs.begin(); i != m_PIDs.end(); ++i )
	{
		packet.push_back( (*i)->GetPID() );
		UTIL_AppendByteArray( packet, (*i)->GetName(), true );
	}

    m_Decompressed = string( packet.begin( ), packet.end( ) );
}

void CSaveGame :: ParseSaveGame( )
{
	m_MapPath.clear( );
	m_GameName.clear( );
	m_NumSlots = 0;
	m_Slots.clear( );
	m_RandomSeed = 0;
	m_MagicNumber.clear( );
	m_EnforcePlayers.clear();

	if( m_Flags != 0 )
	{
		CONSOLE_Print( "[SAVEGAME] invalid replay (flags mismatch)" );
		m_Valid = false;
		return;
	}

	istringstream ISS( m_Decompressed );

	// savegame format figured out by Varlock:
	// string		-> map path
	// 0 (string?)	-> ??? (no idea what this is)
	// string		-> game name
	// 0 (string?)	-> ??? (maybe original game password)
	// string		-> stat string
	// 4 bytes		-> ??? (seems to be # of slots)
	// 4 bytes		-> ??? (seems to be 0x01 0x28 0x49 0x00 on both of the savegames examined)
	// 2 bytes		-> ??? (no idea what this is)
	// slot structure
	// 4 bytes		-> magic number

	unsigned char Garbage1;
	uint16_t Garbage2;
	uint32_t Garbage4;
	string GarbageString;
	string idString;
	uint32_t MagicNumber;

	READSTR( ISS, m_MapPath );				// map path
	READSTR( ISS, idString );				// ??? , mark for custom save game file
	READSTR( ISS, m_GameName );				// game name
	READSTR( ISS, GarbageString );			// ???
	READSTR( ISS, GarbageString );			// stat string
	READB( ISS, &Garbage4, 4 );				// ???
	READB( ISS, &Garbage4, 4 );				// ???
	READB( ISS, &Garbage2, 2 );				// ???
	READB( ISS, &m_NumSlots, 1 );			// number of slots

	if( m_NumSlots > 12 )
	{
		CONSOLE_Print( "[SAVEGAME] invalid savegame (too many slots)" );
		m_Valid = false;
		return;
	}

	for( unsigned char i = 0; i < m_NumSlots; i++ )
	{
		unsigned char SlotData[9];
		READB( ISS, SlotData, 9 );			// slot data
		m_Slots.push_back( CGameSlot( SlotData[0], SlotData[1], SlotData[2], SlotData[3], SlotData[4], SlotData[5], SlotData[6], SlotData[7], SlotData[8] ) );
	}

	READB( ISS, &m_RandomSeed, 4 );			// random seed
	READB( ISS, &Garbage1, 1 );				// GameType
	READB( ISS, &Garbage1, 1 );				// number of player slots (non observer)
	READB( ISS, &MagicNumber, 4 );			// magic number

	if( ISS.eof( ) || ISS.fail( ) )
	{
		CONSOLE_Print( "[SAVEGAME] failed to parse savegame header" );
		m_Valid = false;
		return;
	}

	m_MagicNumber = UTIL_CreateByteArray( MagicNumber, false );

	if (idString == customMarkFileType)
	{
		unsigned char nPIDsize;

		READB( ISS, &nPIDsize, 1 );
		m_EnforcePlayers.resize( nPIDsize );

		for ( unsigned char i = 0; i < nPIDsize; ++i)
		{
			unsigned char PID;
			string playerName;

			READB( ISS, &PID, 1 );
			READSTR( ISS, playerName );

			m_EnforcePlayers[i] = PIDPlayer(PID, playerName);
		}

	}

	m_Valid = true;
}
