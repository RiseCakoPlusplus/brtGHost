#include "includes.h"

#include "configdata.h"

void CConfig::Parse( const string& nFileName )
{
    try
    {
        read_info(nFileName, data);

        const boost::property_tree::ptree& server = data.get_child("server");

        client_lastversion              = server.get<string>("version", "");
		command_trigger					= server.get<char>("command_trigger", '/');

        client_updater_directory         = server.get<string>("autoupdater.localpath", "");
        client_updater_download_httppath = server.get<string>("autoupdater.download_httppath", "");
        client_download_localpath        = server.get<string>("autoupdater.download_localpath", "");

		war3path = server.get<string>("system.war3path", "");
			
		update_dota_elo = server.get<bool>("dota_elo.enable", false);
		update_after_end_of_game = server.get<bool>("dota_elo.update_after_end_of_game", false);
		update_every_seconds = server.get<uint32_t>("dota_elo.update_every_seconds", 0);
        
		BOOST_FOREACH (const boost::property_tree::ptree::value_type& base,
            data.get_child("server.battle_net"))
        {
            const boost::property_tree::ptree& battlenet = base.second;

            if ( battlenet.get<bool>("enable", false) )
            {
                CBNetConfig bnet;

				bnet.enabled = battlenet.get<bool>("enable", false);
				bnet.tft	 = battlenet.get<bool>("TFT", true);
				bnet.cdkeyroc = battlenet.get<string>("cdkeyroc", "");
				bnet.cdkeytft = battlenet.get<string>("cdkeytft", "");
				bnet.bnet_server   = battlenet.get<string>("server", "");
				bnet.username = battlenet.get<string>("username", "");
				bnet.password = battlenet.get<string>("password", "");
				bnet.channel  = battlenet.get<string>("channel", "");
				bnet.exeversion		= battlenet.get<string>("exeversion", "");
				bnet.exeversionhash	= battlenet.get<string>("exeversionhash", "");
				bnet.passwordhashtype = battlenet.get<string>("passwordhashtype", "");

				bnet.bnls_server = battlenet.get<string>("bnls_server", "");
				bnet.bnls_port = battlenet.get<uint32_t>("bnls_port", 0);
				bnet.bnls_wardercookie = battlenet.get<uint32_t>("bnls_wardercookie", 0); 

				bnet.war3version = battlenet.get<uint16_t>("war3version", 26);
				bnet.port = battlenet.get<uint32_t>("port", 6112);

                m_BNetList.push_back(bnet);
            
				cout << "[CONFIG] BNET " << base.first.data() << " config data succesfully parsed." << endl;
            }
		}




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

					botdata.name = bot.first.data();
                    botdata.bot_channel = values.get<string>("channel", "");
                    botdata.bot_command_port = values.get<int>("command_port", 8200);
                    botdata.bot_gameport = values.get<int>("gameport", 6112);
                    botdata.bot_ip = values.get<string>("ip", "127.0.0.1");
					botdata.password = values.get<string>("password", "");
                    botdata.can_create_game = values.get<bool>("can_create_game", false);
                    botdata.SetSocket( NULL );

                    nBotList.push_back(botdata);
                
					cout << "[CONFIG] Bot " << bot.first.data() << " config data succesfully parsed." << endl;
                }
            }
        }
    }
	catch (boost::property_tree::info_parser_error& error)
	{
		std::cout 
			<< "[CONFIG] " << error.message() << " " << error.filename() << " at line "
			<< error.line() << std::endl;
	}
}

void CConfig :: Save( const string& nFileName )
{
   write_info(nFileName, data);
}