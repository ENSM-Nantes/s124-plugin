#ifndef XML_LOADER_H
#define XML_LOADER_H

#include <wx/wx.h>
#include <vector>
#include "PointsLayer.h"

class XmlLoader
{
public:
    static bool LoadPoints(const wxString &filePath,
                           std::vector<XmlPoint> &points,
                           wxString &errorMsg);
};

#endif // XML_LOADER_H
