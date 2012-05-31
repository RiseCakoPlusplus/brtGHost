#ifndef CONFIGDATA_H
#define CONFIGDATA_H

class CConfigData
{
private:
	boost::property_tree::ptree data;

public:
	uint32_t port_host, port_reconnect, port_command, port_admingame;

	string logfile;
	int logmethod;

public:
	CConfigData(void);
	~CConfigData(void);

	bool Parse( const string& nFileName );
	bool Save( const string& nFileName );
};

#endif