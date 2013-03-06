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
#include "CTiXmlHelper.h"

using namespace std;

bool CTiXmlHelper::HasRoot(TiXmlDocument* document, std::string name, TiXmlElement* &root)
{
  root = document->RootElement();
  if (root && !root->NoChildren() && root->ValueTStr()==name.c_str())
  {
    return true;
  }
  else
    return false;
}
string CTiXmlHelper::GetAttribute(const TiXmlElement* node, const string name, const string defaults)
{
  bool present;
  string result = CTiXmlHelper::GetAttribute(node, name, present);

  return present ? result : defaults;
}
string CTiXmlHelper::GetAttribute(const TiXmlElement* node, const string name, bool &present)
{
  string result;
  present = false;

  if (node->Attribute(name.c_str()) && ( result = string(node->Attribute(name.c_str()))) != "")
  {
    present = true;
    return result;
  }
  else
  {
    return "";
  }
}
string CTiXmlHelper::GetInnerText(const TiXmlElement* node)
{
  string result;
  if (node && node->FirstChild() && (result = string(node->FirstChild()->Value())) != "")
  {
    return result;
  }
  else
    return "";
}