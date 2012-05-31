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

#ifndef UTIL_H
#define UTIL_H

#include "includes.h"

/* ParseURL: Used to break apart a URL such as
             http://www.wdj.com:80/index.htm into protocol, port, host
             and request. */
void ParseURL(char* url,char* protocol,int lprotocol, char* host,int lhost,char* request,int lrequest,int *port);
/* ValidHostChar: Return TRUE if the specified character is valid
                  for a host name, i.e. A-Z or 0-9 or -.: */
bool ValidHostChar(char ch);

// byte arrays

BYTEARRAY UTIL_Uncompress( const BYTEARRAY& data ); // zlib uncompress
BYTEARRAY UTIL_Compress( const BYTEARRAY& data );
BYTEARRAY UTIL_Compress( const string& data );

string sha1(const string& input);

BYTEARRAY UTIL_CreateByteArray( char *a, int size );
BYTEARRAY UTIL_CreateByteArray( unsigned char *a, int size );
BYTEARRAY UTIL_CreateByteArray( unsigned char c );
BYTEARRAY UTIL_CreateByteArray( uint16_t i, bool reverse );
BYTEARRAY UTIL_CreateByteArray( uint32_t i, bool reverse );

uint16_t UTIL_ByteArrayToUInt16( const BYTEARRAY &b, bool reverse, unsigned int start = 0 );
uint32_t UTIL_ByteArrayToUInt32( const BYTEARRAY &b, bool reverse, unsigned int start = 0 );

string UTIL_ByteArrayToDecString( BYTEARRAY b );
string UTIL_ByteArrayToHexString( const BYTEARRAY &b );
string UTIL_ByteArrayToHexStringWithoutSpaces( const BYTEARRAY &b );

void UTIL_AppendByteArray( BYTEARRAY &b, const BYTEARRAY &append );
void UTIL_AppendByteArray( BYTEARRAY &b, unsigned char *a, int size );
void UTIL_AppendByteArray( BYTEARRAY &b, const string &append, bool terminator = true );
void UTIL_AppendByteArray( BYTEARRAY &b, uint16_t i, bool reverse );
void UTIL_AppendByteArray( BYTEARRAY &b, uint32_t i, bool reverse );

string    UTIL_ExtractCString( const BYTEARRAY &b, unsigned int start );
BYTEARRAY UTIL_ExtractCStringBytes( const BYTEARRAY& b, unsigned int start );

unsigned char UTIL_ExtractHex( BYTEARRAY &b, unsigned int start, bool reverse );
void UTIL_ExtractStrings( string s, vector<string> &v );
void UTIL_AddStrings( vector<string> &dest, vector<string> sourc );
BYTEARRAY UTIL_ExtractNumbers( string s, unsigned int count );
BYTEARRAY UTIL_ExtractHexNumbers( string s );

template <class Type> string UTIL_ToString( Type i )
{
	string result;
	stringstream SS;
	SS << i;
	SS >> result;
	return result;
}

template <class Type> string UTIL_ToString( Type i, int digits )
{
	string result;
	stringstream SS;
	SS << std :: fixed << std :: setprecision( digits ) << i;
	SS >> result;
	return result;
}
void Replace( string &Text, string Key, string Value );
string UTIL_ToString( double d, int digits );
string UTIL_ToHexString( uint32_t i );
uint16_t UTIL_ToUInt16( string &s );
uint32_t UTIL_ToUInt32( string &s );
int16_t UTIL_ToInt16( string &s );
int32_t UTIL_ToInt32( string &s );
double UTIL_ToDouble( string &s );
static inline int UTIL_ToInt(double d);
string UTIL_MSToString( uint32_t ms );
string UTIL_ToBinaryString( uint32_t data);

string sha1(const string& input);

// files

bool UTIL_FileExists( string file );
string UTIL_FileRead( string file, uint32_t start, uint32_t length );
string UTIL_FileRead( string file );
bool UTIL_FileWrite( string file, unsigned char *data, uint32_t length );
string UTIL_FileSafeName( string fileName );
string UTIL_AddPathSeperator( string path );

// stat strings

BYTEARRAY UTIL_EncodeStatString( BYTEARRAY &data );
BYTEARRAY UTIL_DecodeStatString( BYTEARRAY &data );

// other

bool UTIL_IsLanIP( BYTEARRAY ip );
bool UTIL_IsLocalIP( BYTEARRAY ip, vector<BYTEARRAY> &localIPs );
void UTIL_Replace( string &Text, string Key, string Value );
vector<string> UTIL_Tokenize( string s, char delim );

// math

uint32_t UTIL_Factorial( uint32_t x );

#define nCr(n, r) (UTIL_Factorial(n) / UTIL_Factorial((n)-(r)) / UTIL_Factorial(r))
#define nPr(n, r) (UTIL_Factorial(n) / UTIL_Factorial((n)-(r)))

#endif
