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

#ifndef STATSDOTA_H
#define STATSDOTA_H

#include "items.h"

//
// CStatsDOTA
//

class CDBDotAPlayer;

class CStatsDOTA : public CStats
{
private:
	CDBDotAPlayer *m_Players[12];
	CDotaItems *m_DotaItems[12];
	CDotaAllItems *m_AllItems;
	uint32_t m_Winner;
	uint32_t m_Min;
	uint32_t m_Sec;
	bool m_Swap;
	bool m_SwitchOff;

public:
	CStatsDOTA( CBaseGame *nGame );
	virtual ~CStatsDOTA( );

	virtual bool ProcessAction( CIncomingAction *Action );
	virtual void Save( CGHost *GHost, CGHostDB *DB, uint32_t GameID );
	virtual void SetWinner( uint32_t winner );
	virtual void SwitchProcess( uint32_t FromColour, uint32_t ToColour );
};

#endif
