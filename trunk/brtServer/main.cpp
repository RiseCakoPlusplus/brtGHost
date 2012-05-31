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

#include "redisclient.h"
#include "includes.h"
#include "util.h"
#include "socket.h"
#include "server.h"
#include "updater.h"
#include "sha1.h"
#include "md5.h"
#include "user.h"

#include <boost/date_time.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <boost/filesystem.hpp>

#include <iostream>

#include <iterator>
#include <vector>
#include <algorithm>
#include <map>

#include "zlib/zlib.h"

string dbServer;
string dbDatabase;
string dbUser;
string dbPassword;
string bot_ips;
string bot_ports;
string bot_admin_list;

string redis_host;
int redis_port;
string redis_password;

string client_lastversion;

string client_updater_directory;
string client_updater_download_httppath;
string client_download_localpath;

const string defaultCFGFile = "default.cfg";

int Port;
MYSQL *Connection = NULL;
CUpdater* updater;

deque<CBotData> nBotList;
list<CUser*> DBUsersList;
list<CUser*> UsersList;

boost::shared_ptr<redis::client> redis_client;

void loadconfig( string& nFileName )
{
    try
    {
        boost::property_tree::ptree data;
        read_info(nFileName, data);

        const boost::property_tree::ptree& server = data.get_child("server");

        client_lastversion              = server.get<string>("version", "");

        client_updater_directory         = server.get<string>("autoupdater.localpath", "");
        client_updater_download_httppath = server.get<string>("autoupdater.download_httppath", "");
        client_download_localpath        = server.get<string>("autoupdater.download_localpath", "");

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
                else if ( static_cast<string>(base.first.data()) == "redis" )
                {
                    redis_password = values.get<string>("password", "");
                    redis_host = values.get<string>("host", "localhost");
                    redis_port = values.get<int>("port", 6378 );
                }

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

                    cout << "[CONFIG] Bot " << bot.first.data() << "(" << values.get<string>("name", static_cast<string>(bot.first.data())) << ") config data succesfully parsed." << endl;
                }


            }

        }
    }
    catch (const boost::property_tree::ptree_bad_data& error)
    {
        std::cout << "[CONFIG] Parse error: " << error.what() << std::endl;
    }
    catch (const boost::property_tree::ptree_bad_path& error)
    {
        std::cout << "[CONFIG] Parse error: " << error.what() << std::endl;
    }
}

void initRedisClient()
{
    try
    {
        redis_client = boost::shared_ptr<redis::client>( new redis::client(redis_host, redis_port) );
    }
    catch (redis::redis_error & e)
    {
        cerr << "[REDIS] error: " << e.what() << endl << "FAIL" << endl;
    }

    redis_client->auth(redis_password);
    redis_client->select(0);
}



void CONSOLE_Print( string message )
{
    cout << message << endl;
}

uint32_t GetTicks( )
{
#ifdef WIN32
	// don't use GetTickCount anymore because it's not accurate enough (~16ms resolution)
	// don't use QueryPerformanceCounter anymore because it isn't guaranteed to be strictly increasing on some systems and thus requires "smoothing" code
	// use timeGetTime instead, which typically has a high resolution (5ms or more) but we request a lower resolution on startup
	return timeGetTime( );

	//return GetTickCount();
#elif __APPLE__
	uint64_t current = mach_absolute_time( );
	static mach_timebase_info_data_t info = { 0, 0 };
	// get timebase info
	if( info.denom == 0 )
		mach_timebase_info( &info );
	uint64_t elapsednano = current * ( info.numer / info.denom );
	// convert ns to ms
	return elapsednano / 1e6;
#else
	uint32_t ticks;
	struct timespec t;
	clock_gettime( CLOCK_MONOTONIC, &t );
	ticks = t.tv_sec * 1000;
	ticks += t.tv_nsec / 1000000;
	return ticks;
#endif
}

uint32_t GetTime( )
{
	return GetTicks( ) / 1000;
}

string MySQLEscapeString( MYSQL *conn, string str )
{
	char *to = new char[str.size( ) * 2 + 1];
	unsigned long size = mysql_real_escape_string( conn, to, str.c_str( ), str.size( ) );
	string result( to, size );
	delete [] to;
	return result;
}

vector<string> MySQLFetchRow( MYSQL_RES *res )
{
	vector<string> Result;

	MYSQL_ROW Row = mysql_fetch_row( res );

	if( Row )
	{
		unsigned long *Lengths;
		Lengths = mysql_fetch_lengths( res );

		for( unsigned int i = 0; i < mysql_num_fields( res ); i++ )
		{
			if( Row[i] )
				Result.push_back( string( Row[i], Lengths[i] ) );
			else
				Result.push_back( string( ) );
		}

	}

	return Result;
}

int ConvertItems()
{
   	cout << "[SERVER] Converting items from mysql to redis database." << endl;

    string Query = "SELECT * FROM items;";

	if( mysql_real_query( Connection, Query.c_str( ), Query.size( ) ) != 0 )
	{
		cout << "error: " << mysql_error( Connection ) << endl;
		return 1;
	}
	else
	{
		MYSQL_RES *Result = mysql_store_result( Connection );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			while( !Row.empty( ) )
			{
                if ( !Row[0].empty())
                {
                    string itemCode = Row[0];
                    string itemName1 = Row[1];
                    string itemName2 = Row[2];
                    string itemFileName = Row[3];

                    cout << "[ITEM] " << itemCode << " -> " << itemFileName << endl;

                    try
                    {
                        redis_client->hset("DOTA_DATA:item:" + itemCode, "filename", itemFileName);
                        redis_client->hset("DOTA_DATA:item:" + itemCode, "name", itemName1);
                        redis_client->hset("DOTA_DATA:item:" + itemCode, "shortname", itemName2);

                    } catch (redis::redis_error & e)
					{
						//cerr << "[REDIS] " << e.what() << endl;
					}

                }

				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
		{
			cout << "error: " << mysql_error( Connection ) << endl;
			return 1;
		}
	}


}

int UpdateUsersList()
{
    for (list<CUser*>::iterator i = DBUsersList.begin(); i != DBUsersList.end(); i++)
        delete *i;

    DBUsersList.clear();

	cout << "[SERVER] Checking datatable" << endl;

        string QAdminsList = "SELECT name, password FROM users;";

	if( mysql_real_query( Connection, QAdminsList.c_str( ), QAdminsList.size( ) ) != 0 )
	{
		cout << "error: " << mysql_error( Connection ) << endl;
		return 1;
	}
	else
	{
		MYSQL_RES *Result = mysql_store_result( Connection );

		if( Result )
		{
			vector<string> Row = MySQLFetchRow( Result );

			while( !Row.empty( ) )
			{
                if ( !Row[0].empty())
                {
                    CUser* user = new CUser(NULL);
                    user->SetName( Row[0] );
                    user->SetHashPassword( Row[1] );
                    user->SetStatus( 1 );

                    DBUsersList.push_back(user);
                }

				Row = MySQLFetchRow( Result );
			}

			mysql_free_result( Result );
		}
		else
		{
			cout << "error: " << mysql_error( Connection ) << endl;
			return 1;
		}
	}

}

int main( int argc, char **argv )
{
    CONSOLE_Print( "[SRV] Starting server ");

    string CFGFile = defaultCFGFile;
    string mode;

	if( argc > 1 && argv[1] )
	{
	    mode = argv[1];

	    if ( mode != "--items_convert")
            CFGFile = argv[1];
	}

    loadconfig( CFGFile );

    updater = new CUpdater( client_updater_directory, client_updater_download_httppath, client_download_localpath );

    initRedisClient();

	if( !( Connection = mysql_init( NULL ) ) )
	{
		cout << "[MySQL] error: " << mysql_error( Connection ) << endl;
		return 1;
	}

	my_bool Reconnect = true;
	mysql_options( Connection, MYSQL_OPT_RECONNECT, &Reconnect );

	if( !( mysql_real_connect( Connection, dbServer.c_str( ), dbUser.c_str( ), dbPassword.c_str( ), dbDatabase.c_str( ), Port, NULL, 0 ) ) )
	{
		cout << "error: " << mysql_error( Connection ) << endl;
		return 1;
	}

	cout << "[SRV] connecting to users database server" << endl;

    UsersList.clear();

    if ( mode == "--items_convert")
    {
        ConvertItems();
    } else
    {

    UpdateUsersList();

	CServer* gGGB = new CServer( 6143, 6144, updater, client_lastversion );
	gGGB->BotList = nBotList;

    gGGB->UpdateBotStatus();

	while( 1 )
	{
		if( gGGB->Update( 40000 ) )
			break;
	}

	// shutdown GGB

	CONSOLE_Print( "[SRV] shutting down" );
	delete gGGB;
	gGGB = NULL;

    }

	delete updater;


    for (list<CUser*>::iterator i = DBUsersList.begin(); i != DBUsersList.end(); i++)
        delete *i;

    mysql_close(Connection);

    return 0;
}
