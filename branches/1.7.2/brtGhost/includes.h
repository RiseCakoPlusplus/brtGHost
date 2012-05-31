/*

   Copyright [2008] [Trevor Hogan]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

*/

#ifndef INCLUDES_H
#define INCLUDES_H

// standard integer sizes for 64 bit compatibility

#ifdef WIN32
 #include "ms_stdint.h"
#else
 #include <stdint.h>
#endif

// STL

#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>
#include <map>

#include <boost/filesystem.hpp>
#include <boost/date_time.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>

using namespace std;

typedef vector<unsigned char> BYTEARRAY;
typedef pair<unsigned char,string> PIDPlayer;
class CLanguage;
extern CLanguage *m_Language;			// language
										// todotodo - Use singelton

// time

uint32_t GetTime( );		// seconds
uint32_t GetTicks( );		// milliseconds

#ifdef WIN32
 #define MILLISLEEP( x ) Sleep( x )
#else
 #define MILLISLEEP( x ) usleep( ( x ) * 1000 )
#endif

// network

#undef FD_SETSIZE
#define FD_SETSIZE 512

// output

void CONSOLE_Print( const string& message );
void DEBUG_Print( string message );
void DEBUG_Print( BYTEARRAY b );

// channel

vector<string> Channel_Users ();
void Channel_Clear (string name);
void Channel_Add (string name);
void Channel_Join (string server, string name);
void Channel_Del (string name);

const uint32_t CMD_ban = 0;
const uint32_t CMD_delban = 1;
const uint32_t CMD_host = 2;
const uint32_t CMD_unhost = 3;
const uint32_t CMD_end = 4;
const uint32_t CMD_mute = 5;
const uint32_t CMD_kick = 6;
const uint32_t CMD_say = 7;
const uint32_t CMD_open = 8;
const uint32_t CMD_close = 9;
const uint32_t CMD_swap = 10;
const uint32_t CMD_sp = 11;
const uint32_t CMD_quit = 12;
const uint32_t CMD_mod = 13;
const uint32_t CMD_admin = 14;
const string CMD_string = "ban delban host unhost end mute kick say open close swap sp quit";
const string CMD_stringshow = "ban delban host unhost end mute kick say open close swap sp quit";

// command access

bool CMDCheck (uint32_t cmd, uint32_t acc);
uint32_t CMDAccessAll ();

// path fix

string FixPath(string Path, string End);

// patch 21

bool Patch21();

string tr(const string& lang_id);
string tr(const string& lang_id, const string& v1);
string tr(const string& lang_id, const string& v1, const string& s1,
									string v2 = "", string s2 = "",
                                    string v3 = "", string s3 = "",
                                    string v4 = "", string s4 = "",
                                    string v5 = "", string s5 = "",
                                    string v6 = "", string s6 = "",
                                    string v7 = "", string s7 = "",
                                    string v8 = "", string s8 = "",
                                    string v9 = "", string s9 = "",
                                    string v10 = "", string s10 = "",
                                    string v11 = "", string s11 = "",
                                    string v12 = "", string s12 = "",
                                    string v13 = "", string s13 = "",
                                    string v14 = "", string s14 = "",
                                    string v15 = "", string s15 = "",
                                    string v16 = "", string s16 = "",
                                    string v17 = "", string s17 = "",
                                    string v18 = "", string s18 = "",
                                    string v19 = "", string s19 = "");

#endif
