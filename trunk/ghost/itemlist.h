/*

   Copyright [2011] [Grigoriy Orekhov <gpm@ukr.net>]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

*/

#ifndef ITEMLIST_H
#define ITEMLIST_H

#include "includes.h"

//
// CDotaItem
//

struct CDotaItem {
	CDotaItem ( ) {value=0; name=string(); stackable=false;};
	CDotaItem (CDotaItem* p) {value=p->value; name=p->name; stackable=p->stackable;};
	CDotaItem (uint32_t nVal, string nName, bool nStackable) {value=nVal; name=nName; stackable=nStackable;};
	
	uint32_t value;
	string name;
	bool stackable;
};

//
// CDotaItemList
//

class CDotaItemList
{
	private:
		map<uint32_t, CDotaItem> m_AllItems;
		void add (uint32_t nItem, string name, uint32_t max_count) {m_AllItems.insert(pair<uint32_t, CDotaItem>(nItem, CDotaItem(nItem, name, max_count)));};
	public:
		CDotaItemList ( );
		~CDotaItemList ( ) { m_AllItems.clear(); };
		CDotaItem*  find (uint32_t nItem);
		map<uint32_t, CDotaItem> GetAllItems () {return m_AllItems;};
};

class CDotaItemList_669 : public CDotaItemList {
	CDotaItemList_669 ( );
};

class CDotaItemList_670 : public CDotaItemList_669 {
	CDotaItemList_670 ( ) : CDotaItemList_669 ();
};


#endif
