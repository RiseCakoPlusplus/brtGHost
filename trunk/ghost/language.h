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

#ifndef LANGUAGE_H
#define LANGUAGE_H

//
// CLanguage
//
// brtGhost team, freed@gmail.com

class CLanguage
{
private:
	CConfig *m_CFG;

public:
	CLanguage( const string& nCFGFile );
	~CLanguage( );

    string GetLang(const string& lang_id) { return m_CFG -> GetString(lang_id,"Error find "+lang_id+" in language config."); }; // Get Lang string from config

    string GetLang(const string& lang_id, const string& v1);

    string GetLang(const string& lang_id, const string& v1, const string& s1,
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
};

#endif
