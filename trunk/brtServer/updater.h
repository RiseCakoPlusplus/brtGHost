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

#ifndef CUPDATER_H
#define CUPDATER_H

class CUpdaterFile
{
    private:
        string m_Name, m_Hash;
        uint32_t m_Size, m_SizeComp;

    public:
        CUpdaterFile( string nName, string nHash, uint32_t nSize, uint32_t nSizeComp) : m_Name(nName), m_Hash(nHash), m_Size(nSize), m_SizeComp(nSizeComp) {}

        string getName() { return m_Name; }
        string getHash() { return m_Hash; }
        uint32_t getSize() { return m_Size; }
        uint32_t getSizeComp() { return m_SizeComp; }
};

class CUpdater
{
    private:
        vector<CUpdaterFile> m_UpdaterData;

        string m_localpath;
        string m_downloadpath;
        string m_download_localpath;

        void ProcessDir( string start_dir, path dir );

    public:
        CUpdater( string nLocalPath, string n_DownloadPath, string n_DownloadLocalPath );

        bool CheckFiles( vector<CUpdaterFile>& testFiles, vector<CUpdaterFile>& notCompareFiles );
        string GetDownloadUrl() { return m_downloadpath; }
        string GetLocalDownloadUrl() { return m_download_localpath; }

        virtual ~CUpdater();
};

#endif // CUPDATER_H
