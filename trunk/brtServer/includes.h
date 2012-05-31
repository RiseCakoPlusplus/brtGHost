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

#ifndef INCLUDES_H
#define INCLUDES_H

#ifdef WIN32
 #include <winsock.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <map>
#include <string>
#include <string.h>
#include <vector>
#include <iomanip>
#include <iostream>
#include <map>
#include <queue>
#include <deque>
#include <list>
#include <sstream>
#include <fstream>

#ifdef WIN32
 //#include "ms_stdint.h"
 #include <stdint.h>
#else
 #include <stdint.h>
#endif

#include <mysql/mysql.h>
#include <algorithm>

#include <boost/filesystem.hpp>

#include "md5.h"

using namespace std;
using namespace boost::filesystem;

typedef vector<unsigned char> BYTEARRAY;

void CONSOLE_Print( string message );

uint32_t GetTicks( );

uint32_t GetTime( );

#ifdef WIN32
 #define MILLISLEEP( x ) Sleep( x )
#else
 #define MILLISLEEP( x ) usleep( ( x ) * 1000 )
#endif


#endif
