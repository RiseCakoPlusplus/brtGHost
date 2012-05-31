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

#include "items.h"
#include "util.h"
#include <algorithm>

//
// CDotaItemRecipe
//

CDotaItemRecipe::CDotaItemRecipe (uint32_t nItem, CDotaItemList* nAllItems)
{
	m_Count = 0;
	m_Counter = 0;
	m_ReturnedItem = nItem;
	m_AllItems = nAllItems;
};

CDotaItemRecipe::~CDotaItemRecipe ()
{
	m_Items.clear();
};


vector<uint32_t> CDotaItemRecipe::PickUpItem (uint32_t nItem)
{
	vector<uint32_t> ret;
	for (multimap<uint32_t, char>::iterator it = m_Items.begin(); it != m_Items.end(); it++)
	{
		if ( (*it).first == nItem )
		{
			(*it).second++;
			m_Counter++;
			break;
		}
	}
	
	if (m_Counter == m_Count)
	{	
		//create vector with an items that we need to drop where the last item it is an item we need to pick up.
		for (multimap<uint32_t, char>::iterator it = m_Items.begin(); it != m_Items.end(); it++)
			ret.push_back((*it).first);
		
		ret.push_back(m_ReturnedItem);
		return ret;
	}
	return ret;
};

void CDotaItemRecipe::DropItem (uint32_t nItem)
{
	for (multimap<uint32_t, char>::iterator it = m_Items.begin(); it != m_Items.end(); it++)
	{
		if ( (*it).first == nItem && (*it).second > 1)
		{
			if (--((*it).second) == 0) m_Counter--;
			break;
		}
	}
};

void CDotaItemRecipe::AddItem (uint32_t nItem)
{
	m_Items.insert( pair<uint32_t, char>(nItem, 0) );
	m_Count++;
};

vector<uint32_t> CDotaItemRecipe::GetItems()
{
	vector<uint32_t> ret;
	multimap<uint32_t, char>::iterator it;
	for(it = m_Items.begin(); it != m_Items.end(); it++)
		ret.push_back(it->first);
	return ret;
};

//
// CDotaItems
//

CDotaItems::CDotaItems (CDotaItemList* nAllItems)
{
	m_AllItems = nAllItems;
	
	m_Items[0] = CDotaItem();
	m_Items[1] = CDotaItem();
	m_Items[2] = CDotaItem();
	m_Items[3] = CDotaItem();
	m_Items[4] = CDotaItem();
	m_Items[5] = CDotaItem();
	
};

CDotaItems::~CDotaItems ()
{
	multimap<uint32_t, CDotaItemRecipe*>::iterator it;
	vector<CDotaItemRecipe*> buf;
	for(it = m_ItemRecipes.begin(); it != m_ItemRecipes.end(); it++)
	{
		if (find(buf.begin(), buf.end(), it->second) == buf.end())
		{
			buf.push_back(it->second);
			delete it->second;
		}
	}
	m_ItemRecipes.clear();
	buf.clear();
};

void CDotaItems::AddRecipe (CDotaItemRecipe* nRecipe)
{
	vector<uint32_t> items = nRecipe.GetItems();
	vector<uint32_t>::iterator it;
	while(!items.empty())
	{
		m_ItemRecipes.insert( pair<uint32_t, CDotaItemRecipe*>(items.back(), nRecipe) );
		items.pop_back();
	}
};

bool CDotaItems::PickUpItem (uint32_t nItem)
{
	vector<uint32_t> items;
	multimap<uint32_t, CDotaItemRecipe*>::iterator it, it2;
	pair<multimap<uint32_t, CDotaItemRecipe*>::iterator, multimap<uint32_t, CDotaItemRecipe*>::iterator> ret;
	ret = m_ItemRecipes.equal_range(nItem);
	// update recipe list info and check for a building of a new item.
	for (it = ret.first; it != ret.second; ++it)
	{
		items = (*it).second->PickUpItem(nItem);
		if (!items.empty()) // we have a new item
		{
			uint32_t newItem = items.back();
			items.pop_back();
			// drop all items that in recipe
			while(!items.empty())
			{
				// we cant drop an item that we just try to pick up
				if(items.back() == nItem)
				{
					// but we just do incriment for other recipes, and now we need to do it back.
					for (it2 = ret.first; it2 != it; it2++) //it2!=it cause not for all, but only for recipes that go befor current recipe.
						(*it2).second->DropItem(nItem);
				}else
					DropItem(items.back());
				items.pop_back();
			}
			PickUpItem(newItem);
			return true;
		}
	}
	// Pick up a new item
	return PickUpPItem(nItem);
};

bool CDotaItems::PickUpPItem (uint32_t nItem)
{
	for (int i = 0; i < 6; i++)
	{
		// if we already have this item and it stackable.
		if (m_Items[i].value == nItem && m_Items[i].stackable)
		{
			return true;
		}
	}
	for (int i = 0; i < 6; i++)
	{
		if (m_Items[i].value == 0)
		{
			m_Items[i] = CDotaItem(m_AllItems->find(nItem));
			return false;
		}
	}
	CONSOLE_Print( "[CDotaItems:PickUpPItem] ERRRROR... we have no place and no increment.");
	return false;
};

bool CDotaItems::DropItem (uint32_t nItem)
{
	multimap<uint32_t, CDotaItemRecipe*>::iterator it, it2;
	pair<multimap<uint32_t, CDotaItemRecipe*>::iterator, multimap<uint32_t, CDotaItemRecipe*>::iterator> ret;
	ret = m_ItemRecipes.equal_range(nItem);
	for (it = ret.first; it != ret.second; ++it)
	{
		(*it).second->DropItem(nItem);
	}
	return DropPItem(nItem);
};

bool CDotaItems::DropPItem (uint32_t nItem)
{
//	CONSOLE_Print( "[CDotaItems:DropPItem] Start to frop ["+m_AllItems->find(nItem)->name+"].");
	for (int i = 0; i < 6; i++)
	{
		if (m_Items[i].value == nItem)
		{
				m_Items[i] = CDotaItem();
//				CONSOLE_Print( "[CDotaItems:DropPItem] End with drop.");
				return false;
		}
	}
//	CONSOLE_Print( "[CDotaItems:DropPItem] no such item.");
	return false;
};

vector<string> CDotaItems::GetItems( )
{
	vector<string> ret;
	for (int i = 0; i < 6; i++)
	{
		if (m_Items[i].value > 0)
		{
			BYTEARRAY b = UTIL_CreateByteArray( m_Items[i].value, false );
			ret.push_back(string(b.rbegin(), b.rend()));
		}
		else
			ret.push_back(string( ));
//		CONSOLE_Print( "[CDotaItems:GetItem] slot["+UTIL_ToString(i)+"], item["+m_Items[i].name+"].");
	}
	return ret;
};

//
// CDotaItems 6.69
//

CDotaItems_669::CDotaItems_669(CDotaItemList* nAllItems) : CDotaItems(nAllItems)
{
	// Recipes definition
	CDotaItemRecipe *recipe;
	// Magic Wand
	recipe = new CDotaItemRecipe(1227900994, m_AllItems);
	recipe->AddItem(1227900739);
	recipe->AddItem(1227895879);
	recipe->AddItem(1227895879);
	recipe->AddItem(1227895879);
	recipe->AddItem(1227900983);
	AddRecipe(recipe);
	// Null of talisman
	recipe = new CDotaItemRecipe(1227896396, m_AllItems);
	recipe->AddItem(1227896152);
	recipe->AddItem(1227895385);
	recipe->AddItem(1227895882);
	AddRecipe(recipe);
	// Wraith band
	recipe = new CDotaItemRecipe(1227896394, m_AllItems);
	recipe->AddItem(1227896151);
	recipe->AddItem(1227895385);
	recipe->AddItem(1227895897);
	AddRecipe(recipe);
	// Bracer
	recipe = new CDotaItemRecipe(1227896392, m_AllItems);
	recipe->AddItem(1227896150);
	recipe->AddItem(1227895385);
	recipe->AddItem(1227895875);
	AddRecipe(recipe);
	// Poor Man's Shield
	recipe = new CDotaItemRecipe(1227901766, m_AllItems);
	recipe->AddItem(1227896113);
	recipe->AddItem(1227895897);
	recipe->AddItem(1227895897);
	AddRecipe(recipe);
	// Perseverance
	recipe = new CDotaItemRecipe(1227896370, m_AllItems);
	recipe->AddItem(1227896116);
	recipe->AddItem(1227895892);
	AddRecipe(recipe);
	// Oblivion staff
	recipe = new CDotaItemRecipe(1227896370, m_AllItems);
	recipe->AddItem(1227895891);
	recipe->AddItem(1227895895);
	recipe->AddItem(1227895898);
	AddRecipe(recipe);
	// Hand of Midas
	recipe = new CDotaItemRecipe(1227896388, m_AllItems);
	recipe->AddItem(1227896149);
	recipe->AddItem(1227895376);
	AddRecipe(recipe);
	// Soul Ring
	recipe = new CDotaItemRecipe(1227902028, m_AllItems);
	recipe->AddItem(1227902030);
	recipe->AddItem(1227895894);
	recipe->AddItem(1227895898);
	AddRecipe(recipe);
	// Power Threads of Strength 
	recipe = new CDotaItemRecipe(1227896153, m_AllItems);
	recipe->AddItem(1227895375);
	recipe->AddItem(1227895376);
	recipe->AddItem(1227895859);
	AddRecipe(recipe);
	// Power Threads of Agility 
	recipe = new CDotaItemRecipe(1227895380, m_AllItems);
	recipe->AddItem(1227895375);
	recipe->AddItem(1227895376);
	recipe->AddItem(1227895378);
	AddRecipe(recipe);
	// Power Threads of Magic
	recipe = new CDotaItemRecipe(1227896154, m_AllItems);
	recipe->AddItem(1227895375);
	recipe->AddItem(1227895376);
	recipe->AddItem(1227895895);
	AddRecipe(recipe);
	// Phase Boots 
	recipe = new CDotaItemRecipe(1227900746, m_AllItems);
	recipe->AddItem(1227895375);
	recipe->AddItem(1227895861);
	recipe->AddItem(1227895861);
	AddRecipe(recipe);
	// Headdress of Rejuvenation
	recipe = new CDotaItemRecipe(1227896371, m_AllItems);
	recipe->AddItem(1227895894);
	recipe->AddItem(1227895879);
	recipe->AddItem(1227896146);
	AddRecipe(recipe);
	// Urn of Shadows 
	recipe = new CDotaItemRecipe(1227901785, m_AllItems);
	recipe->AddItem(1227901782);
	recipe->AddItem(1227895898);
	recipe->AddItem(1227895875);
	recipe->AddItem(1227895875);
	AddRecipe(recipe);
	// Khadgar's Pipe of Insight
	recipe = new CDotaItemRecipe(1227901750, m_AllItems);
	recipe->AddItem(1227901018);
	recipe->AddItem(1227896371);
	recipe->AddItem(1227899469);
	AddRecipe(recipe);
	// Ring of Basilius 
	recipe = new CDotaItemRecipe(1227896375, m_AllItems);
	recipe->AddItem(1227895893);
	recipe->AddItem(1227895898);
	AddRecipe(recipe);
	// Nathrezim Buckler
	recipe = new CDotaItemRecipe(1227896374, m_AllItems);
	recipe->AddItem(1227896147);
	recipe->AddItem(1227895863);
	recipe->AddItem(1227895879);
	AddRecipe(recipe);
	// Arcane boots
	recipe = new CDotaItemRecipe(1227902281, m_AllItems);
	recipe->AddItem(1227895375);
	recipe->AddItem(1227895874);
	AddRecipe(recipe);
	// Vladmir's Offering
	recipe = new CDotaItemRecipe(1227899463, m_AllItems);
	recipe->AddItem(1227896375);
	recipe->AddItem(1227896898);
	recipe->AddItem(1227895883);
	recipe->AddItem(1227895894);
	AddRecipe(recipe);
	// Mekansm
	recipe = new CDotaItemRecipe(1227897139, m_AllItems);
	recipe->AddItem(1227896627);
	recipe->AddItem(1227896371);
	recipe->AddItem(1227896374);
	AddRecipe(recipe);
	// Refresher orb
	recipe = new CDotaItemRecipe(1227899210, m_AllItems);
	recipe->AddItem(1227896888);
	recipe->AddItem(1227896390);
	recipe->AddItem(1227896370);
	AddRecipe(recipe);
	// Aghanim's Scepter 1
	recipe = new CDotaItemRecipe(1227894833, m_AllItems);
	recipe->AddItem(1227895890);
	recipe->AddItem(1227896112);
	recipe->AddItem(1227895860);
	recipe->AddItem(1227895887);
	// Aghanim's Scepter 2
	/*

	recipe = new CDotaItemRecipe(1227894833, m_AllItems);

	recipe->AddItem(1227895890);

	recipe->AddItem(1227895887);

	recipe->AddItem(1227895860);

	// Aghanim's Scepter 3

	recipe = new CDotaItemRecipe(1227894833, m_AllItems);

	recipe->AddItem(1227895890);

	recipe->AddItem(1227895887);

	recipe->AddItem(1227896112);

	*/
	AddRecipe(recipe);
	// Boots of Travel
	recipe = new CDotaItemRecipe(1227896386, m_AllItems);
	recipe->AddItem(1227895375);
	recipe->AddItem(1227896148);
	AddRecipe(recipe);
	// Necromicon 1
	recipe = new CDotaItemRecipe(1227897172, m_AllItems);
	recipe->AddItem(1227896881);
	recipe->AddItem(1227896112);
	recipe->AddItem(1227895859);
	AddRecipe(recipe);
	// Necromicon 2
	recipe = new CDotaItemRecipe(1227897176, m_AllItems);
	recipe->AddItem(1227896881);
	recipe->AddItem(1227897172);
	AddRecipe(recipe);
	// Necromicon 3
	recipe = new CDotaItemRecipe(1227897177, m_AllItems);
	recipe->AddItem(1227896881);
	recipe->AddItem(1227897176);
	AddRecipe(recipe);
	// Dagon 1
	recipe = new CDotaItemRecipe(1227897165, m_AllItems);
	recipe->AddItem(1227896880);
	recipe->AddItem(1227896112);
	recipe->AddItem(1227896396);
	AddRecipe(recipe);
	// Dagon 2
	recipe = new CDotaItemRecipe(1227897168, m_AllItems);
	recipe->AddItem(1227896880);
	recipe->AddItem(1227897165);
	AddRecipe(recipe);
	// Dagon 3
	recipe = new CDotaItemRecipe(1227897166, m_AllItems);
	recipe->AddItem(1227896880);
	recipe->AddItem(1227897168);
	AddRecipe(recipe);
	// Dagon 4
	recipe = new CDotaItemRecipe(1227897167, m_AllItems);
	recipe->AddItem(1227896880);
	recipe->AddItem(1227897166);
	AddRecipe(recipe);
	// Dagon 5
	recipe = new CDotaItemRecipe(1227897164, m_AllItems);
	recipe->AddItem(1227896880);
	recipe->AddItem(1227897167);
	AddRecipe(recipe);
	// Force Staff
	recipe = new CDotaItemRecipe(1227901001, m_AllItems);
	recipe->AddItem(1227900998);
	recipe->AddItem(1227895891);
	recipe->AddItem(1227896112);
	AddRecipe(recipe);
	// Eul's Scepter of Divinity
	recipe = new CDotaItemRecipe(1227896922, m_AllItems);
	recipe->AddItem(1227896626);
	recipe->AddItem(1227896112);
	recipe->AddItem(1227896116);
	recipe->AddItem(1227895898);
	AddRecipe(recipe);
	// Orchid Malevolence
	recipe = new CDotaItemRecipe(1227895090, m_AllItems);
	recipe->AddItem(1227896390);
	recipe->AddItem(1227896390);
	recipe->AddItem(1227896390);
	AddRecipe(recipe);
	// Guinsoo's Scythe of Vyse
	recipe = new CDotaItemRecipe(1227899212, m_AllItems);
	recipe->AddItem(1227895886);
	recipe->AddItem(1227896114);
	recipe->AddItem(1227896116);
	AddRecipe(recipe);
	// Hood of Defiance /1
	recipe = new CDotaItemRecipe(1227899469, m_AllItems);
	recipe->AddItem(1227895888);
	recipe->AddItem(1227895877);
	recipe->AddItem(1227895892);
	AddRecipe(recipe);
	// Hood of Defiance /2
	recipe = new CDotaItemRecipe(1227899469, m_AllItems);
	recipe->AddItem(1227895888);
	recipe->AddItem(1227895877);
	recipe->AddItem(1227895894);
	recipe->AddItem(1227895894);
	AddRecipe(recipe);
	// Soul Booster
	recipe = new CDotaItemRecipe(1227897137, m_AllItems);
	recipe->AddItem(1227896115);
	recipe->AddItem(1227895890);
	recipe->AddItem(1227895874);
	AddRecipe(recipe);
	// Blade Mail 
	recipe = new CDotaItemRecipe(1227896911, m_AllItems);
	recipe->AddItem(1227895862);
	recipe->AddItem(1227895863);
	recipe->AddItem(1227895895);
	AddRecipe(recipe);
	// Vanguard
	recipe = new CDotaItemRecipe(1227902019, m_AllItems);
	recipe->AddItem(1227896113);
	recipe->AddItem(1227895892);
	recipe->AddItem(1227896115);
	AddRecipe(recipe);
	// Linken's Sphere
	recipe = new CDotaItemRecipe(1227897178, m_AllItems);
	recipe->AddItem(1227896882);
	recipe->AddItem(1227896370);
	recipe->AddItem(1227896114);
	AddRecipe(recipe);
	// Bloodstone
	recipe = new CDotaItemRecipe(1227899467, m_AllItems);
	recipe->AddItem(1227896370);
	recipe->AddItem(1227897137);
	AddRecipe(recipe);
	// Shiva's Guard
	recipe = new CDotaItemRecipe(1227894874, m_AllItems);
	recipe->AddItem(1227896901);
	recipe->AddItem(1227895889);
	recipe->AddItem(1227895886);
	AddRecipe(recipe);
	// Black King Bar
	recipe = new CDotaItemRecipe(1227900506, m_AllItems);
	recipe->AddItem(1227896664);
	recipe->AddItem(1227895887);
	recipe->AddItem(1227895885);
	AddRecipe(recipe);
	// Heart of Tarrasque
	recipe = new CDotaItemRecipe(1227899193, m_AllItems);
	recipe->AddItem(1227896884);
	recipe->AddItem(1227895884);
	recipe->AddItem(1227896115);
	AddRecipe(recipe);
	// Assault Cuirass
	recipe = new CDotaItemRecipe(1227899466, m_AllItems);
	recipe->AddItem(1227896899);
	recipe->AddItem(1227895889);
	recipe->AddItem(1227895878);
	recipe->AddItem(1227895863);
	AddRecipe(recipe);
	// Diffusal Blade
	recipe = new CDotaItemRecipe(1227896914, m_AllItems);
	recipe->AddItem(1227896409);
	recipe->AddItem(1227895860);
	recipe->AddItem(1227895860);
	recipe->AddItem(1227895895);
	AddRecipe(recipe);
	// Mask Of Madness
	recipe = new CDotaItemRecipe(1227896921, m_AllItems);
	recipe->AddItem(1227896410);
	recipe->AddItem(1227895883);
	AddRecipe(recipe);
	// Yasha
	recipe = new CDotaItemRecipe(1227896903, m_AllItems);
	recipe->AddItem(1227896399);
	recipe->AddItem(1227895860);
	recipe->AddItem(1227895378);
	AddRecipe(recipe);
	// Stygian Desolator
	recipe = new CDotaItemRecipe(1227897143, m_AllItems);
	recipe->AddItem(1227896662);
	recipe->AddItem(1227895885);
	recipe->AddItem(1227895885);
	AddRecipe(recipe);
	// Maelstorm
	recipe = new CDotaItemRecipe(1227896912, m_AllItems);
	recipe->AddItem(1227896406);
	recipe->AddItem(1227895885);
	recipe->AddItem(1227895376);
	AddRecipe(recipe);
	// Helm of the Dominator 
	recipe = new CDotaItemRecipe(1227896916, m_AllItems);
	recipe->AddItem(1227895877);
	recipe->AddItem(1227895883);
	AddRecipe(recipe);
	// Sange
	recipe = new CDotaItemRecipe(1227896904, m_AllItems);
	recipe->AddItem(1227896400);
	recipe->AddItem(1227895887);
	recipe->AddItem(1227895859);
	AddRecipe(recipe);
	// Eye of Skadi
	recipe = new CDotaItemRecipe(1227899205, m_AllItems);
	recipe->AddItem(1227896114);
	recipe->AddItem(1227896114);
	recipe->AddItem(1227895890);
	recipe->AddItem(1227902265);
	AddRecipe(recipe);
	// Mjollnir
	recipe = new CDotaItemRecipe(1227899461, m_AllItems);
	recipe->AddItem(1227901774);
	recipe->AddItem(1227896912);
	recipe->AddItem(1227895878);
	AddRecipe(recipe);
	// Satanic 
	recipe = new CDotaItemRecipe(1227899202, m_AllItems);
	recipe->AddItem(1227896885);
	recipe->AddItem(1227896916);
	recipe->AddItem(1227895884);
	AddRecipe(recipe);
	// Sange and Yasha 
	recipe = new CDotaItemRecipe(1227897141, m_AllItems);
	recipe->AddItem(1227896904);
	recipe->AddItem(1227896903);
	AddRecipe(recipe);
	// Ethereal Blade 
	recipe = new CDotaItemRecipe(1227902036, m_AllItems);
	recipe->AddItem(1227895873);
	recipe->AddItem(1227901513);
	AddRecipe(recipe);
	// Lothar's Edge
	recipe = new CDotaItemRecipe(1227897160, m_AllItems);
	recipe->AddItem(1227896666);
	recipe->AddItem(1227895864);
	recipe->AddItem(1227895891);
	AddRecipe(recipe);
	// Armlet of Mordiggian
	recipe = new CDotaItemRecipe(1227894865, m_AllItems);
	recipe->AddItem(1227896900);
	recipe->AddItem(1227895877);
	recipe->AddItem(1227895376);
	recipe->AddItem(1227895861);
	AddRecipe(recipe);
	// Crystalys
	recipe = new CDotaItemRecipe(1227897155, m_AllItems);
	recipe->AddItem(1227896663);
	recipe->AddItem(1227895862);
	recipe->AddItem(1227895861);
	AddRecipe(recipe);
	// Manta Style
	recipe = new CDotaItemRecipe(1227897158, m_AllItems);
	recipe->AddItem(1227896665);
	recipe->AddItem(1227896903);
	recipe->AddItem(1227896114);
	AddRecipe(recipe);
	// Battle Fury
	recipe = new CDotaItemRecipe(1227897145, m_AllItems);
	recipe->AddItem(1227895864);
	recipe->AddItem(1227895862);
	recipe->AddItem(1227896370);
	AddRecipe(recipe);
	// Cranium Basher 
	recipe = new CDotaItemRecipe(1227896907, m_AllItems);
	recipe->AddItem(1227896403);
	recipe->AddItem(1227896117);
	recipe->AddItem(1227895859);
	AddRecipe(recipe);
	// Buriza-do Kyanon
	recipe = new CDotaItemRecipe(1227899187, m_AllItems);
	recipe->AddItem(1227896883);
	recipe->AddItem(1227895865);
	recipe->AddItem(1227897155);
	AddRecipe(recipe);
	// The Butterfly
	recipe = new CDotaItemRecipe(1227899209, m_AllItems);
	recipe->AddItem(1227895873);
	recipe->AddItem(1227901494);
	recipe->AddItem(1227895891);
	AddRecipe(recipe);
	// Radiance
	recipe = new CDotaItemRecipe(1227899192, m_AllItems);
	recipe->AddItem(1227896646);
	recipe->AddItem(1227895896);
	AddRecipe(recipe);
	// Monkey King Bar
	recipe = new CDotaItemRecipe(1227899189, m_AllItems);
	recipe->AddItem(1227896117);
	recipe->AddItem(1227896117);
	recipe->AddItem(1227895865);
	AddRecipe(recipe);
	// Divine Rapier
	recipe = new CDotaItemRecipe(1227899185, m_AllItems);
	recipe->AddItem(1227895865);
	recipe->AddItem(1227895896);
	AddRecipe(recipe);
};

//
// CDotaItems 6.70
//

CDotaItems_670::CDotaItems_670(CDotaItemList* nAllItems) : CDotaItems_669(nAllItems)
{

};

