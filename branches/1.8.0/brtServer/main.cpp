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
#include "socket.h"
#include "server.h"
#include "updater.h"
#include "sha1.h"
#include "md5.h"
#include "user.h"
#include "configdata.h"
#include "bnet.h"

#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "zlib/zlib.h"

CUpdater* nUpdater;
CConfig* nConfig;

bool nExiting = false;

const string defaultCFGFile = "server.cfg";

MYSQL *Connection = NULL;

list<CUser*> DBUsersList;
list<CUser*> UsersList;

class CBNetConfig;

class CBNETThread
{
private:
	CBNET* m_BNet;
	bool m_Exiting;

public:
	CBNETThread( CBNET* nBNet ) : m_BNet( nBNet ) { m_Exiting = false; };

	bool Update( int usecBlock );
	void UpdateThread();

	void StartThread();
	void SetExiting() { m_Exiting = true; }
};

void CBNETThread :: StartThread()
{
	boost::thread Thread( boost :: bind( &CBNETThread::UpdateThread, this ) );
}

bool CBNETThread :: Update(int usecBlock)
{
	int nfds = 0;
	fd_set fd;
	fd_set send_fd;
	FD_ZERO( &fd );
	FD_ZERO( &send_fd );

	m_BNet->SetFD( &fd, &send_fd, &nfds );

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = usecBlock;

	struct timeval send_tv;
	send_tv.tv_sec = 0;
	send_tv.tv_usec = 0;

#ifdef WIN32
	select( 1, &fd, NULL, NULL, &tv );
	select( 1, NULL, &send_fd, NULL, &send_tv );
#else
	select( nfds + 1, &fd, NULL, NULL, &tv );
	select( nfds + 1, NULL, &send_fd, NULL, &send_tv );
#endif

	if( m_BNet->Update( &fd, &send_fd ) )
		return true;

	return false;
}

void CBNETThread :: UpdateThread()
{
	while (1)
		if (Update(40000) || m_Exiting)
			break;

	delete m_BNet;
	m_BNet = NULL;
}

vector<CBNETThread*> nBNetList;

char rand_small_letter() 
{
	int nHigh = 122;
	int nLow = 97;
	int small_letter = ((rand()%(nHigh - nLow + 1)) + nLow);      //values from 97 to 122
	return (char)(small_letter);
}
char rand_big_letter() 
{
	int nHigh = 90;
	int nLow = 65;
	int big_letter = ((rand()%(nHigh - nLow + 1)) + nLow);      //values from 65 to 90
	return (char)(big_letter);
}

int rand_number(int nLow, int nHigh) 
{
	return ((rand()%(nHigh - nLow + 1)) + nLow);
}

void CONSOLE_Print( string message )
{
    cout << message << endl;
}

string getRandomPassword()
{
	string pass = string();

	for (int i = 0; i < 12; ++i)
		switch (rand_number(1,3))
		{
			case 1: pass += string(1, rand_small_letter() ); break;
			case 2: pass += string(1, rand_big_letter() ); break;
			case 3: pass += UTIL_ToString(rand_number(0,9)); break;
		}

	return pass;
}

string getHashPassword( const string& pass )
{
	string return_password = pass;

	for ( int i = 0; i < 100; ++i )
	{
		return_password = md5(return_password);
	}

	return return_password; 
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

int UpdateUsersList()
{
    for (list<CUser*>::iterator i = DBUsersList.begin(); i != DBUsersList.end(); ++i)
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

	return 0;
}
/*
bool UpdateBNetFunc(int usecBlock)
{
	int nfds = 0;
	fd_set fd;
	fd_set send_fd;
	FD_ZERO( &fd );
	FD_ZERO( &send_fd );

	nBnet->SetFD( &fd, &send_fd, &nfds );

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = usecBlock;

	struct timeval send_tv;
	send_tv.tv_sec = 0;
	send_tv.tv_usec = 0;

#ifdef WIN32
	select( 1, &fd, NULL, NULL, &tv );
	select( 1, NULL, &send_fd, NULL, &send_tv );
#else
	select( nfds + 1, &fd, NULL, NULL, &tv );
	select( nfds + 1, NULL, &send_fd, NULL, &send_tv );
#endif

	if( nBnet->Update( &fd, &send_fd ) )
		return true;

	return false;
}

void UpdateBNetThread()
{
	while (1)
		if (UpdateBNetFunc(40000) || nExiting)
			break;

	delete nBnet;
	nBnet = NULL;
}
*/
int main( int argc, char **argv )
{
	srand( time(NULL) );

    CONSOLE_Print( "[SERVER] Starting server ");

	#ifndef WIN32
	// disable SIGPIPE since some systems like OS X don't define MSG_NOSIGNAL

	signal( SIGPIPE, SIG_IGN );
	#endif

	#ifdef WIN32
	// initialize winsock

	CONSOLE_Print( "[SERVER] Starting winsock" );
	WSADATA wsadata;

	if( WSAStartup( MAKEWORD( 2, 2 ), &wsadata ) != 0 )
	{
		CONSOLE_Print( "[SERVER] error starting winsock" );
		return 1;
	}
	#endif

    string CFGFile = defaultCFGFile;

	if( argc > 1 && argv[1] )
            CFGFile = argv[1];

	nConfig = new CConfig();
	nConfig->Parse(CFGFile);

	nUpdater = new CUpdater( nConfig );

	if( !( Connection = mysql_init( NULL ) ) )
	{
		cout << "[MySQL] error: " << mysql_error( Connection ) << endl;
		return 1;
	}

	my_bool Reconnect = true;
	mysql_options( Connection, MYSQL_OPT_RECONNECT, &Reconnect );

	if( !( mysql_real_connect( Connection, nConfig->dbServer.c_str( ), nConfig->dbUser.c_str( ), nConfig->dbPassword.c_str( ), nConfig->dbDatabase.c_str( ), nConfig->Port, NULL, 0 ) ) )
	{
		cout << "[DATABASE] Error: " << mysql_error( Connection ) << " " << nConfig->dbUser << " " << nConfig->dbPassword << endl;
//		return 1;
	}

	cout << "[SERVER] connecting to users database server" << endl;

    UsersList.clear();

    UpdateUsersList();

	for (vector<CBNetConfig>::iterator i = nConfig->m_BNetList.begin(); i != nConfig->m_BNetList.end(); ++i)
	{
		if ( !(*i).enabled ) 
			continue;

		CBNET* nBNet = new CBNET( (*i).tft,
								  nConfig->war3path,
								  (*i).bnet_server, 
								  (*i).bnls_server, 
							  	  (*i).bnls_port, 
							  	  (*i).bnls_wardercookie, 
								  (*i).cdkeyroc, 
								  (*i).cdkeytft, 
								  "USA", "United States", 
								  (*i).username, 
								  (*i).password, 
								  (*i).channel, 
								  (*i).war3version, 
								  UTIL_ExtractNumbers((*i).exeversion, 4), 
								  UTIL_ExtractNumbers((*i).exeversionhash, 4), 
								  (*i).passwordhashtype, 
								  200, nConfig); 

		CBNETThread* newBNETThread = new CBNETThread( nBNet );
		newBNETThread->StartThread();
		nBNetList.push_back( newBNETThread ); 
	}

	CServer* nServer = new CServer( 6143, 6144, nUpdater, nConfig->client_lastversion );
	
	for ( vector<CBotData>::iterator i = nConfig->nBotList.begin(); i != nConfig->nBotList.end(); ++i )
	{
		if ( (*i).password.empty() )
		{
			(*i).password = getRandomPassword();
			
			ofstream passwords;
			passwords.open( "passwords.cfg", ios :: app );

			if( !passwords.fail( ) )
			{
				passwords << "Password for " << (*i).name << " = " << (*i).password << endl;
				passwords.close( );
			}

			(*i).password = getHashPassword( (*i).password );

			nConfig->data.put<string>("server.bots." + (*i).name + ".password", (*i).password );
		}
	}
	nConfig->Save(CFGFile);

	nServer->BotList = nConfig->nBotList;

    nServer->UpdateBotStatus();

	while( 1 )
	{
		if( nServer->Update( 40000 ) )
			break;

		/*
		if (nBnet)
		{
			while ( !nBnet->m_chatCommands.empty() )
			{
				nServer->ProcessUserCommands( nBnet->m_chatCommands.front() );

				nBnet->m_chatCommands.pop();
			}
		}
		*/
	}

	CONSOLE_Print( "[SERVER] shutting down" );

	nExiting = true;

	delete nServer;
	delete nUpdater;
	delete nConfig;

    for (list<CUser*>::iterator i = DBUsersList.begin(); i != DBUsersList.end(); i++)
        delete *i;

	for (vector<CBNETThread*>::iterator i = nBNetList.begin(); i != nBNetList.end(); ++i)
		(*i)->SetExiting();

    mysql_close(Connection);

    return 0;
}
