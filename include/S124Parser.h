#ifndef S124_PARSER_H
#define S124_PARSER_H

#include <wx/wx.h>
#include <wx/xml/xml.h>
#include <vector>
#include <utility>
#include "S124Warning.h"

class S124Parser
{
public:
    static bool ParseFile(const wxString &path,
                          std::vector<S124Warning> &out,
                          wxString &err);

    static bool ParseString(const wxString &gml,
                            std::vector<S124Warning> &out,
                            wxString &err);

private:
    static bool ParseDoc(wxXmlDocument &doc,
                         std::vector<S124Warning> &out,
                         wxString &err);

    static wxString LocalName(const wxString &qname);
    static wxString GetGmlId(wxXmlNode *node);
    static wxXmlNode *FindChild(wxXmlNode *parent, const wxString &localName);

    static void ExtractNavwarn(wxXmlNode *node,
                               S124Warning &w,
                               std::vector<wxString> &partRefs);

    static void ExtractNwPartGeoms(wxXmlNode *node,
                                   std::vector<S124Geometry> &geoms);

    static void ParseGeomNode(wxXmlNode *node,
                              std::vector<S124Geometry> &geoms);

    static void ParsePosList(const wxString &s,
                             std::vector<std::pair<double,double>> &out);

    static void ParsePos(const wxString &s,
                         std::vector<std::pair<double,double>> &out);

    static void CollectPosList(wxXmlNode *node,
                               std::vector<std::pair<double,double>> &out);
};

#endif // S124_PARSER_H
