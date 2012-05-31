#ifndef _SERVERMYSQL_H_
#define _SERVERMYSQL_H_

class CServerMySQL
{
private:
	queue<void *> m_IdleConnections;

public:
	CServerMySQL(void);
	~CServerMySQL(void);
};
#endif