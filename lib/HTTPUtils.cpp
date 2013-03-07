/*
 *      Copyright (C) 2012 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "Log.h"
#include "HTTPUtils.h"
#include <curl/easy.h>
#include "FileUtils/FileUtils.h"
#include <cctype>
#include "JSONHandler.h"
#include <algorithm>
#include "CTiXmlHelper.h"


using namespace std;

bool CHTTPHandler::ComparePOFiles(std::string strPOFilePath1, std::string strPOFilePath2) const
{
  CPOHandler POHandler1, POHandler2;
  POHandler1.ParsePOStrToMem(g_File.ReadFileToStr(strPOFilePath1), strPOFilePath1);
  POHandler2.ParsePOStrToMem(g_File.ReadFileToStr(strPOFilePath2), strPOFilePath2);
  return ComparePOFilesInMem(&POHandler1, &POHandler2, false);
}

bool CHTTPHandler::ComparePOFilesInMem(CPOHandler * pPOHandler1, CPOHandler * pPOHandler2, bool bLangIsEN) const
{
  if (!pPOHandler1 || !pPOHandler2)
    return false;
  if (pPOHandler1->GetNumEntriesCount() != pPOHandler2->GetNumEntriesCount())
    return false;
  if (pPOHandler1->GetClassEntriesCount() != pPOHandler2->GetClassEntriesCount())
    return false;

  for (size_t POEntryIdx = 0; POEntryIdx != pPOHandler1->GetNumEntriesCount(); POEntryIdx++)
  {
    CPOEntry POEntry1 = *(pPOHandler1->GetNumPOEntryByIdx(POEntryIdx));
    const CPOEntry * pPOEntry2 = pPOHandler2->GetNumPOEntryByID(POEntry1.numID);
    if (!pPOEntry2)
      return false;
    CPOEntry POEntry2 = *pPOEntry2;

    if (bLangIsEN)
    {
      POEntry1.msgStr.clear();
      POEntry2.msgStr.clear();
    }

    if (!(POEntry1 == POEntry2))
      return false;
  }

  for (size_t POEntryIdx = 0; POEntryIdx != pPOHandler1->GetClassEntriesCount(); POEntryIdx++)
  {
    const CPOEntry * POEntry1 = pPOHandler1->GetClassicPOEntryByIdx(POEntryIdx);
    CPOEntry POEntryToFind = *POEntry1;
    if (!pPOHandler2->LookforClassicEntry(POEntryToFind))
      return false;
  }
  return true;
}
CHTTPHandler::CHTTPHandler(bool fake_uploads)
{
  m_bFakeUploads = fake_uploads;
  m_curlHandle = curl_easy_init();
};

CHTTPHandler::~CHTTPHandler()
{
  Cleanup();
};

std::string CHTTPHandler::GetURLToSTR(const CHTTPCachedItem &urlcached, bool bSkiperror)
{
  std::string strBuffer;
  std::string strCacheFile = urlcached.LocalName;
  strCacheFile = m_strCacheDir + "GET/" + strCacheFile;
  
  std::replace(strCacheFile.begin(), strCacheFile.end(),'/', DirSepChar);

  if (!g_File.FileExist(strCacheFile) || g_File.GetFileAge(strCacheFile) > m_HTTPExpireTime * 60)
  {
    long result = curlURLToCache(strCacheFile, urlcached.URL, bSkiperror, strBuffer);
    if (result < 200 || result >= 400)
      return "";
  }
  else
    strBuffer = g_File.ReadFileToStr(strCacheFile);

  return strBuffer;
}
std::string CHTTPHandler::GetURLToSTR(std::string strURL, bool bSkiperror /*=false*/)
{
  CHTTPCachedItem temp = CHTTPCachedItem(strURL);
  std::string strBuffer;
  std::string strCacheFile = temp.LocalName;
  strCacheFile = m_strCacheDir + "GET/" + strCacheFile;
  
  std::replace(strCacheFile.begin(), strCacheFile.end(),'/', DirSepChar);

  if (!g_File.FileExist(strCacheFile) || g_File.GetFileAge(strCacheFile) >  m_HTTPExpireTime * 60)
  {
    long result = curlURLToCache(strCacheFile, strURL, bSkiperror, strBuffer);
    if (result < 200 || result >= 400)
      return "";
  }
  else
    strBuffer = g_File.ReadFileToStr(strCacheFile);

  return strBuffer;
};

long CHTTPHandler::curlURLToCache(std::string strCacheFile, std::string strURL, bool bSkiperror, std::string &strBuffer)
{
  CURLcode curlResult;
  strBuffer.clear();

  strURL = URLEncode(strURL);

  CLoginData LoginData = GetCredentials(strURL);

    if(m_curlHandle) 
    {
      curl_easy_setopt(m_curlHandle, CURLOPT_URL, strURL.c_str());
      curl_easy_setopt(m_curlHandle, CURLOPT_WRITEFUNCTION, Write_CurlData_String);
      if (!LoginData.strLogin.empty())
      {
        curl_easy_setopt(m_curlHandle, CURLOPT_USERNAME, LoginData.strLogin.c_str());
        curl_easy_setopt(m_curlHandle, CURLOPT_PASSWORD, LoginData.strPassword.c_str());
      }
      curl_easy_setopt(m_curlHandle, CURLOPT_FAILONERROR, true);
      curl_easy_setopt(m_curlHandle, CURLOPT_WRITEDATA, &strBuffer);
      curl_easy_setopt(m_curlHandle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
      curl_easy_setopt(m_curlHandle, CURLOPT_SSL_VERIFYPEER, 0);

      curlResult = curl_easy_perform(m_curlHandle);
      long http_code = 0;
      curl_easy_getinfo (m_curlHandle, CURLINFO_RESPONSE_CODE, &http_code);

      if (curlResult == 0 && http_code >= 200 && http_code < 400)
        CLog::Log(logINFO, "HTTPHandler: curlURLToCache finished with success from URL %s to cachefile %s, read filesize: %i bytes",
                  strURL.c_str(), strCacheFile.c_str(), strBuffer.size());
      else
      {
        if (!bSkiperror)
        CLog::Log(logERROR, "HTTPHandler: curlURLToCache finished with error code: %i from URL %s to localdir %s",
                  http_code, strURL.c_str(), strCacheFile.c_str());
        CLog::Log(logINFO, "HTTPHandler: curlURLToCache finished with code: %i from URL %s", http_code, strURL.c_str());
        return http_code;
      }
      g_File.WriteFileFromStr(strCacheFile, strBuffer);
      return http_code;
    }
    else
      CLog::Log(logERROR, "HTTPHandler: curlURLToCache failed because Curl was not initalized");
    return 0;
};

void CHTTPHandler::ReInit()
{
  if (!m_curlHandle)
    m_curlHandle = curl_easy_init();
  else
    CLog::Log(logWARNING, "HTTPHandler: Trying to reinitalize an already existing Curl handle");
};

void CHTTPHandler::Cleanup()
{
  if (m_curlHandle)
  {
    curl_easy_cleanup(m_curlHandle);
    m_curlHandle = NULL;
  }
};

size_t Write_CurlData_File(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t written = fwrite(ptr, size, nmemb, stream);
  return written;
};

size_t Read_CurlData_File(char *bufptr, size_t size, size_t nitems, FILE *stream) 
{
  size_t read;
  read = fread(bufptr, size, nitems, stream);
  return read;
}


size_t Write_CurlData_String(char *data, size_t size, size_t nmemb, string *buffer)
{
  size_t written = 0;
  if(buffer != NULL)
  {
    buffer -> append(data, size * nmemb);
    written = size * nmemb;
  }
  return written;
}

typedef struct
{ 
  std::string * pPOString; 
  size_t pos; 
} Tputstrdata; 


size_t Read_CurlData_String(void * ptr, size_t size, size_t nmemb, void * stream)
{
  if (stream)
  {
    Tputstrdata * pPutStrData = (Tputstrdata*) stream; 

    size_t available = (pPutStrData->pPOString->size() - pPutStrData->pos);

    if (available > 0)
    {
      size_t written = std::min<size_t>(size * nmemb, available);
      memcpy(ptr, &pPutStrData->pPOString->at(pPutStrData->pos), written); 
      pPutStrData->pos += written;
      return written; 
    }
  }

  return 0; 
}

void CHTTPHandler::SetCacheDir(std::string strCacheDir, size_t cache_expire_time_minutes)
{
  m_HTTPExpireTime = cache_expire_time_minutes;
  if (!g_File.DirExists(strCacheDir))
    g_File.MakeDir(strCacheDir);
  m_strCacheDir = strCacheDir + DirSepChar;
  CLog::Log(logINFO, "HTTPHandler: Cache directory set to: %s", strCacheDir.c_str());
};

bool CHTTPHandler::HaveCredentials ()
{
  return m_mapLoginData.size() > 0;
}
bool CHTTPHandler::LoadCredentials (std::string CredentialsFilename)
{
  TiXmlDocument xmlPasswords;

  CLog::Log(logINFO, "HTTPHandler: Looking for %s.", CredentialsFilename.c_str());
  
  if (g_File.FileExist(CredentialsFilename))
  {
    if (!xmlPasswords.LoadFile(CredentialsFilename.c_str()))
    {
      CLog::Log(logINFO, "TinyXml: Error while loading password file.");
      return false;
    }
  }
  else
  {
    CLog::Log(logINFO, "HTTPHandler: No password file found.");
    return false;
  }

  CLog::Log(logINFO, "HTTPHandler: Succesfully found a password file.");

  TiXmlElement* pRootElement;

  if (!CTiXmlHelper::HasRoot(&xmlPasswords,"websites", pRootElement))
  {
    CLog::Log(logWARNING, "HTTPHandler: No root element called \"websites\" in the password file.");
    return false;
  }

  CLoginData LoginData;

  const TiXmlElement *pChildElement = pRootElement->FirstChildElement("website");
  while (pChildElement && pChildElement->FirstChild())
  {
    std::string strWebSitePrefix = pChildElement->Attribute("prefix");
    if (pChildElement->FirstChild())
    {
        LoginData.strLogin = CTiXmlHelper::GetInnerText(pChildElement->FirstChildElement("login"));
        LoginData.strPassword = CTiXmlHelper::GetInnerText(pChildElement->FirstChildElement("password"));

      m_mapLoginData [strWebSitePrefix] = LoginData;
      CLog::Log(logINFO, "HTTPHandler: Found login credentials for website prefix: %s", strWebSitePrefix.c_str());
    }
    pChildElement = pChildElement->NextSiblingElement("website");
  }

  return true;
};

CLoginData CHTTPHandler::GetCredentials (std::string strURL)
{
  CLoginData LoginData;
  for (itMapLoginData = m_mapLoginData.begin(); itMapLoginData != m_mapLoginData.end(); itMapLoginData++)
  {
    if (strURL.find(itMapLoginData->first) != std::string::npos)
    {
      LoginData = itMapLoginData->second;
      return LoginData;
    }
  }

  return LoginData;
};

std::string CHTTPHandler::URLEncode (std::string strURL)
{
  std::string strOut;
  for (std::string::iterator it = strURL.begin(); it != strURL.end(); it++)
  {
    if (*it == ' ')
      strOut += "%20";
    else
      strOut += *it;
  }
  return strOut;
}

bool CHTTPHandler::GetCachedPath(HTTPCacheStore store, const CHTTPCachedItem &urlcached, std::string &strPath)
{
  if (store == CACHE_UPLOAD)
  {
    strPath = m_strCacheDir + "PUT/" + urlcached.LocalName;
  }
  else
  {
    strPath = m_strCacheDir + "GET/" + urlcached.LocalName;
  }

  std::replace(strPath.begin(), strPath.end(),'/', DirSepChar);

  return g_File.FileExist(strPath);
}

bool CHTTPHandler::PutFileToURL(std::string const &strFilePath, const CHTTPCachedItem &urlcached, bool &buploaded,
                                size_t &stradded, size_t &strupd)
{
  std::string strCacheFile;
  GetCachedPath(CACHE_UPLOAD, urlcached, strCacheFile);

  CLog::Log(logINFO, "HTTPHandler::PutFileToURL: Uploading file to Transifex: %s", strFilePath.c_str());

  long result = curlPUTPOFileToURL(strFilePath, urlcached.URL, stradded, strupd);
  if (result < 200 || result >= 400)
  {
    CLog::Log(logERROR, "HTTPHandler::PutFileToURL: File upload was unsuccessful, http errorcode: %i", result);
    return false;
  }

  CLog::Log(logINFO, "HTTPHandler::PutFileToURL: File upload was successful so creating a copy at the .httpcache directory");
  g_File.CpFile(strFilePath, strCacheFile);

  buploaded = true;

  return true;
};

long CHTTPHandler::curlPUTPOFileToURL(std::string const &strFilePath, std::string const &cstrURL, size_t &stradded, size_t &strupd)
{

  CURLcode curlResult;

  std::string strURL = URLEncode(cstrURL);
  
  if (m_bFakeUploads)
  {
      CLog::Log(logINFO, "HTTPHandler::curlFileToURL faked with success from File %s to URL %s",
            strFilePath.c_str(), strURL.c_str());
    return 200;
  }

  std::string strPO = g_File.ReadFileToStr(strFilePath);

  std::string strPOJson = g_Json.CreateJSONStrFromPOStr(strPO);

  Tputstrdata PutStrData;
  PutStrData.pPOString = &strPOJson;
  PutStrData.pos = 0;

  std::string strServerResp;
  CLoginData LoginData = GetCredentials(strURL);

  if(m_curlHandle) 
  {
    struct curl_slist *headers=NULL;
    headers = curl_slist_append( headers, "Content-Type: application/json");
    headers = curl_slist_append( headers, "charsets: utf-8");

    curl_easy_setopt(m_curlHandle, CURLOPT_READFUNCTION, Read_CurlData_String);
    curl_easy_setopt(m_curlHandle, CURLOPT_WRITEFUNCTION, Write_CurlData_String);
    curl_easy_setopt(m_curlHandle, CURLOPT_URL, strURL.c_str());
    curl_easy_setopt(m_curlHandle, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(m_curlHandle, CURLOPT_PUT, 1L);
    curl_easy_setopt(m_curlHandle, CURLOPT_POST, 0); // disable post, we are doing a PUT not a POST this time
    if (!LoginData.strLogin.empty())
    {
      curl_easy_setopt(m_curlHandle, CURLOPT_USERNAME, LoginData.strLogin.c_str());
      curl_easy_setopt(m_curlHandle, CURLOPT_PASSWORD, LoginData.strPassword.c_str());
    }
    curl_easy_setopt(m_curlHandle, CURLOPT_FAILONERROR, true);
    curl_easy_setopt(m_curlHandle, CURLOPT_READDATA, &PutStrData);
    curl_easy_setopt(m_curlHandle, CURLOPT_WRITEDATA, &strServerResp);
    curl_easy_setopt(m_curlHandle, CURLOPT_INFILESIZE_LARGE, (curl_off_t)strPOJson.size());
    curl_easy_setopt(m_curlHandle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(m_curlHandle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(m_curlHandle, CURLOPT_SSL_VERIFYPEER, 0);

    curlResult = curl_easy_perform(m_curlHandle);

    g_Json.ParseUploadedStringsData(strServerResp, stradded, strupd);

    long http_code = 0;
    curl_easy_getinfo (m_curlHandle, CURLINFO_RESPONSE_CODE, &http_code);

    if (curlResult == 0 && http_code >= 200 && http_code < 400)
      CLog::Log(logINFO, "HTTPHandler::curlFileToURL finished with success from File %s to URL %s",
                strFilePath.c_str(), strURL.c_str());
    else
    {
      CLog::Log(logINFO, "HTTPHandler::curlFileToURL finished with error code: %i from file %s to URL %s",
                http_code, strFilePath.c_str(), strURL.c_str());
    }
    return http_code;
  }
  else
    CLog::Log(logERROR, "HTTPHandler::curlFileToURL failed because Curl was not initalized");
  return 700;
};

bool CHTTPHandler::CreateNewResource(std::string strResname, std::string strENPOFilePath, std::string strURL, size_t &stradded,
                                     std::string const &strURLENTransl)
{
  CHTTPCachedItem temp = CHTTPCachedItem(strURLENTransl);
  std::string strCacheFile = temp.LocalName;
  strCacheFile = m_strCacheDir + "PUT/" + strCacheFile;

  CURLcode curlResult;

  strURL = URLEncode(strURL);
  if (m_bFakeUploads)
  {
      CLog::Log(logINFO, "CHTTPHandler::CreateNewResource faked with success for resource %s from EN PO file %s to URL %s",
                strResname.c_str(), strENPOFilePath.c_str(), strURL.c_str());
    return true;
  }
  std::string strPO = g_File.ReadFileToStr(strENPOFilePath);

  std::string strPOJson = g_Json.CreateNewresJSONStrFromPOStr(strResname, strPO);

  Tputstrdata PutStrData;
  PutStrData.pPOString = &strPOJson;
  PutStrData.pos = 0;

  std::string strServerResp;
  CLoginData LoginData = GetCredentials(strURL);

  if(m_curlHandle) 
  {
    struct curl_slist *headers=NULL;
    headers = curl_slist_append( headers, "Content-Type: application/json");
    headers = curl_slist_append( headers, "charsets: utf-8");

    curl_easy_setopt(m_curlHandle, CURLOPT_READFUNCTION, Read_CurlData_String);
    curl_easy_setopt(m_curlHandle, CURLOPT_WRITEFUNCTION, Write_CurlData_String);
    curl_easy_setopt(m_curlHandle, CURLOPT_URL, strURL.c_str());
    curl_easy_setopt(m_curlHandle, CURLOPT_POST, 1L);
    if (!LoginData.strLogin.empty())
    {
      curl_easy_setopt(m_curlHandle, CURLOPT_USERNAME, LoginData.strLogin.c_str());
      curl_easy_setopt(m_curlHandle, CURLOPT_PASSWORD, LoginData.strPassword.c_str());
    }
    curl_easy_setopt(m_curlHandle, CURLOPT_FAILONERROR, true);
    curl_easy_setopt(m_curlHandle, CURLOPT_READDATA, &PutStrData);
    curl_easy_setopt(m_curlHandle, CURLOPT_WRITEDATA, &strServerResp);
    curl_easy_setopt(m_curlHandle, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)strPOJson.size());
    curl_easy_setopt(m_curlHandle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(m_curlHandle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(m_curlHandle, CURLOPT_SSL_VERIFYPEER, 0);

    curlResult = curl_easy_perform(m_curlHandle);

    long http_code = 0;
    curl_easy_getinfo (m_curlHandle, CURLINFO_RESPONSE_CODE, &http_code);

    if (curlResult == 0 && http_code >= 200 && http_code < 400)
    {
      CLog::Log(logINFO, "CHTTPHandler::CreateNewResource finished with success for resource %s from EN PO file %s to URL %s",
                strResname.c_str(), strENPOFilePath.c_str(), strURL.c_str());
      g_File.CpFile(strENPOFilePath, strCacheFile);
      g_Json.ParseUploadedStrForNewRes(strServerResp, stradded);
    }
    else
    {
      CLog::Log(logINFO, "CHTTPHandler::CreateNewResource finished with error code %i, for resource %s from EN PO file %s to URL %s ",
                http_code, strResname.c_str(), strENPOFilePath.c_str(), strURL.c_str());
    }
    return true;
  }
  else
    CLog::Log(logERROR, "CHTTPHandler::CreateNewResource failed because Curl was not initalized");
  return false;
};

void CHTTPHandler::DeleteCachedFile (std::string const &strURL, std::string strPrefix)
{
  CHTTPCachedItem temp = CHTTPCachedItem(strURL);
  std::string strCacheFile = temp.LocalName;

  strCacheFile = m_strCacheDir + strPrefix + "/" + strCacheFile;
  if (g_File.FileExist(strCacheFile))
    g_File.DelFile(strCacheFile);
}

CHTTPCachedItem::CHTTPCachedItem(std::string url)
{
  URL = url;
  LocalName = CacheFileNameFromURL(url);
};

CHTTPCachedItem::CHTTPCachedItem()
{
};

std::string CHTTPCachedItem::CacheFileNameFromURL(std::string strURL)
{
  std::string strResult;
  std::string strCharsToKeep  = "/.-=_() ";
  std::string strReplaceChars = "/.-=_() ";

  std::string hexChars = "01234567890abcdef"; 

  for (std::string::iterator it = strURL.begin(); it != strURL.end(); it++)
  {
    if (isalnum(*it))
      strResult += *it;
    else
    {
      size_t pos = strCharsToKeep.find(*it);
      if (pos != std::string::npos)
        strResult += strReplaceChars[pos];
      else
      {
        strResult += "%";
        strResult += hexChars[(*it >> 4) & 0x0F];
        strResult += hexChars[*it & 0x0F];
      }
    }
  }

  if (strResult.at(strResult.size()-1) == '/')
    strResult[strResult.size()-1] = '-';

  return strResult;
};