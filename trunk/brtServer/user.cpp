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
#include "socket.h"
#include "user.h"

CUser :: CUser(CTCPSocket* socket)
{
    name.clear();
    lower_name.clear();
    hash_password.clear();
    key.clear();
    status = 0;

    client_version_checked = false;

    last_ping_send = GetTime();
    last_pong_recv = GetTime();

    m_last_chat_to_game_send = GetTime();

    m_socket = socket;
}

CUser::~CUser()
{
    delete m_socket;
}
