/*

   Copyright [2010] [Igor Bygaev] engdev@mail.ru

   This file is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
 
   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
   Lesser General Public License for more details.
  
   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
 
*/

#include "ghost.h"
#include "util.h"
#include "config.h"
#include "language.h"

#include <cstdarg>
#include <stdio.h>

//
// CLanguage
//

CLanguage :: CLanguage( const string& nCFGFile )
{
	m_CFG = new CConfig( );
	m_CFG->Read( nCFGFile );

}

CLanguage :: ~CLanguage( )
{
	delete m_CFG;
}


string CLanguage :: GetLang(const string& lang_id, const string& v1, const string& s1,
                                             string v2, string s2,
                                             string v3, string s3,
                                             string v4, string s4,
                                             string v5, string s5,
                                             string v6, string s6,
                                             string v7, string s7,
                                             string v8, string s8,
                                             string v9, string s9,
                                             string v10, string s10,
                                             string v11, string s11,
                                             string v12, string s12,
                                             string v13, string s13,
                                             string v14, string s14,
                                             string v15, string s15,
                                             string v16, string s16,
                                             string v17, string s17,
                                             string v18, string s18,
                                             string v19, string s19)
{

    int pos;
    string out = GetLang(lang_id);
    
    if (!out.compare("ignore")) return "#ignore#";
    
    if (!v1.empty())
    {
		if ((pos = out.find(v1)) != string :: npos)
            out.replace(pos, v1.size(), s1);
    } else return out;

    if (!v2.empty())
    {
        if ((pos = out.find(v2)) != string :: npos)
            out.replace(pos, v2.size(), s2);
    } else return out;

    if (!v3.empty())
    {
        if ((pos = out.find(v3)) != string :: npos)
            out.replace(pos, v3.size(), s3);
    } else return out;

    if (!v4.empty())
    {
        if ((pos = out.find(v4)) != string :: npos)
            out.replace(pos, v4.size(), s4);
    } else return out;


    if (!v5.empty())
    {
        if ((pos = out.find(v5)) != string :: npos)
            out.replace(pos, v5.size(), s5);
    } else return out;

    if (!v6.empty())
    {
        if ((pos = out.find(v6)) != string :: npos)
            out.replace(pos, v6.size(), s6);
    } else return out;

    if (!v7.empty())
    {
        if ((pos = out.find(v7)) != string :: npos)
            out.replace(pos, v7.size(), s7);
    } else return out;

    if (!v8.empty())
    {
        if ((pos = out.find(v8)) != string :: npos)
            out.replace(pos, v8.size(), s8);
    } else return out;

    if (!v9.empty())
    {
        if ((pos = out.find(v9)) != string :: npos)
            out.replace(pos, v9.size(), s9);
    } else return out;

    if (!v10.empty())
    {
        if ((pos = out.find(v10)) != string :: npos)
            out.replace(pos, v10.size(), s10);
    } else return out;

    if (!v11.empty())
    {
        if ((pos = out.find(v11)) != string :: npos)
            out.replace(pos, v11.size(), s11);
    } else return out;

    if (!v12.empty())
    {
        if ((pos = out.find(v12)) != string :: npos)
            out.replace(pos, v12.size(), s12);
    } else return out;

    if (!v13.empty())
    {
        if ((pos = out.find(v13)) != string :: npos)
            out.replace(pos, v13.size(), s13);
    } else return out;


    if (!v14.empty())
    {
        if ((pos = out.find(v14)) != string :: npos)
            out.replace(pos, v14.size(), s14);
    } else return out;


    if (!v15.empty())
    {
        if ((pos = out.find(v15)) != string :: npos)
            out.replace(pos, v15.size(), s15);
    } else return out;


    if (!v16.empty())
    {
        if ((pos = out.find(v16)) != string :: npos)
            out.replace(pos, v16.size(), s16);
    } else return out;

    if (!v17.empty())
    {
        if ((pos = out.find(v17)) != string :: npos)
            out.replace(pos, v17.size(), s17);
    } else return out;

    if (!v18.empty())
    {
        if ((pos = out.find(v18)) != string :: npos)
            out.replace(pos, v18.size(), s18);
    } else return out;

    if (!v19.empty())
    {
        if ((pos = out.find(v19)) != string :: npos)
            out.replace(pos, v19.size(), s19);
    } else return out;


    return out;
}

string CLanguage :: GetLang(const string& lang_id, const string& v1)
{
    string out = GetLang(lang_id);

    if (!out.compare("ignore")) return "#ignore#";
    
    if (v1.empty()) return out;

    int pos_start, pos_end;

    if ( (pos_start = out.find("$") ) != string :: npos)
    if ( (pos_end = out.find("$", pos_start +1 ) ) != string :: npos)
        out.replace(pos_start, pos_end - pos_start + 1, v1);

    return out;
}
