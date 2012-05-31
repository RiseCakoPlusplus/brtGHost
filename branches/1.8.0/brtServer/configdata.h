#ifndef _CONFIG_DATA_
#define _CONFIG_DATA_

#include "server.h"

class CBNetConfig
{
	public:
		bool enabled;
		bool tft;
		string cdkeyroc;
		string cdkeytft;
		string bnet_server;
		string username;
		string password;
		string channel;
		
		uint16_t war3version;
		uint32_t port;
		string exeversion;
		string exeversionhash;
		string passwordhashtype;

		string bnls_server;
		uint32_t bnls_wardercookie;
		uint32_t bnls_port;
};

class CConfig
{
public:
	// BATTLE.NET config

	bool m_TFT;

	char command_trigger;

	string war3path;

	vector<CBNetConfig> m_BNetList;

	// SERVER config
	string client_lastversion;
	string client_updater_directory;
	string client_updater_download_httppath;
	string client_download_localpath;

	// MySQL server config
	string dbServer;
	string dbDatabase;
	string dbUser;
	string dbPassword;
	uint32_t Port;

	string bot_ips;
	string bot_ports;
	string bot_admin_list;

	//
	vector<CBotData> nBotList;

	bool update_dota_elo;
	bool update_after_end_of_game;
	uint32_t update_every_seconds;

	 boost::property_tree::ptree data;

public:
	void Parse( const string& nFileName );
	void Save( const string& nFileName );
};

#endif