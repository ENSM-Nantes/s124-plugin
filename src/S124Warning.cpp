#include "S124Warning.h"

void S124Warning::computeCentroid()
{
    double sumLat = 0, sumLon = 0;
    int count = 0;
    for (const auto &g : geometries)
        for (const auto &c : g.coords) {
            sumLat += c.first;
            sumLon += c.second;
            ++count;
        }
    if (count > 0) { centroidLat = sumLat / count; centroidLon = sumLon / count; }
}

wxString S124Warning::warningTypeLabel() const
{
    switch (warningType) {
        case 1:  return _("Local");
        case 2:  return _("Coastal");
        case 3:  return _("Sub-area");
        case 4:  return _("NAVAREA");
        case 5:  return _("No Warning");
        case 6:  return _("Local – In-Force Bulletin");
        case 7:  return _("Coastal – In-Force Bulletin");
        case 8:  return _("Sub-area – In-Force Bulletin");
        case 9:  return _("NAVAREA – In-Force Bulletin");
        case 10: return _("SAR (HYDROLANT/HYDROPAC)");
        case 11: return _("Piracy Warning");
        case 12: return _("Tropical Storm");
        default: return warningType > 0
                     ? wxString::Format(_("Type %d"), warningType)
                     : _("Unknown");
    }
}
