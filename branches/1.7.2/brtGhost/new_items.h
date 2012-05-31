/*

   Copyright [2010] [Grigoriy Orekhov <gpm@ukr.net>]

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

#ifndef DOTAITEMS_H
#define DOTAITEMS_H

#include "includes.h"
#include "itemlist.h"

//
// CDotaItemRecipe
//

class CDotaItemRecipe 
{
	private:
		multimap<uint32_t, char> m_Items;
		uint32_t m_Count;
		uint32_t m_Counter;
		CDotaItemList* m_AllItems;
		
	public:
		uint32_t m_ReturnedItem;
		CDotaItemRecipe(uint32_t nItem, CDotaItemList* nAllItems);			   // nItem - result of recipe
		~CDotaItemRecipe();							   //
		vector<uint32_t> PickUpItem (uint32_t nItem);  // drop an item from some storage
		void DropItem (uint32_t nItem);                // pick up an item to some storage
		void AddItem (uint32_t nItem);                 // add item to recipe
		vector<uint32_t> GetItems () { };
};

//
// CDotaItems
//

class CDotaItems
{
	private:
		multimap<uint32_t, CDotaItemRecipe*> m_ItemRecipes;
		CDotaItem m_Items[6];
		CDotaItemList* m_AllItems;
		
		bool PickUpPItem (uint32_t nItem);  // pick up to m_Items
		bool DropPItem (uint32_t nItem);    // drop from m_Items
		void AddRecipe(CDotaItemRecipe* nRecipe);
		
	public:
		CDotaItems(CDotaItemList* nAllItems);
		~CDotaItems();
		bool PickUpItem (uint32_t nItem);   // return true if recipe works or item increment.
		bool DropItem (uint32_t nItem);		// return true if recipe works or item decrement.
		vector<string> GetItems();
};

class CDotaItems_669 : public CDotaItems {
	CDotaItems_669(CDotaItemList* nAllItems) : CDotaItemList(nAllItems);
}

class CDotaItems_670 : public CDotaItems_669 {
	CDotaItems_670(CDotaItemList* nAllItems) : CDotaItems_669(nAllItems);
}

#endif
