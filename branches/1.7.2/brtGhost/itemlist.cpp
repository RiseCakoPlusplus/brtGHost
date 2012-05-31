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

#include "items.h"

//
// CDotaItemList
//

CDotaItemList::CDotaItemList( )
{

};

CDotaItem* CDotaAllItems::find(uint32_t nItem)
{ 
	map<uint32_t, CDotaItem>::iterator it;
	it =  m_AllItems.find(nItem);
	if (it == m_AllItems.end())
		add(nItem, "UnknownItem", 0);
		return find(nItem);
	CDotaItem* dotaItem = &(it->second);
	return dotaItem;
};

//
// CDotaItemList 6.69
//

CDotaItemList_669::CDotaItemList_669( )
{
	add(1227900983, "Recipe Magic Wand", 0);
	add(1227900994, "Magic Wand", 0);
	add(1227900739, "Magic stick", 0);
	add(1227895879, "Ironwood branch", 0);
	add(1227896152, "Recipe Null of talisman", 0);
	add(1227896396, "Null of talisman", 0);
	add(1227895385, "Circlet of Nobility", 0);
	add(1227895882, "Mante of intelligence", 0);
	add(1227896151, "Recipe Wraith band", 0);
	add(1227896394, "Wraith band", 0);
	add(1227895897, "Slippers of Agility", 0);
	add(1227896150, "Recipe Bracer", 0);
	add(1227896392, "Bracer", 0);
	add(1227895875, "Gauntlets of Strength", 0);
	add(1227901766, "Poor Man's Shield", 0);
	add(1227896113, "Stout Shield", 0);
	add(1227896370, "Perseverance", 0);
	add(1227896116, "Void Stone", 0);
	add(1227895892, "Ring of Health", 0);
	add(1227896390, "Oblivion staff", 0);
	add(1227895891, "Quarterstaff", 0);
	add(1227895895, "Robe of Magi", 0);
	add(1227895898, "Sobi Mask", 0);
	add(1227896149, "Recipe Hand of Midas", 0);
	add(1227896388, "Hand of Midas", 0);
	add(1227895376, "Gloves of haste", 0);
	add(1227902030, "Recipe Soul Ring", 0);
	add(1227902028, "Soul Ring", 0);
	add(1227895894, "Ring of Regeneration", 0);
	add(1227895380, "Power Threads of Agility ", 0);
	add(1227895375, "Boots Of Speed", 0);
	add(1227895376, "Gloves of haste", 0);
	add(1227895378, "Boots of Elvenskin", 0);
	add(1227896153, "Power Threads of Strength", 0);
	add(1227895859, "Belt of Giant Strength", 0);
	add(1227896154, "Power Threads of Magic", 0);
	add(1227900746, "Phase Boots ", 0);
	add(1227895861, "Blades of Attack", 0);
	add(1227896146, "Recipe Headdress of Rejuvenation", 0);
	add(1227896371, "Headdress of Rejuvenation", 0);
	add(1227901782, "Recipe Urn of Shadows", 0);
	add(1227901785, "Urn of Shadows", 0);
	add(1227901018, "Recipe Khadgar's Pipe of Insight", 0);
	add(1227901750, "Khadgar's Pipe of Insight", 0);
	add(1227899469, "Hood of Defiance", 0);
	add(1227896375, "Ring of Basilius", 0);
	add(1227895893, "Ring of protection", 0);
	add(1227896147, "Recipe Nathrezim Buckler", 0);
	add(1227896374, "Nathrezim Buckler", 0);
	add(1227895863, "Chainmail", 0);
	add(1227902281, "Arcane boots", 0);
	add(1227895874, "Energy Booster", 0);
	add(1227896898, "Recipe Vladmir's Offering", 0);
	add(1227899463, "Vladmir's Offering", 0);
	add(1227895883, "Mask of Death", 0);
	add(1227896627, "Recipe Mekansm", 0);
	add(1227897139, "Mekansm", 0);
	add(1227896888, "Recipe Refresher orb", 0);
	add(1227899210, "Refresher orb", 0);
	add(1227901236, "Recipe Aghanim's Scepter", 0);
	add(1227894833, "Aghanim's Scepter", 0);
	add(1227895890, "Point Booster", 0);
	add(1227896112, "Staff of Wizardry", 0);
	add(1227895860, "Blade of Alacrity", 0);
	add(1227895887, "Ogre Axe", 0);
	add(1227896881, "Recipe Necromicon", 0);
	add(1227897172, "Necromicon 0", 0);
	add(1227897176, "Necromicon 2", 0);
	add(1227897177, "Necromicon 3", 0);
	add(1227896880, "Recipe Dagon", 0);
	add(1227897165, "Dagon 0", 0);
	add(1227897168, "Dagon 2", 0);
	add(1227897166, "Dagon 3", 0);
	add(1227897167, "Dagon 4", 0);
	add(1227897164, "Dagon 5", 0);
	add(1227900998, "Recipe Force Staff", 0);
	add(1227901001, "Force Staff", 0);
	add(1227896626, "Recipe Eul's Scepter of Divinity", 0);
	add(1227896922, "Eul's Scepter of Divinity", 0);
	add(1227895090, "Orchid Malevolence", 0);
	add(1227899212, "Guinsoo's Scythe of Vyse", 0);
	add(1227896114, "Ultimate Orb", 0);
	add(1227895886, "Mystic Staff", 0);
	add(1227895888, "Planeswalker's Cloak", 0);
	add(1227895877, "Helm of Iron Will", 0);
	add(1227896115, "Vitality Booster", 0);
	add(1227897137, "Soul Booster", 0);
	add(1227895862, "Broadsword", 0);
	add(1227896911, "Blade Mail", 0);
	add(1227902019, "Vanguard", 0);
	add(1227896882, "Linken's Sphere Recipe", 0);
	add(1227897178, "Linken's Sphere", 0);
	add(1227899467, "Bloodstone", 0);
	add(1227896901, "Shiva's Guard Recipe", 0);
	add(1227895889, "Plate Mail", 0);
	add(1227894874, "Shiva's Guard", 0);
	add(1227896664, "Black King Bar Recipe", 0);
	add(1227895885, "Mathril Mahher", 0);
	add(1227900506, "Black King Bar", 0);
	add(1227896884, "Heart of Tarrasque Recipe", 0);
	add(1227895884, "Messerschmidt's Reaver", 0);
	add(1227899193, "Heart of Tarrasque", 0);
	add(1227896899, "Assault Cuirass Recipe", 0);
	add(1227899466, "Assault Cuirass", 0);
	add(1227895878, "Hyperstone", 0);
	add(1227896409, "Diffusal Blade Recipe", 0);
	add(1227896914, "Diffusal Blade", 0);
	add(1227896410, "Mask Of Madness Recipe", 0);
	add(1227896921, "Mask Of Madness", 0);
	add(1227896399, "Yasha Recipe", 0);
	add(1227896903, "Yasha", 0);
	add(1227896662, "Stygian Desolator Recipe", 0);
	add(1227897143, "Stygian Desolator", 0);
	add(1227896406, "Maelstorm Recipe", 0);
	add(1227896912, "Maelstorm", 0);
	add(1227896916, "Helm of the Dominator", 0);
	add(1227896400, "Sange Recipe", 0);
	add(1227896904, "Sange", 0);
	add(1227899205, "Eye of Skadi", 0);
	add(1227902265, "Orb of Venom", 0);
	add(1227901774, "Mjollnir Recipe", 0);
	add(1227899461, "Mjollnir", 0);
	add(1227896885, "Satanic Recipe", 0);
	add(1227899202, "Satanic", 0);
	add(1227897141, "Sange and Yasha", 0);
	add(1227902036, "Ethereal Blade", 0);
	add(1227895873, "Eaglehorn", 0);
	add(1227901513, "Ghost Scepter", 0);
	add(1227896666, "Lothar's Edge Recipe", 0);
	add(1227895864, "Claymore", 0);
	add(1227897160, "Lothar's Edge", 0);
	add(1227896900, "Armlet of Mordiggian Recipe", 0);
	add(1227894865, "Armlet of Mordiggian", 0);
	add(1227896663, "Crystalys Recipe", 0);
	add(1227897155, "Crystalys", 0);
	add(1227896665, "Manta Style Recipe", 0);
	add(1227897158, "Manta Style", 0);
	add(1227897145, "Battle Fury", 0);
	add(1227896403, "Cranium Basher Recipe", 0);
	add(1227896907, "Cranium Basher", 0);
	add(1227896117, "Javelin", 0);
	add(1227896883, "Buriza-do Kyanon Recipe", 0);
	add(1227899187, "Buriza-do Kyanon", 0);
	add(1227895865, "Demon Edge", 0);
	add(1227899209, "The Butterfly", 0);
	add(1227901494, "Talisman of Avansion", 0);
	add(1227896646, "Radiance Recipe", 0);
	add(1227899192, "Radiance", 0);
	add(1227895896, "Sacred Relic", 0);
	add(1227899189, "Monkey King Bar", 0);
	add(1227899185, "Divine Rapier", 0);
	add(1227895876, "Gem of True Sight", 0);
	add(1227895880, "Kelen's Dagger", 0);
	add(1227896132, "Clarity Potion", 1);
	add(1227896134, "Healing Salve", 1);
	add(1227896131, "Ancient Tango of Essifation", 1);
	add(1227899222, "Empty Bottle Store", 0); // IOAV
	add(1227899216, "3/3 Bottle", 0);  // IOAP
	add(1227896135, "Observer Wards", 1);
	add(1227896136, "Sentry Wards", 1);
	add(1227900744, "Dust of Appearance", 1);
	add(1227896138, "Animal Courier", 0);
	add(1227896137, "Scroll of Town Portal", 1);
	add(1227901010, "Quelling Blade", 0);
	add(1227896148, "Boots of Travel Recipe", 0);
	add(1227896386, "Boots of Travel", 0);
};

//
// CDotaItemList 6.70
//

CDotaItemList_670::CDotaItemList_670( ) : CDotaItemList_669 ()
{

};


