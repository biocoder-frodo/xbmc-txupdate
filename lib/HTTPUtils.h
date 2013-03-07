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

#ifndef HTTPUTILS_H
#define HTTPUTILS_H

#pragma once

#include <string>
#include <map>
#include <stdio.h>
#include <curl/curl.h>
#include "POHandler.h"

struct CLoginData
{
  std::string strLogin;
  std::string strPassword;
};

class CHTTPCachedItem
{
public:
  CHTTPCachedItem(std::string URL);
  CHTTPCachedItem();

  std::string URL;
  std::string LocalName; // local path below the CacheDir
private:
  std::string CacheFileNameFromURL(std::string strURL); //automatic localname by transforming source URL
};

enum HTTPCacheStore
{
  CACHE_DOWNLOAD,
  CACHE_UPLOAD
};

class CHTTPHandler
{
public:
  CHTTPHandler(bool fake_uploads);
  ~CHTTPHandler();
  void ReInit();
  std::string GetURLToSTR(std::string strURL, bool bSkiperror = false);
  std::string GetURLToSTR(const CHTTPCachedItem &urlcached, bool bSkiperror = false);
  void Cleanup();
  void SetCacheDir(std::string strCacheDir, size_t cache_expire_time_minutes);
  bool LoadCredentials (std::string CredentialsFilename);
  bool HaveCredentials ();
  bool GetCachedPath(HTTPCacheStore store, const CHTTPCachedItem &urlcached, std::string &strPath);
  bool PutFileToURL(std::string const &strFilePath, const CHTTPCachedItem &urlcached, bool &buploaded,
                                size_t &stradded, size_t &strupd);
  bool CreateNewResource(std::string strResname, std::string strENPOFilePath, std::string strURL, size_t &stradded,
                         std::string const &strURLENTransl);
  void DeleteCachedFile(std::string const &strURL, std::string strPrefix);
  bool ComparePOFilesInMem(CPOHandler * pPOHandler1, CPOHandler * pPOHandler2, bool bLangIsEN) const;
  bool ComparePOFiles(std::string strPOFilePath1, std::string strPOFilePath2) const;
  //bool ComparePOFilesInMem(CPOHandler * pPOHandler1, CPOHandler * pPOHandler2, bool bLangIsEN) const;

private:
  CURL *m_curlHandle;
  std::string m_strCacheDir;
  long curlURLToCache(std::string strCacheFile, std::string strURL, bool bSkiperror, std::string &strBuffer);
  long curlPUTPOFileToURL(std::string const &strFilePath, std::string const &strURL, size_t &stradded, size_t &strupd);

  CLoginData GetCredentials (std::string strURL);


  std::string URLEncode (std::string strURL);
  std::map<std::string, CLoginData> m_mapLoginData;
  std::map<std::string, CLoginData>::iterator itMapLoginData;
  bool m_bFakeUploads;
  size_t m_HTTPExpireTime;
};

size_t Write_CurlData_File(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t Write_CurlData_String(char *data, size_t size, size_t nmemb, std::string *buffer);

extern CHTTPHandler g_HTTPHandler; // supply one global HTTP handler
#endif