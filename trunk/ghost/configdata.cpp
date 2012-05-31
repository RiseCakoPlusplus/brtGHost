/*
    Copyright [2011] [Igor Bygaev] [http://brt.org.ua]
	
	email: avon.dn.ua@gmail.com
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#include "includes.h"
#include "util.h"
#include "configdata.h"

using boost::property_tree::ptree;

CConfigData::CConfigData(void)
{
}

CConfigData::~CConfigData(void)
{
}

bool CConfigData::Parse( const string& nFileName )
{
    try
    {
        read_info(nFileName, data);

        const boost::property_tree::ptree& ghost = data.get_child("ghost");

		port_host		= ghost.get<uint32_t>("ports.host", 6112);
		port_reconnect	= ghost.get<uint32_t>("ports.reconnect", 6113);
		port_command	= ghost.get<uint32_t>("ports.command", 8100);
		port_admingame	= ghost.get<uint32_t>("ports.admingame", 6114);

		logfile = ghost.get<string>("system.log", "ghost.log");
		logmethod = ghost.get<int>("system.logmethod", 1);

		/*
		const boost::property_tree::ptree& battlenet = data.get_child("server.battle_net");
		
		connect_to_battle_net = battlenet.get<bool>("enable", false);
		war3path = battlenet.get<string>("war3path", "");
		cdkeyroc = battlenet.get<string>("cdkeyroc", "");
		cdkeytft = battlenet.get<string>("cdkeytft", "");
		bnet_server   = battlenet.get<string>("server", "");
		username = battlenet.get<string>("username", "");
		password = battlenet.get<string>("password", "");
		channel  = battlenet.get<string>("channel", "");
		exeversion		= battlenet.get<string>("exeversion", "");
		exeversionhash	= battlenet.get<string>("exeversionhash", "");
		passwordhashtype = battlenet.get<string>("passwordhashtype", "");
		war3path = battlenet.get<string>("war3path", "");

		bnls_server = battlenet.get<string>("bnls_server", "");
		bnls_port = battlenet.get<uint32_t>("bnls_port", 0);
		bnls_wardercookie = battlenet.get<uint32_t>("bnls_wardercookie", 0); 

		war3version = battlenet.get<uint16_t>("war3version", 26);
		port = battlenet.get<uint32_t>("port", 6112);

        BOOST_FOREACH (const boost::property_tree::ptree::value_type& base,
            data.get_child("server.databases"))
        {
            const boost::property_tree::ptree& values = base.second;

            if (const boost::optional<std::string> optionalComment =
                    values.get_optional<std::string>("comment"))
            {
                std::cout << optionalComment.get() << endl;
            } else
            {
                if ( static_cast<string>(base.first.data()) == "mysql" )
                {
                    dbServer = values.get<string>("host", "localhost");
                    dbDatabase = values.get<string>("database", "");
                    dbUser = values.get<string>("user", "");
                    dbPassword = values.get<string>("password", "");
                    Port = values.get<int>("port", 0);
                }
 //               else if ( static_cast<string>(base.first.data()) == "redis" )
 //               {
//                    redis_password = values.get<string>("password", "");
//                    redis_host = values.get<string>("host", "localhost");
//                    redis_port = values.get<int>("port", 6378 );
//                }

                std::cout << "[CONFIG] " << base.first.data() << " database config succesfully parsed. " << endl;
            }
        }

        BOOST_FOREACH (const boost::property_tree::ptree::value_type& bot,
            data.get_child("server.bots"))
        {
            const boost::property_tree::ptree& values = bot.second;

            if (const boost::optional<std::string> optionalComment =
                    values.get_optional<std::string>("comment"))
            {
                std::cout << optionalComment.get() << endl;
            } else
            {
                if ( values.get<bool>("enabled", false) )
                {
                    CBotData botdata;

                    botdata.bot_channel = values.get<string>("channel", "");
                    botdata.bot_command_port = values.get<int>("command_port", 8200);
                    botdata.bot_gameport = values.get<int>("gameport", 6112);
                    botdata.bot_ip = values.get<string>("ip", "127.0.0.1");
                    botdata.can_create_game = values.get<bool>("can_create_game", false);
                    botdata.SetSocket( NULL );

                    nBotList.push_back(botdata);
          
					cout << "[CONFIG] Bot " << bot.first.data() << " config data succesfully parsed." << endl;
                }
            }
        }
		*/
    }
	catch (boost::property_tree::info_parser_error& error)
	{
		CONSOLE_Print( "[CONFIG] " + error.message() + " " + error.filename() + " at line " + UTIL_ToString(error.line()) );
		CONSOLE_Print( "[GHOST] shutdown due the invalid config file" );

		return true;
	}

	return false;
}

bool CConfigData::Save( const string& nFileName )
{
   // Create an empty property tree object

   /*
   ptree pt;

   // Put log filename in property tree
   pt.put("debug.filename", m_file);

   // Put debug level in property tree
   pt.put("debug.level", m_level);

   // Iterate over the modules in the set and put them in the
   // property tree. Note that the put function places the new
   // key at the end of the list of keys. This is fine most of
   // the time. If you want to place an item at some other place
   // (i.e. at the front or somewhere in the middle), this can
   // be achieved using a combination of the insert and put_own
   // functions.
   BOOST_FOREACH(const std::string &name, m_modules)
      pt.put("debug.modules.module", name, true);

   // Write the property tree to the XML file.
   */
   write_info(nFileName, data);

   return true;
}