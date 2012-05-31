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
#include "util.h"
#include "updater.h"

void CUpdater::ProcessDir( string start_dir, path dir )
{
    typedef vector<path> vec;
    vec v;

    copy(directory_iterator(dir), directory_iterator(), back_inserter(v));

    sort(v.begin(), v.end());

    for (vec::const_iterator it(v.begin()), it_end(v.end()); it != it_end; ++it)
    {
       string sFileName = (*it).string().substr( start_dir.size(), (*it).string().size() );

       if ( is_regular_file( *it) )
       {
           string sFileData = UTIL_FileRead( (*it).string() );
           string hash = md5( sFileData );
           string sFileName = (*it).string().substr( start_dir.size(), (*it).string().size() );
           BYTEARRAY sFileDataPacked = UTIL_Compress( sFileData );

           cout << "[UPDATER] Pack " << sFileName << " [" << UTIL_ToString(static_cast<int>(((sFileDataPacked.size() * 100 ) / sFileData.size()))) << "%]" << endl;

           if ( !exists( m_download_localpath + sFileName ) || file_size( (*it) ) != file_size(  m_download_localpath + sFileName ) )
                    UTIL_FileWrite( m_download_localpath + sFileName, (unsigned char*)&sFileDataPacked[0], sFileDataPacked.size() );

           m_UpdaterData.push_back( CUpdaterFile( sFileName , hash, file_size(*it), file_size( m_download_localpath + sFileName ) ) );
       }

       if ( is_directory( *it ) )
            ProcessDir( start_dir, *it );
    }
}

CUpdater::CUpdater( string nLocalPath, string n_DownloadPath, string n_DownloadLocalPath )
{
  m_localpath = nLocalPath;
  m_downloadpath = n_DownloadPath;
  m_download_localpath = n_DownloadLocalPath;

  path p( m_localpath );

  try
  {
    if ( exists(p) )
    {
      if ( is_directory(p) )
      {
        ProcessDir( m_localpath, p );

        cout << "[UPDATER] Directory (" << p << ") sucessfuly processed." << endl;

      }
      else
        cout << p << "[UPDATER] Directory (" << p << ") exists, but is neither a regular file nor a directory." << endl;
    }
    else
       cout << p << "[UPDATER] Directory (" << p << ") does not exist." << endl;
  }

  catch (const filesystem_error& ex)
  {
    cout << ex.what() << endl;
  }

}

bool CUpdater::CheckFiles( vector<CUpdaterFile>& testFiles, vector<CUpdaterFile>& notCompareFiles )
{
    vector<CUpdaterFile> nReturnBuffer;

    bool r = true;

    for ( vector<CUpdaterFile>::iterator i = m_UpdaterData.begin(); i != m_UpdaterData.end(); ++i )
    {
        bool find = false;

 //       cout << "[UPDATER] Check " << (*i).getName();

        for ( vector<CUpdaterFile>::iterator it = testFiles.begin(); it != testFiles.end(); ++it )
        {
            if ( (*i).getName() == (*it).getName() && (*i).getHash() == (*it).getHash() && (*i).getSize() == (*it).getSize() )
            {
                find = true;
 //               cout << "...OK. " << endl;
                break;
            }

        }

        if ( !find )
        {
 //           cout << "...failed. " << endl;
            r = false;

            notCompareFiles.push_back( *i );
        }
    }

    return r;
}

CUpdater::~CUpdater()
{

}
