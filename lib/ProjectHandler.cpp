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

#include "ProjectHandler.h"
#include <list>
#include "HTTPUtils.h"
#include "JSONHandler.h"
#include "Settings.h"
#include <algorithm>

CProjectHandler::CProjectHandler()
{};

CProjectHandler::~CProjectHandler()
{};

bool CProjectHandler::LoadProject(std::string strProjRootDir)
{
  GetResourcesFromDir(strProjRootDir);
  int loadCounter = 0;

  for (T_itmapRes itmapResources = m_mapResourcesLocal.begin(); itmapResources != m_mapResourcesLocal.end(); itmapResources++)
  {
    CLog::Log(logLINEFEED, "");
    CLog::Log(logINFO, "ProjHandler: *** Resource: %s ***", itmapResources->first.c_str());
    if (itmapResources->second.LoadResource(strProjRootDir + itmapResources->first + DirSepChar, ""))
      loadCounter++;
  };

  CLog::Log(logLINEFEED, "");
  CLog::Log(logINFO, "ProjHandler: Loaded %i resources from root dir: %s", loadCounter, strProjRootDir.c_str());

  return true;
};

bool CProjectHandler::GetResourcesFromDir(std::string strProjRootDir)
{
  std::list<std::string> listDirs;
  std::list<std::string>::iterator itlistDirs;
  DIR* Dir;
  struct dirent *DirEntry;
  Dir = opendir(strProjRootDir.c_str());

  while((DirEntry=readdir(Dir)))
  {
    if (DirEntry->d_type == DT_DIR && DirEntry->d_name[0] != '.')
      listDirs.push_back(DirEntry->d_name);
  }
  listDirs.sort();

  int resCounter = 0;
  for (itlistDirs = listDirs.begin(); itlistDirs != listDirs.end(); itlistDirs++)
  {
    resCounter++;
    CResourceHandler ResourceHandler;
    m_mapResourcesLocal[*itlistDirs] = ResourceHandler;
  }

  CLog::Log(logINFO, "ProjHandler: Found %i resources at root dir: %s", resCounter, strProjRootDir.c_str());

  return true;
};

bool CProjectHandler::FetchResourcesFromTransifex(std::string strProjRootDir)
{

  std::string strtemp = g_HTTPHandler.GetURLToSTR("https://www.transifex.com/api/2/project/" + g_Settings.GetProjectname()
                                                  + "/resources/");

  char cstrtemp[strtemp.size()];
  strcpy(cstrtemp, strtemp.c_str());

  CJSONHandler JSONHandler;
  m_mapResourceNamesTX.clear();
  m_mapResourceNamesTX = JSONHandler.ParseResources(strtemp);

  CResourceHandler ResourceHandler;

  for (std::map<std::string, std::string>::iterator it = m_mapResourceNamesTX.begin(); it != m_mapResourceNamesTX.end(); it++)
  {
    CLog::Log(logLINEFEED, "");
    CLog::Log(logINFO, "ProjHandler: *** Fetch Resource: %s ***", it->first.c_str());
    if (!DirExists(strProjRootDir + it->first))
    {
      CLog::Log(logERROR, "ProjHandler: Creating local directory for new resource on Transifex: %s", it->first.c_str());
      MakeDir(strProjRootDir + it->first);
    }
    m_UpdateXMLHandler.AddResourceToXMLFile(it->first);
    m_mapResourcesTX[it->first] = ResourceHandler;
    m_mapResourcesTX[it->first].FetchPOFilesTXToMem("https://www.transifex.com/api/2/project/" + g_Settings.GetProjectname()
                                              + "/resource/" + it->first + "/", it->second);
  }
  return true;
};

bool CProjectHandler::FetchResourcesFromUpstream(std::string strProjRootDir)
{

  std::map<std::string, CXMLResdata> mapRes = m_UpdateXMLHandler.GetResMap();
  std::map<std::string, CXMLResdata> mapResWithUpstream;

  for (std::map<std::string, CXMLResdata>::iterator it = mapRes.begin(); it != mapRes.end(); it++)
  {
    if (!it->second.strUptreamURL.empty())
      mapResWithUpstream[it->first] = it->second;
  }

  CResourceHandler ResourceHandler;

  for (std::map<std::string, CXMLResdata>::iterator it = mapResWithUpstream.begin(); it != mapResWithUpstream.end(); it++)
  {
    CLog::Log(logLINEFEED, "");
    CLog::Log(logINFO, "ProjHandler: *** Fetch Resource from upstream: %s ***", it->first.c_str());
    if (!DirExists(strProjRootDir + it->first))
    {
      CLog::Log(logERROR, "ProjHandler: Creating local directory for new resource in update.xml: %s", it->first.c_str());
      MakeDir(strProjRootDir + it->first);
    }
    m_mapResourcesUpstr[it->first] = ResourceHandler;
    m_mapResourcesUpstr[it->first].FetchPOFilesUpstreamToMem(it->second, m_mapResourcesLocal[it->first].GetCurrResType());
  }
  return true;
};

bool CProjectHandler::WriteResourcesToFile(std::string strProjRootDir, std::string strPOSuffix)
{
  for (T_itmapRes itmapResources = m_mapResMerged.begin(); itmapResources != m_mapResMerged.end(); itmapResources++)
  {
    CLog::Log(logLINEFEED, "");
    CLog::Log(logINFO, "ProjHandler: *** Write Resource: %s ***", itmapResources->first.c_str());
    if (!DirExists(strProjRootDir + itmapResources->first))
    {
      CLog::Log(logERROR, "ProjHandler: Creating local directory for new resource on Transifex: %s", itmapResources->first.c_str());
      MakeDir(strProjRootDir + itmapResources->first + DirSepChar);
    }
    m_mapResMerged[itmapResources->first].WritePOToFiles (strProjRootDir + itmapResources->first + DirSepChar, strPOSuffix);
  }
  return true;
};

bool CProjectHandler::CreateMergedResources()
{
  CLog::Log(logINFO, "CreateMergedResources started");

  std::list<std::string> listMergedResource = CreateResourceList();

  m_mapResMerged.clear();

  for (std::list<std::string>::iterator itResAvail = listMergedResource.begin(); itResAvail != listMergedResource.end(); itResAvail++)
  {
    printf("Merging resource: %s\n", itResAvail->c_str());
    CLog::Log(logINFO, "CreateMergedResources: Merging resource:%s", itResAvail->c_str());


    CResourceHandler * pcurrResHandler = ChoosePrefResMap(*itResAvail);

    CResourceHandler mergedResHandler;
    mergedResHandler.SetResType(pcurrResHandler->GetResType());
    mergedResHandler.SetXMLHandler(pcurrResHandler->GetXMLHandler());
    mergedResHandler.SetLangDir(pcurrResHandler->GetLangDir());

    CLog::Log(logINFO, "MergedPOHandl:\tLanguage\t\t\tID entries\tnon-ID entries\tInterline-comments");

    std::list<std::string> listMergedLangs = CreateLanguageList(*itResAvail);

    for (std::list<std::string>::iterator itlang = listMergedLangs.begin(); itlang != listMergedLangs.end(); itlang++)
    {
      std::string strLangCode = *itlang;
      CPOHandler mergedPOHandler;
      const CPOEntry* pPOEntryTX;
      const CPOEntry* pPOEntryUpstr;
      const CPOEntry* pPOEntryLocal;

      mergedPOHandler.SetIfIsEnglish(strLangCode == "en");
//      mergedPOHandler.SetAddonMetaData(pcurrResHandler->GetXMLHandler().);

      for (size_t POEntryIdx = 0; POEntryIdx != pcurrResHandler->GetPOData("en")->GetNumEntriesCount(); POEntryIdx++)
      {
        size_t numID = pcurrResHandler->GetPOData("en")->GetNumPOEntryByIdx(POEntryIdx)->numID;
        const CPOEntry* pcurrPOEntryEN = pcurrResHandler->GetPOData("en")->GetNumPOEntryByIdx(POEntryIdx);

        pPOEntryTX = SafeGetPOEntry(m_mapResourcesTX, *itResAvail, strLangCode, numID);
        pPOEntryUpstr = SafeGetPOEntry(m_mapResourcesUpstr, *itResAvail, strLangCode, numID);
        pPOEntryLocal = SafeGetPOEntry(m_mapResourcesLocal, *itResAvail, strLangCode, numID);


        if (*itResAvail == "xbmc-core" && strLangCode == "en")
        {
//          printf("ilcommUpstr: %i, ilcommTX:%i\n", pPOEntryUpstr->interlineComm.size(), pPOEntryTX->interlineComm.size());
        }

        if (strLangCode != "en" && pPOEntryTX && pPOEntryTX->msgID == pcurrPOEntryEN->msgID && !pPOEntryTX->msgStr.empty())
          mergedPOHandler.AddNumPOEntryByID(numID, *pPOEntryTX);
        else if (pPOEntryUpstr && pPOEntryUpstr->msgID == pcurrPOEntryEN->msgID && !pPOEntryUpstr->msgStr.empty())
          mergedPOHandler.AddNumPOEntryByID(numID, *pPOEntryUpstr);
        else if (pPOEntryLocal && pPOEntryLocal->msgID == pcurrPOEntryEN->msgID && !pPOEntryLocal->msgStr.empty())
          mergedPOHandler.AddNumPOEntryByID(numID, *pPOEntryLocal);
        else
          mergedPOHandler.AddNumPOEntryByID(numID, *pcurrPOEntryEN);

      }
      if (mergedPOHandler.GetNumEntriesCount() !=0 || mergedPOHandler.GetClassEntriesCount() !=0)
      {
        mergedResHandler.AddPOData(mergedPOHandler, strLangCode);
        CLog::Log(logINFO, "MegredPOHandler: %s\t\t%i\t\t%i\t\t%i", strLangCode.c_str(), mergedPOHandler.GetNumEntriesCount(),
                  mergedPOHandler.GetClassEntriesCount(), mergedPOHandler.GetCommntEntriesCount());
      }
    }
    if (mergedResHandler.GetLangsCount() != 0)
      m_mapResMerged[*itResAvail] = mergedResHandler;
  }
  return true;
}

std::list<std::string> CProjectHandler::CreateLanguageList(std::string strResname)
{
  std::list<std::string> listMergedLangs;
  for (size_t i =0; i != m_mapResourcesTX[strResname].GetLangsCount(); i++)
  {
    std::string strMLCode = m_mapResourcesTX[strResname].GetLangCodeFromPos(i);
    if (std::find(listMergedLangs.begin(), listMergedLangs.end(), strMLCode) == listMergedLangs.end())
      listMergedLangs.push_back(strMLCode);
  }

  for (size_t i =0; i != m_mapResourcesLocal[strResname].GetLangsCount(); i++)
  {
    std::string strMLCode = m_mapResourcesLocal[strResname].GetLangCodeFromPos(i);
    if (std::find(listMergedLangs.begin(), listMergedLangs.end(), strMLCode) == listMergedLangs.end())
      listMergedLangs.push_back(strMLCode);
  }

  for (size_t i =0; i != m_mapResourcesUpstr[strResname].GetLangsCount(); i++)
  {
    std::string strMLCode = m_mapResourcesUpstr[strResname].GetLangCodeFromPos(i);
    if (std::find(listMergedLangs.begin(), listMergedLangs.end(), strMLCode) == listMergedLangs.end())
      listMergedLangs.push_back(strMLCode);
  }

  return listMergedLangs;
}

std::list<std::string> CProjectHandler::CreateResourceList()
{
  std::list<std::string> listMergedResources;
  for (T_itmapRes it = m_mapResourcesUpstr.begin(); it != m_mapResourcesUpstr.end(); it++)
  {
    if (std::find(listMergedResources.begin(), listMergedResources.end(), it->first) == listMergedResources.end())
      listMergedResources.push_back(it->first);
  }

  for (T_itmapRes it = m_mapResourcesTX.begin(); it != m_mapResourcesTX.end(); it++)
  {
    if (std::find(listMergedResources.begin(), listMergedResources.end(), it->first) == listMergedResources.end())
      listMergedResources.push_back(it->first);
  }

  for (T_itmapRes it = m_mapResourcesLocal.begin(); it != m_mapResourcesLocal.end(); it++)
  {
    if (std::find(listMergedResources.begin(), listMergedResources.end(), it->first) == listMergedResources.end())
      listMergedResources.push_back(it->first);
  }
  return listMergedResources;
}

CResourceHandler * CProjectHandler::ChoosePrefResMap(std::string strResname)
{
  std::map<std::string, CResourceHandler> * pmapRes;
  if (m_mapResourcesUpstr.find(strResname) != m_mapResourcesUpstr.end())
  {
    pmapRes = &m_mapResourcesUpstr;
    CLog::Log(logINFO, "CreateMergedResources: Using upstream English file as source for merging");
  }
  else if (m_mapResourcesTX.find(strResname) != m_mapResourcesTX.end())
  {
    pmapRes = &m_mapResourcesTX;
    CLog::Log(logINFO, "CreateMergedResources: Using Transifex English file as source for merging");
  }
  else
  {
    pmapRes = &m_mapResourcesLocal;
    CLog::Log(logINFO, "CreateMergedResources: Using Local English file as source for merging");
  }
  return &((*pmapRes)[strResname]);
}

const CPOEntry * CProjectHandler::SafeGetPOEntry(std::map<std::string, CResourceHandler> &mapResHandl, const std::string &strResname,
                          std::string &strLangCode, size_t numID)
{
  if (mapResHandl.find(strResname) == mapResHandl.end())
    return NULL;
  if (!mapResHandl[strResname].GetPOData(strLangCode))
    return NULL;
  return mapResHandl[strResname].GetPOData(strLangCode)->GetNumPOEntryByID(numID);
}



void CProjectHandler::InitUpdateXMLHandler(std::string strProjRootDir)
{
m_UpdateXMLHandler.LoadXMLToMem(strProjRootDir);
}

void CProjectHandler::SaveUpdateXML(std::string strProjRootDir)
{
m_UpdateXMLHandler.SaveMemToXML(strProjRootDir);
}
