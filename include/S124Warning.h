#ifndef S124_WARNING_H
#define S124_WARNING_H

#include <wx/wx.h>
#include <vector>
#include <utility>

struct S124Geometry {
    enum Type { POINT, CURVE, SURFACE };
    Type type = POINT;
    std::vector<std::pair<double,double>> coords; // (lat, lon)
};

struct S124Warning {
    wxString id;
    int      warningNumber     = 0;
    int      year              = 0;
    wxString seriesName;
    int      warningType       = 0;
    int      warningTypeDetail = 0;
    wxString publicationTime;
    wxString cancellationDate;
    wxString headerText;
    wxString language;
    std::vector<S124Geometry> geometries;

    double centroidLat = 0.0;
    double centroidLon = 0.0;

    void computeCentroid();
    bool hasCancellation() const { return !cancellationDate.IsEmpty(); }
    wxString warningTypeLabel() const;
};

#endif // S124_WARNING_H
