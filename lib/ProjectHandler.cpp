/*
 *      Copyright (C) 2014 Team Kodi
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
 *  along with Kodi; see the file COPYING.  If not, write to
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

bool CProjectHandler::FetchResourcesFromTransifex()
{
  g_HTTPHandler.Cleanup();
  g_HTTPHandler.ReInit();
  std::string strtemp = g_HTTPHandler.GetURLToSTR("https://www.transifex.com/api/2/project/" + g_Settings.GetProjectname()
                                                  + "/resources/");
  if (strtemp.empty())
    CLog::Log(logERROR, "ProjectHandler::FetchResourcesFromTransifex: error getting resources from transifex.net");

  char cstrtemp[strtemp.size()];
  strcpy(cstrtemp, strtemp.c_str());

  std::list<std::string> listResourceNamesTX = g_Json.ParseResources(strtemp);

  CResourceHandler ResourceHandler;
  for (std::list<std::string>::iterator it = listResourceNamesTX.begin(); it != listResourceNamesTX.end(); it++)
  {
    printf("Downloading resource from TX: %s (", it->c_str());
    CLog::Log(logLINEFEED, "");
    CLog::Log(logINFO, "ProjHandler: ****** FETCH Resource from TRANSIFEX: %s", it->c_str());
    CLog::IncIdent(4);

    std::string strResname = m_UpdateXMLHandler.GetResNameFromTXResName(*it);
    if (strResname.empty())
    {
      printf(" )\n");
      CLog::Log(logWARNING, "ProjHandler: found resource on Transifex which is not in xbmc-txupdate.xml: %s", it->c_str());
      CLog::DecIdent(4);
      continue;
    }

    m_mapResourcesTX[strResname]=ResourceHandler;
    m_mapResourcesTX[strResname].FetchPOFilesTXToMem("https://www.transifex.com/api/2/project/" + g_Settings.GetProjectname() +
                                              "/resource/" + *it + "/", strResname == "xbmc.core");
    CLog::DecIdent(4);
    printf(" )\n");
  }
  return true;
};

bool CProjectHandler::FetchResourcesFromUpstream()
{

  std::map<std::string, CXMLResdata> mapRes = m_UpdateXMLHandler.GetResMap();

  CResourceHandler ResourceHandler;

  for (std::map<std::string, CXMLResdata>::iterator it = mapRes.begin(); it != mapRes.end(); it++)
  {
    printf("Downloading resource from Upstream: %s (", it->first.c_str());
    CLog::Log(logLINEFEED, "");
    CLog::Log(logINFO, "ProjHandler: ****** FETCH Resource from UPSTREAM: %s ******", it->first.c_str());

    CLog::IncIdent(4);
    m_mapResourcesUpstr[it->first] = ResourceHandler;
    m_mapResourcesUpstr[it->first].FetchPOFilesUpstreamToMem(it->second, CreateMergedLanguageList(it->first, true));
    CLog::DecIdent(4);
    printf(" )\n");
  }
  return true;
};

bool CProjectHandler::WriteResourcesToFile(std::string strProjRootDir)
{
  std::string strPrefixDir;

  strPrefixDir = g_Settings.GetMergedLangfilesDir();
  CLog::Log(logINFO, "Deleting merged language file directory");
  g_File.DeleteDirectory(strProjRootDir + strPrefixDir);
  for (T_itmapRes itmapResources = m_mapResMerged.begin(); itmapResources != m_mapResMerged.end(); itmapResources++)
  {
    printf("Writing merged resources to HDD: %s\n", itmapResources->first.c_str());
    std::list<std::string> lChangedLangsFromUpstream = m_mapResMerged[itmapResources->first].GetChangedLangsFromUpstream();
    std::list<std::string> lChangedAddXMLLangsFromUpstream = m_mapResMerged[itmapResources->first].GetChangedLangsInAddXMLFromUpstream();
    if (!lChangedAddXMLLangsFromUpstream.empty())
    {
      printf("    Changed Langs in addon.xml file from upstream: ");
      PrintChangedLangs(lChangedAddXMLLangsFromUpstream);
      printf("\n");
    }
    if (!lChangedLangsFromUpstream.empty())
    {
      printf("    Changed Langs in strings files from upstream: ");
      PrintChangedLangs(lChangedLangsFromUpstream);
      printf ("\n");
    }

    CLog::Log(logLINEFEED, "");
    CLog::Log(logINFO, "ProjHandler: *** Write Merged Resource: %s ***", itmapResources->first.c_str());
    CLog::IncIdent(4);
    CXMLResdata XMLResdata = m_UpdateXMLHandler.GetResData(itmapResources->first);
    m_mapResMerged[itmapResources->first].WritePOToFiles (strProjRootDir, strPrefixDir, itmapResources->first, XMLResdata, false);
    CLog::DecIdent(4);
  }
  printf ("\n\n");

  strPrefixDir = g_Settings.GetTXUpdateLangfilesDir();
  CLog::Log(logINFO, "Deleting tx update language file directory");
  g_File.DeleteDirectory(strProjRootDir + strPrefixDir);
  for (T_itmapRes itmapResources = m_mapResUpdateTX.begin(); itmapResources != m_mapResUpdateTX.end(); itmapResources++)
  {
    printf("Writing update TX resources to HDD: %s\n", itmapResources->first.c_str());
    CLog::Log(logLINEFEED, "");
    CLog::Log(logINFO, "ProjHandler: *** Write UpdTX Resource: %s ***", itmapResources->first.c_str());
    CLog::IncIdent(4);
    CXMLResdata XMLResdata = m_UpdateXMLHandler.GetResData(itmapResources->first);
    m_mapResUpdateTX[itmapResources->first].WritePOToFiles (strProjRootDir, strPrefixDir, itmapResources->first, XMLResdata, true);
    CLog::DecIdent(4);
  }

  return true;
};

bool CProjectHandler::CreateMergedResources()
{
  CLog::Log(logINFO, "CreateMergedResources started");

  std::list<std::string> listMergedResource = CreateResourceList();

  m_mapResMerged.clear();
  m_mapResUpdateTX.clear();

  for (std::list<std::string>::iterator itResAvail = listMergedResource.begin(); itResAvail != listMergedResource.end(); itResAvail++)
  {
    printf("Merging resource: %s\n", itResAvail->c_str());
    CLog::SetSyntaxAddon(*itResAvail);
    CLog::Log(logINFO, "CreateMergedResources: Merging resource:%s", itResAvail->c_str());
    CLog::IncIdent(4);

    CResourceHandler mergedResHandler, updTXResHandler;
    std::list<std::string> lAddXMLLangsChgedFromUpstream, lLangsChgedFromUpstream;

    // Get available pretext for Resource Header. we use the upstream one
    std::string strResPreHeader;
    if (m_mapResourcesUpstr.find(*itResAvail) != m_mapResourcesUpstr.end())
      strResPreHeader = m_mapResourcesUpstr[*itResAvail].GetXMLHandler()->GetResHeaderPretext();
    else
      CLog::Log(logERROR, "CreateMergedResources: Not able to read addon data for header text");

    CAddonXMLEntry * pENAddonXMLEntry;

    if ((pENAddonXMLEntry = GetAddonDataFromXML(&m_mapResourcesUpstr, *itResAvail, "en")) != NULL)
    {
      mergedResHandler.GetXMLHandler()->SetStrAddonXMLFile(m_mapResourcesUpstr[*itResAvail].GetXMLHandler()->GetStrAddonXMLFile());
      mergedResHandler.GetXMLHandler()->SetAddonVersion(m_mapResourcesUpstr[*itResAvail].GetXMLHandler()->GetAddonVersion());
      mergedResHandler.GetXMLHandler()->SetAddonChangelogFile(m_mapResourcesUpstr[*itResAvail].GetXMLHandler()->GetAddonChangelogFile());
      mergedResHandler.GetXMLHandler()->SetAddonLogFilename(m_mapResourcesUpstr[*itResAvail].GetXMLHandler()->GetAddonLogFilename());
      mergedResHandler.GetXMLHandler()->SetAddonMetadata(m_mapResourcesUpstr[*itResAvail].GetXMLHandler()->GetAddonMetaData());
      updTXResHandler.GetXMLHandler()->SetStrAddonXMLFile(m_mapResourcesUpstr[*itResAvail].GetXMLHandler()->GetStrAddonXMLFile());
      updTXResHandler.GetXMLHandler()->SetAddonVersion(m_mapResourcesUpstr[*itResAvail].GetXMLHandler()->GetAddonVersion());
      updTXResHandler.GetXMLHandler()->SetAddonChangelogFile(m_mapResourcesUpstr[*itResAvail].GetXMLHandler()->GetAddonChangelogFile());
      updTXResHandler.GetXMLHandler()->SetAddonLogFilename(m_mapResourcesUpstr[*itResAvail].GetXMLHandler()->GetAddonLogFilename());
    }
    else if (*itResAvail != "xbmc.core")
      CLog::Log(logERROR, "CreateMergedResources: No Upstream AddonXML file found as source for merging");

    std::list<std::string> listMergedLangs = CreateMergedLanguageList(*itResAvail, false);

    CPOHandler * pcurrPOHandlerEN = m_mapResourcesUpstr[*itResAvail].GetPOData("en");

    for (std::list<std::string>::iterator itlang = listMergedLangs.begin(); itlang != listMergedLangs.end(); itlang++)
    {
      CLog::SetSyntaxLang(*itlang);
      std::string strLangCode = *itlang;
      CPOHandler mergedPOHandler, updTXPOHandler;
      const CPOEntry* pPOEntryTX;
      const CPOEntry* pPOEntryUpstr;
      bool bResChangedInAddXMLFromUpstream = false; bool bResChangedFromUpstream = false;

      mergedPOHandler.SetIfIsEnglish(strLangCode == "en");
      updTXPOHandler.SetIfIsEnglish(strLangCode == "en");
      updTXPOHandler.SetIfPOIsUpdTX(true);

      CAddonXMLEntry MergedAddonXMLEntry, MergedAddonXMLEntryTX;
      CAddonXMLEntry * pAddonXMLEntry;

      // Get addon.xml file translatable strings from Transifex to the merged Entry
      if (m_mapResourcesTX.find(*itResAvail) != m_mapResourcesTX.end() && m_mapResourcesTX[*itResAvail].GetPOData(*itlang))
      {
        CAddonXMLEntry AddonXMLEntryInPO, AddonENXMLEntryInPO;
        m_mapResourcesTX[*itResAvail].GetPOData(*itlang)->GetAddonMetaData(AddonXMLEntryInPO, AddonENXMLEntryInPO);
        MergeAddonXMLEntry(AddonXMLEntryInPO, MergedAddonXMLEntry, *pENAddonXMLEntry, AddonENXMLEntryInPO, false,
                           bResChangedInAddXMLFromUpstream);
      }
      // Save these strings from Transifex for later use
      MergedAddonXMLEntryTX = MergedAddonXMLEntry;

      // Get the addon.xml file translatable strings from upstream merged into the merged entry
      if ((pAddonXMLEntry = GetAddonDataFromXML(&m_mapResourcesUpstr, *itResAvail, *itlang)) != NULL)
        MergeAddonXMLEntry(*pAddonXMLEntry, MergedAddonXMLEntry, *pENAddonXMLEntry,
                           *GetAddonDataFromXML(&m_mapResourcesUpstr, *itResAvail, "en"), true, bResChangedInAddXMLFromUpstream);
      else if (!MergedAddonXMLEntryTX.strDescription.empty() || !MergedAddonXMLEntryTX.strSummary.empty() ||
               !MergedAddonXMLEntryTX.strDisclaimer.empty())
        bResChangedInAddXMLFromUpstream = true;

      if (*itResAvail != "xbmc.core")
      {
        mergedResHandler.GetXMLHandler()->GetMapAddonXMLData()->operator[](*itlang) = MergedAddonXMLEntry;
        updTXResHandler.GetXMLHandler()->GetMapAddonXMLData()->operator[](*itlang) = MergedAddonXMLEntry;
        updTXPOHandler.SetAddonMetaData(MergedAddonXMLEntry, MergedAddonXMLEntryTX, *pENAddonXMLEntry, *itlang); // add addonxml data as PO  classic entries
      }

      for (size_t POEntryIdx = 0; pcurrPOHandlerEN && POEntryIdx != pcurrPOHandlerEN->GetNumEntriesCount(); POEntryIdx++)
      {
        size_t numID = pcurrPOHandlerEN->GetNumPOEntryByIdx(POEntryIdx)->numID;

        CPOEntry currPOEntryEN = *(pcurrPOHandlerEN->GetNumPOEntryByIdx(POEntryIdx));
        currPOEntryEN.msgStr.clear();
        CPOEntry* pcurrPOEntryEN = &currPOEntryEN;

        pPOEntryTX = SafeGetPOEntry(m_mapResourcesTX, *itResAvail, strLangCode, numID);
        pPOEntryUpstr = SafeGetPOEntry(m_mapResourcesUpstr, *itResAvail, strLangCode, numID);

        CheckPOEntrySyntax(pPOEntryTX, strLangCode, pcurrPOEntryEN);

        if (strLangCode == "en") // English entry
        {
          mergedPOHandler.AddNumPOEntryByID(numID, *pcurrPOEntryEN, *pcurrPOEntryEN, true);
          updTXPOHandler.AddNumPOEntryByID(numID, *pcurrPOEntryEN, *pcurrPOEntryEN, true);
        }
        //1. Tx entry single
        else if (pPOEntryTX && pcurrPOEntryEN->msgIDPlur.empty() && !pPOEntryTX->msgStr.empty() &&
                 pPOEntryTX->msgID == pcurrPOEntryEN->msgID)
        {
          mergedPOHandler.AddNumPOEntryByID(numID, *pPOEntryTX, *pcurrPOEntryEN, true);
          if (g_Settings.GetForceTXUpdate())
            updTXPOHandler.AddNumPOEntryByID(numID, *pPOEntryTX, *pcurrPOEntryEN, true);
          //Check if the string is new on TX or changed from the upstream one -> Addon version bump later
          if ((!pPOEntryUpstr || pPOEntryUpstr->msgStr.empty() ||
              (!pPOEntryUpstr->msgID.empty() && pPOEntryUpstr->msgID != pcurrPOEntryEN->msgID)) &&
              (!pPOEntryUpstr || pPOEntryUpstr->msgStr != pPOEntryTX->msgStr))
              bResChangedFromUpstream = true;
        }
        //2. Tx entry plural
        else if (pPOEntryTX && !pcurrPOEntryEN->msgIDPlur.empty() && !pPOEntryTX->msgStrPlural.empty() &&
                 pPOEntryTX->msgIDPlur == pcurrPOEntryEN->msgIDPlur)
        {
          mergedPOHandler.AddNumPOEntryByID(numID, *pPOEntryTX, *pcurrPOEntryEN, true);
          if (g_Settings.GetForceTXUpdate())
            updTXPOHandler.AddNumPOEntryByID(numID, *pPOEntryTX, *pcurrPOEntryEN, true);
          //Check if the string is new on TX or changed from the upstream one -> Addon version bump later
          if ((!pPOEntryUpstr || pPOEntryUpstr->msgStrPlural.empty() ||
              (!pPOEntryUpstr->msgID.empty() && pPOEntryUpstr->msgID != pcurrPOEntryEN->msgID)) &&
              (!pPOEntryUpstr || pPOEntryUpstr->msgStrPlural != pPOEntryTX->msgStrPlural))
              bResChangedFromUpstream = true;
        }
        //3. Upstr entry single
        else if (pPOEntryUpstr && pcurrPOEntryEN->msgIDPlur.empty() && !pPOEntryUpstr->msgStr.empty() &&
                (pPOEntryUpstr->msgID.empty() || pPOEntryUpstr->msgID == pcurrPOEntryEN->msgID)) //if it is empty it is from a strings.xml file
        {
          mergedPOHandler.AddNumPOEntryByID(numID, *pPOEntryUpstr, *pcurrPOEntryEN, true);
          updTXPOHandler.AddNumPOEntryByID(numID, *pPOEntryUpstr, *pcurrPOEntryEN, false);
        }
        //4. Upstr entry plural
        else if (pPOEntryUpstr && !pcurrPOEntryEN->msgIDPlur.empty() && !pPOEntryUpstr->msgStrPlural.empty() &&
                 pPOEntryUpstr->msgIDPlur == pcurrPOEntryEN->msgIDPlur)
        {
          mergedPOHandler.AddNumPOEntryByID(numID, *pPOEntryUpstr, *pcurrPOEntryEN, true);
          updTXPOHandler.AddNumPOEntryByID(numID, *pPOEntryUpstr, *pcurrPOEntryEN, false);
        }

      }




      // Handle classic non-id based po entries
      for (size_t POEntryIdx = 0; pcurrPOHandlerEN && POEntryIdx != pcurrPOHandlerEN->GetClassEntriesCount(); POEntryIdx++)
      {

        CPOEntry currPOEntryEN = *(pcurrPOHandlerEN->GetClassicPOEntryByIdx(POEntryIdx));
        currPOEntryEN.msgStr.clear();
        CPOEntry* pcurrPOEntryEN = &currPOEntryEN;

        pPOEntryTX = SafeGetPOEntry(m_mapResourcesTX, *itResAvail, strLangCode, currPOEntryEN);
        pPOEntryUpstr = SafeGetPOEntry(m_mapResourcesUpstr, *itResAvail, strLangCode, currPOEntryEN);

        CheckPOEntrySyntax(pPOEntryTX, strLangCode, pcurrPOEntryEN);

        if (strLangCode == "en")
        {
          mergedPOHandler.AddClassicEntry(*pcurrPOEntryEN, *pcurrPOEntryEN, true);
          updTXPOHandler.AddClassicEntry(*pcurrPOEntryEN, *pcurrPOEntryEN, true);
        }
        else if (pPOEntryTX && pcurrPOEntryEN->msgIDPlur.empty() && !pPOEntryTX->msgStr.empty()) // Tx entry single
        {
          mergedPOHandler.AddClassicEntry(*pPOEntryTX, *pcurrPOEntryEN, true);
          if (g_Settings.GetForceTXUpdate())
            updTXPOHandler.AddClassicEntry(*pPOEntryTX, *pcurrPOEntryEN, true);
          //Check if the string is new on TX or changed from the upstream one -> Addon version bump later
          if (!pPOEntryUpstr || pPOEntryUpstr->msgStr.empty())
          {
            CPOEntry POEntryToCompare = *pPOEntryTX;
            POEntryToCompare.msgIDPlur.clear();
            POEntryToCompare.Type = 0;
            if (!pPOEntryUpstr || !(*pPOEntryUpstr == POEntryToCompare))
              bResChangedFromUpstream = true;
          }
        }
        else if (pPOEntryTX && !pcurrPOEntryEN->msgIDPlur.empty() && !pPOEntryTX->msgStrPlural.empty()) // Tx entry plural
        {
          mergedPOHandler.AddClassicEntry(*pPOEntryTX, *pcurrPOEntryEN, true);
          if (g_Settings.GetForceTXUpdate())
            updTXPOHandler.AddClassicEntry(*pPOEntryTX, *pcurrPOEntryEN, true);
          //Check if the string is new on TX or changed from the upstream one -> Addon version bump later
          if (!pPOEntryUpstr || pPOEntryUpstr->msgStrPlural.empty())
          {
            CPOEntry POEntryToCompare = *pPOEntryTX;
            POEntryToCompare.msgID.clear();
            POEntryToCompare.Type = 0;
            if (!pPOEntryUpstr || !(*pPOEntryUpstr == POEntryToCompare))
              bResChangedFromUpstream = true;
          }
        }
        else if (pPOEntryUpstr && pcurrPOEntryEN->msgIDPlur.empty() && !pPOEntryUpstr->msgStr.empty()) // Upstr entry single
        {
          mergedPOHandler.AddClassicEntry(*pPOEntryUpstr, *pcurrPOEntryEN, true);
          updTXPOHandler.AddClassicEntry(*pPOEntryUpstr, *pcurrPOEntryEN, false);
        }
        else if (pPOEntryUpstr && !pcurrPOEntryEN->msgIDPlur.empty() && !pPOEntryUpstr->msgStrPlural.empty()) // Upstr entry plural
        {
          mergedPOHandler.AddClassicEntry(*pPOEntryUpstr, *pcurrPOEntryEN, true);
          updTXPOHandler.AddClassicEntry(*pPOEntryUpstr, *pcurrPOEntryEN, false);
        }
      }

      CPOHandler * pPOHandlerTX;
      pPOHandlerTX = SafeGetPOHandler(m_mapResourcesTX, *itResAvail, strLangCode);

      if (mergedPOHandler.GetNumEntriesCount() !=0 || mergedPOHandler.GetClassEntriesCount() !=0)
      {
        mergedPOHandler.SetPreHeader(strResPreHeader);
        mergedPOHandler.SetHeaderNEW(*itlang);
        mergedResHandler.AddPOData(mergedPOHandler, strLangCode);
      }

      if ((updTXPOHandler.GetNumEntriesCount() !=0 || updTXPOHandler.GetClassEntriesCount() !=0) &&
        (strLangCode != "en" || g_Settings.GetForceTXUpdate() ||
        !g_HTTPHandler.ComparePOFilesInMem(&updTXPOHandler, pPOHandlerTX, strLangCode == "en")))
      {
        updTXPOHandler.SetPreHeader(strResPreHeader);
        updTXPOHandler.SetHeaderNEW(*itlang);
        updTXResHandler.AddPOData(updTXPOHandler, strLangCode);
      }

      CLog::LogTable(logINFO, "merged", "\t\t\t%s\t\t%i\t\t%i\t\t%i\t\t%i", strLangCode.c_str(), mergedPOHandler.GetNumEntriesCount(),
                     mergedPOHandler.GetClassEntriesCount(), updTXPOHandler.GetNumEntriesCount(), updTXPOHandler.GetClassEntriesCount());

     //store what languages changed from upstream in strings.po and addon.xml files
     if (bResChangedFromUpstream)
       lLangsChgedFromUpstream.push_back(strLangCode);
     if (bResChangedInAddXMLFromUpstream)
       lAddXMLLangsChgedFromUpstream.push_back(strLangCode);
    }
    CLog::LogTable(logADDTABLEHEADER, "merged", "--------------------------------------------------------------------------------------------\n");
    CLog::LogTable(logADDTABLEHEADER, "merged", "MergedPOHandler:\tLang\t\tmergedID\tmergedClass\tupdID\t\tupdClass\n");
    CLog::LogTable(logADDTABLEHEADER, "merged", "--------------------------------------------------------------------------------------------\n");
    CLog::LogTable(logCLOSETABLE, "merged",   "");

    mergedResHandler.SetChangedLangsFromUpstream(lLangsChgedFromUpstream);
    mergedResHandler.SetChangedLangsInAddXMLFromUpstream(lAddXMLLangsChgedFromUpstream);

    if (mergedResHandler.GetLangsCount() != 0 || !mergedResHandler.GetXMLHandler()->GetMapAddonXMLData()->empty())
      m_mapResMerged[*itResAvail] = mergedResHandler;
    if (updTXResHandler.GetLangsCount() != 0 || !updTXResHandler.GetXMLHandler()->GetMapAddonXMLData()->empty())
      m_mapResUpdateTX[*itResAvail] = updTXResHandler;
    CLog::DecIdent(4);
  }
  return true;
}

std::list<std::string> CProjectHandler::CreateMergedLanguageList(std::string strResname, bool bOnlyTX)
{
  std::list<std::string> listMergedLangs;

  if (m_mapResourcesTX.find(strResname) != m_mapResourcesTX.end())
  {

    // Add languages exist in transifex PO files
    for (size_t i =0; i != m_mapResourcesTX[strResname].GetLangsCount(); i++)
    {
      std::string strMLCode = m_mapResourcesTX[strResname].GetLangCodeFromPos(i);
      if (std::find(listMergedLangs.begin(), listMergedLangs.end(), strMLCode) == listMergedLangs.end())
        listMergedLangs.push_back(strMLCode);
    }
  }
  if (bOnlyTX)
    return listMergedLangs;

  if (m_mapResourcesUpstr.find(strResname) != m_mapResourcesUpstr.end())
  {

    // Add languages exist in upstream PO or XML files
    for (size_t i =0; i != m_mapResourcesUpstr[strResname].GetLangsCount(); i++)
    {
      std::string strMLCode = m_mapResourcesUpstr[strResname].GetLangCodeFromPos(i);
      if (std::find(listMergedLangs.begin(), listMergedLangs.end(), strMLCode) == listMergedLangs.end())
        listMergedLangs.push_back(strMLCode);
    }

    // Add languages only exist in addon.xml files
    std::map<std::string, CAddonXMLEntry> * pMapUpstAddonXMLData = m_mapResourcesUpstr[strResname].GetXMLHandler()->GetMapAddonXMLData();
    for (std::map<std::string, CAddonXMLEntry>::iterator it = pMapUpstAddonXMLData->begin(); it != pMapUpstAddonXMLData->end(); it++)
    {
      if (std::find(listMergedLangs.begin(), listMergedLangs.end(), it->first) == listMergedLangs.end())
        listMergedLangs.push_back(it->first);
    }
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

  return listMergedResources;
}

CAddonXMLEntry * const CProjectHandler::GetAddonDataFromXML(std::map<std::string, CResourceHandler> * pmapRes,
                                                            const std::string &strResname, const std::string &strLangCode) const
{
  if (pmapRes->find(strResname) == pmapRes->end())
    return NULL;

  CResourceHandler * pRes = &(pmapRes->operator[](strResname));
  if (pRes->GetXMLHandler()->GetMapAddonXMLData()->find(strLangCode) == pRes->GetXMLHandler()->GetMapAddonXMLData()->end())
    return NULL;

  return &(pRes->GetXMLHandler()->GetMapAddonXMLData()->operator[](strLangCode));
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

const CPOEntry * CProjectHandler::SafeGetPOEntry(std::map<std::string, CResourceHandler> &mapResHandl, const std::string &strResname,
                                                 std::string &strLangCode, CPOEntry const &currPOEntryEN)
{
  CPOEntry POEntryToFind;
  POEntryToFind.Type = currPOEntryEN.Type;
  POEntryToFind.msgCtxt = currPOEntryEN.msgCtxt;
  POEntryToFind.msgID = currPOEntryEN.msgID;
  POEntryToFind.msgIDPlur = currPOEntryEN.msgIDPlur;

  if (mapResHandl.find(strResname) == mapResHandl.end())
    return NULL;
  if (!mapResHandl[strResname].GetPOData(strLangCode))
    return NULL;
  return mapResHandl[strResname].GetPOData(strLangCode)->PLookforClassicEntry(POEntryToFind);
}

CPOHandler * CProjectHandler::SafeGetPOHandler(std::map<std::string, CResourceHandler> &mapResHandl, const std::string &strResname,
                                                 std::string &strLangCode)
{
  if (mapResHandl.find(strResname) == mapResHandl.end())
    return NULL;
  return mapResHandl[strResname].GetPOData(strLangCode);
}

void CProjectHandler::MergeAddonXMLEntry(CAddonXMLEntry const &EntryToMerge, CAddonXMLEntry &MergedAddonXMLEntry,
                                         CAddonXMLEntry const &SourceENEntry, CAddonXMLEntry const &CurrENEntry, bool UpstrToMerge,
                                         bool &bResChangedFromUpstream)
{
  if (!EntryToMerge.strDescription.empty() && MergedAddonXMLEntry.strDescription.empty() &&
      CurrENEntry.strDescription == SourceENEntry.strDescription)
    MergedAddonXMLEntry.strDescription = EntryToMerge.strDescription;
  else if (UpstrToMerge && !MergedAddonXMLEntry.strDescription.empty() &&
           EntryToMerge.strDescription != MergedAddonXMLEntry.strDescription &&
           CurrENEntry.strDescription == SourceENEntry.strDescription)
  {
    bResChangedFromUpstream = true;
  }

  if (!EntryToMerge.strDisclaimer.empty() && MergedAddonXMLEntry.strDisclaimer.empty() &&
      CurrENEntry.strDisclaimer == SourceENEntry.strDisclaimer)
    MergedAddonXMLEntry.strDisclaimer = EntryToMerge.strDisclaimer;
  else if (UpstrToMerge && !MergedAddonXMLEntry.strDisclaimer.empty() &&
           EntryToMerge.strDisclaimer != MergedAddonXMLEntry.strDisclaimer &&
           CurrENEntry.strDisclaimer == SourceENEntry.strDisclaimer)
  {
    bResChangedFromUpstream = true;
  }

  if (!EntryToMerge.strSummary.empty() && MergedAddonXMLEntry.strSummary.empty() &&
      CurrENEntry.strSummary == SourceENEntry.strSummary)
    MergedAddonXMLEntry.strSummary = EntryToMerge.strSummary;
    else if (UpstrToMerge && !MergedAddonXMLEntry.strSummary.empty() &&
           EntryToMerge.strSummary != MergedAddonXMLEntry.strSummary &&
           CurrENEntry.strSummary == SourceENEntry.strSummary)
  {
    bResChangedFromUpstream = true;
  }
}

void CProjectHandler::InitUpdateXMLHandler(std::string strProjRootDir)
{
m_UpdateXMLHandler.LoadXMLToMem(strProjRootDir);
}

void CProjectHandler::UploadTXUpdateFiles(std::string strProjRootDir)
{
  g_HTTPHandler.Cleanup();
  g_HTTPHandler.ReInit();
  std::string strtemp = g_HTTPHandler.GetURLToSTR("https://www.transifex.com/api/2/project/" + g_Settings.GetProjectname()
  + "/resources/");
  if (strtemp.empty())
    CLog::Log(logERROR, "ProjectHandler::FetchResourcesFromTransifex: error getting resources from transifex.net");

  std::list<std::string> listResourceNamesTX = g_Json.ParseResources(strtemp);

  std::map<std::string, CXMLResdata> mapUpdateXMLHandler = m_UpdateXMLHandler.GetResMap();
  std::string strPrefixDir = g_Settings.GetTXUpdateLangfilesDir();

  g_HTTPHandler.Cleanup();
  g_HTTPHandler.ReInit();

  for (std::map<std::string, CXMLResdata>::iterator itres = mapUpdateXMLHandler.begin(); itres != mapUpdateXMLHandler.end(); itres++)
  {
    std::string strResourceDir, strLangDir;
    CXMLResdata XMLResdata = itres->second;
    std::string strResname = itres->first;
    bool bNewResource = false;

    if (!XMLResdata.strResDirectory.empty())
      XMLResdata.strResDirectory += DirSepChar;

    switch (XMLResdata.Restype)
    {
      case ADDON: case ADDON_NOSTRINGS:
        strResourceDir = strProjRootDir + strPrefixDir + DirSepChar + XMLResdata.strResDirectory + strResname + DirSepChar + XMLResdata.strDIRprefix + DirSepChar;
        strLangDir = strResourceDir + "resources" + DirSepChar + "language" + DirSepChar;
        break;
      case SKIN:
        strResourceDir = strProjRootDir + strPrefixDir + DirSepChar + XMLResdata.strResDirectory + strResname + DirSepChar + XMLResdata.strDIRprefix + DirSepChar;
        strLangDir = strResourceDir + "language" + DirSepChar;
        break;
      case CORE:
        strResourceDir = strProjRootDir + strPrefixDir + DirSepChar + XMLResdata.strResDirectory;
        strLangDir = strResourceDir + "language" + DirSepChar;
        break;
      default:
        CLog::Log(logERROR, "ResHandler: No resourcetype defined for resource: %s",strResname.c_str());
    }

    CLog::Log(logINFO, "CProjectHandler::UploadTXUpdateFiles: Uploading resource: %s, from langdir: %s",itres->first.c_str(), strLangDir.c_str());
    printf ("Uploading files for resource: %s", itres->first.c_str());

    if (!FindResInList(listResourceNamesTX, itres->second.strTXResName))
    {
      CLog::Log(logINFO, "CProjectHandler::UploadTXUpdateFiles: No resource %s exists on Transifex. Creating it now.", itres->first.c_str());
      // We create the new resource on transifex and also upload the English source file at once
      g_HTTPHandler.Cleanup();
      g_HTTPHandler.ReInit();
      size_t straddednew;
      g_HTTPHandler.CreateNewResource(itres->second.strTXResName,
                                      strLangDir + "English" + DirSepChar + "strings.po",
                                      "https://www.transifex.com/api/2/project/" + g_Settings.GetProjectname() + "/resources/",
                                      straddednew, "https://www.transifex.com/api/2/project/" + g_Settings.GetProjectname() +
                                      "/resource/" + XMLResdata.strTXResName + "/translation/" + "en" + "/");

      CLog::Log(logINFO, "CProjectHandler::UploadTXUpdateFiles: Resource %s was succesfully created with %i English strings.",
                itres->first.c_str(), straddednew);
      printf (", newly created on Transifex with %lu English strings.\n", straddednew);

      g_HTTPHandler.Cleanup();
      g_HTTPHandler.ReInit();
      bNewResource = true;
      g_HTTPHandler.DeleteCachedFile("https://www.transifex.com/api/2/project/" + g_Settings.GetProjectname() + "/resources/", "GET");
    }

    std::list<std::string> listLangCodes = GetLangsFromDir(strLangDir);

    if (listLangCodes.empty()) // No update needed for the specific resource (not even an English one)
    {
      CLog::Log(logINFO, "CProjectHandler::GetLangsFromDir: no English directory found at langdir: %s,"
                         " skipping upload for this resource.", strLangDir.c_str());
      printf (", no upload was requested.\n");
      continue;
    }
    else if (!bNewResource)
      printf("\n");

    for (std::list<std::string>::const_iterator it = listLangCodes.begin(); it!=listLangCodes.end(); it++)
    {
      if (bNewResource && *it == "en") // Let's not upload the English file again
        continue;
      std::string strFilePath = strLangDir + g_LCodeHandler.FindLang(*it) + DirSepChar + "strings.po";
      std::string strLangCode = *it;

      bool buploaded = false;
      size_t stradded, strupd;
      if (*it == "en")
        g_HTTPHandler.PutFileToURL(strFilePath, "https://www.transifex.com/api/2/project/" + g_Settings.GetProjectname() +
                                                "/resource/" + XMLResdata.strTXResName + "/content/",
                                                buploaded, stradded, strupd);
      else
        g_HTTPHandler.PutFileToURL(strFilePath, "https://www.transifex.com/api/2/project/" + g_Settings.GetProjectname() +
                                                "/resource/" + XMLResdata.strTXResName + "/translation/" + strLangCode + "/",
                                                buploaded, stradded, strupd);
      if (buploaded)
      {
        printf ("\tlangcode: %s:\t added strings:%lu, updated strings:%lu\n", it->c_str(), stradded, strupd);
        g_HTTPHandler.DeleteCachedFile("https://www.transifex.com/api/2/project/" + g_Settings.GetProjectname() +
                                       "/resource/" + strResname + "/stats/", "GET");
        g_HTTPHandler.DeleteCachedFile("https://www.transifex.com/api/2/project/" + g_Settings.GetProjectname() +
        "/resource/" + strResname + "/translation/" + *it + "/?file", "GET");
      }
      else
        printf ("\tlangcode: %s:\t no change, skipping.\n", it->c_str());
    }
  }
}

bool CProjectHandler::FindResInList(std::list<std::string> const &listResourceNamesTX, std::string strTXResName)
{
  for (std::list<std::string>::const_iterator it = listResourceNamesTX.begin(); it!=listResourceNamesTX.end(); it++)
  {
    if (*it == strTXResName)
      return true;
  }
  return false;
}

std::list<std::string> CProjectHandler::GetLangsFromDir(std::string const &strLangDir)
{
  std::list<std::string> listDirs;
  bool bEnglishExists = true;
  if (!g_File.DirExists(strLangDir + "English"))
    bEnglishExists = false;

  DIR* Dir;
  struct dirent *DirEntry;
  Dir = opendir(strLangDir.c_str());

  while(Dir && (DirEntry=readdir(Dir)))
  {
    if (DirEntry->d_type == DT_DIR && DirEntry->d_name[0] != '.')
    {
      std::string strDirname = DirEntry->d_name;
      if (strDirname != "English")
        listDirs.push_back(g_LCodeHandler.FindLangCode(DirEntry->d_name));
    }
  }

  listDirs.sort();
  if (bEnglishExists)
    listDirs.push_front("en");

  return listDirs;
};

void CProjectHandler::CheckPOEntrySyntax(const CPOEntry * pPOEntry, std::string const &strLangCode, const CPOEntry * pcurrPOEntryEN)
{
  if (!pPOEntry)
    return;

  CheckCharCount(pPOEntry, strLangCode, pcurrPOEntryEN, '%');
  CheckCharCount(pPOEntry, strLangCode, pcurrPOEntryEN, '\n');

  return;
}

std::string CProjectHandler::GetEntryContent(const CPOEntry * pPOEntry, std::string const &strLangCode)
{
  if (!pPOEntry)
    return "";

  std::string strReturn;
  strReturn += "\n";

  if (pPOEntry->Type == ID_FOUND)
    strReturn += "msgctxt \"#" + g_CharsetUtils.IntToStr(pPOEntry->numID) + "\"\n";
  else if (!pPOEntry->msgCtxt.empty())
    strReturn += "msgctxt \"" + g_CharsetUtils.EscapeStringCPP(pPOEntry->msgCtxt) + "\"\n";

  strReturn += "msgid \""  + g_CharsetUtils.EscapeStringCPP(pPOEntry->msgID) +  "\"\n";

  if (strLangCode != "en")
    strReturn += "msgstr \"" + g_CharsetUtils.EscapeStringCPP(pPOEntry->msgStr) + "\"\n";
  else
    strReturn += "msgstr \"\"\n";

  return strReturn;
}

void CProjectHandler::CheckCharCount(const CPOEntry * pPOEntry, std::string const &strLangCode, const CPOEntry * pcurrPOEntryEN, char chrToCheck)
{
  // check '%' count in msgid and msgstr entries
  size_t count = g_CharsetUtils.GetCharCountInStr(pcurrPOEntryEN->msgID, chrToCheck);
  if (!pPOEntry->msgIDPlur.empty() && count != g_CharsetUtils.GetCharCountInStr(pPOEntry->msgIDPlur, chrToCheck))
    CLog::SyntaxLog(logWARNING, "Warning: count missmatch of char \"%s\"%s",
                   g_CharsetUtils.EscapeStringCPP(g_CharsetUtils.ChrToStr(chrToCheck)).c_str(), GetEntryContent(pPOEntry, strLangCode).c_str());

  if (strLangCode != "en")
  {
    if (!pPOEntry->msgStr.empty() && count != g_CharsetUtils.GetCharCountInStr(pPOEntry->msgStr, chrToCheck))
      CLog::SyntaxLog(logWARNING, "Warning: count missmatch of char \"%s\"%s",
                      g_CharsetUtils.EscapeStringCPP(g_CharsetUtils.ChrToStr(chrToCheck)).c_str(), GetEntryContent(pPOEntry, strLangCode).c_str());

      for (std::vector<std::string>::const_iterator it =  pPOEntry->msgStrPlural.begin() ; it != pPOEntry->msgStrPlural.end() ; it++)
      {
        if (count != g_CharsetUtils.GetCharCountInStr(*it, '%'))
          CLog::SyntaxLog(logWARNING, "Warning: count missmatch of char \"%s\"%s",
                          g_CharsetUtils.EscapeStringCPP(g_CharsetUtils.ChrToStr(chrToCheck)).c_str(), GetEntryContent(pPOEntry, strLangCode).c_str());
      }
  }
}

void CProjectHandler::PrintChangedLangs(std::list<std::string> lChangedLangs)
{
  std::list<std::string>::iterator itLangs;
  std::size_t counter = 0;
  for (itLangs = lChangedLangs.begin() ; itLangs != lChangedLangs.end(); itLangs++)
  {
    printf ("%s ", itLangs->c_str());
    counter++;
    if (counter > 19)
    {
      printf ("+ %i langs ", lChangedLangs.size() - 20);
      break;
    }
  }
}
