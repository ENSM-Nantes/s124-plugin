#include "PointsLayer.h"
#include "S124Parser.h"
#include <cmath>
#ifdef __APPLE__
#  include <OpenGL/gl.h>
#else
#  include <GL/gl.h>
#endif

static const int HIT_RADIUS  = 10;
static const int DRAW_RADIUS  = 8;

// ── cached screen geometry for hit-testing ────────────────────────────────────

struct CachedEntry {
    int warnIdx;
    S124Geometry::Type geomType;
    std::vector<wxPoint> pts;
};
static std::vector<CachedEntry> s_cached;

// ── geometry helpers ──────────────────────────────────────────────────────────

static bool PointInPolygon(int px, int py, const std::vector<wxPoint> &poly)
{
    bool inside = false;
    int n = (int)poly.size();
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if (((poly[i].y > py) != (poly[j].y > py)) &&
            (px < (long)(poly[j].x - poly[i].x) * (py - poly[i].y) /
                         (poly[j].y - poly[i].y) + poly[i].x))
            inside = !inside;
    }
    return inside;
}

static long DistToSegSq(int px, int py, wxPoint a, wxPoint b)
{
    long dx = b.x - a.x, dy = b.y - a.y;
    if (dx == 0 && dy == 0) {
        long ex = px - a.x, ey = py - a.y;
        return ex * ex + ey * ey;
    }
    double t = ((double)(px - a.x) * dx + (double)(py - a.y) * dy) /
               (double)(dx * dx + dy * dy);
    if (t < 0) t = 0; if (t > 1) t = 1;
    double fx = a.x + t * dx - px;
    double fy = a.y + t * dy - py;
    return (long)(fx * fx + fy * fy);
}

// ── PointsLayer ───────────────────────────────────────────────────────────────

PointsLayer::PointsLayer()  {}
PointsLayer::~PointsLayer() {}

bool PointsLayer::LoadFromFile(const wxString &filePath)
{
    m_warnings.clear();
    bool ok = S124Parser::ParseFile(filePath, m_warnings, m_lastError);
    return ok;
}

void PointsLayer::SetWarnings(const std::vector<S124Warning> &warnings)
{
    m_warnings = warnings;
}

wxPoint PointsLayer::LatLonToScreen(double lat, double lon, PlugIn_ViewPort *vp)
{
    wxPoint p(0, 0);
    if (vp) GetCanvasPixLL(vp, &p, lat, lon);
    return p;
}

wxColour PointsLayer::WarningColor(int warningType) const
{
    switch (warningType) {
        case 4: case 9:  return wxColour(200,  30,  30);  // NAVAREA – deep red
        case 3: case 8:  return wxColour(220,  80,  20);  // Sub-area – red-orange
        case 2: case 7:  return wxColour(230, 130,   0);  // Coastal – orange
        case 1: case 6:  return wxColour(200, 180,   0);  // Local – yellow
        default:         return wxColour(220,  50,  50);  // default red
    }
}

// ── DC rendering ──────────────────────────────────────────────────────────────

void PointsLayer::Render(wxDC &dc, PlugIn_ViewPort *vp)
{
    if (m_warnings.empty()) return;
    s_cached.clear();

    for (int wi = 0; wi < (int)m_warnings.size(); ++wi) {
        const S124Warning &w = m_warnings[wi];
        wxColour col = WarningColor(w.warningType);

        for (const auto &g : w.geometries) {
            CachedEntry ce;
            ce.warnIdx  = wi;
            ce.geomType = g.type;

            for (const auto &c : g.coords)
                ce.pts.push_back(LatLonToScreen(c.first, c.second, vp));

            if (g.type == S124Geometry::POINT && !ce.pts.empty()) {
                dc.SetPen(wxPen(col, 2));
                dc.SetBrush(wxBrush(col));
                dc.DrawCircle(ce.pts[0].x, ce.pts[0].y, DRAW_RADIUS);
                // White outline
                dc.SetPen(wxPen(*wxWHITE, 1));
                dc.SetBrush(*wxTRANSPARENT_BRUSH);
                dc.DrawCircle(ce.pts[0].x, ce.pts[0].y, DRAW_RADIUS + 1);

            } else if (g.type == S124Geometry::CURVE && ce.pts.size() >= 2) {
                dc.SetPen(wxPen(col, 3));
                dc.DrawLines((int)ce.pts.size(), ce.pts.data());

            } else if (g.type == S124Geometry::SURFACE && ce.pts.size() >= 3) {
                wxColour fill(col.Red(), col.Green(), col.Blue(), 60);
                dc.SetPen(wxPen(col, 2));
                dc.SetBrush(wxBrush(fill));
                dc.DrawPolygon((int)ce.pts.size(), ce.pts.data());
            }

            if (!ce.pts.empty()) s_cached.push_back(std::move(ce));
        }

        // Centroid marker (diamond) for multi-geometry warnings so users can click
        if (w.geometries.size() > 1 ||
            (w.geometries.size() == 1 && w.geometries[0].type == S124Geometry::SURFACE)) {
            wxPoint cp = LatLonToScreen(w.centroidLat, w.centroidLon, vp);
            wxPoint diamond[4] = {
                {cp.x, cp.y - 8}, {cp.x + 8, cp.y},
                {cp.x, cp.y + 8}, {cp.x - 8, cp.y}
            };
            dc.SetPen(wxPen(col, 2));
            dc.SetBrush(wxBrush(col));
            dc.DrawPolygon(4, diamond);
            dc.SetPen(wxPen(*wxWHITE, 1));
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.DrawPolygon(4, diamond);

            CachedEntry ce;
            ce.warnIdx  = wi;
            ce.geomType = S124Geometry::POINT;
            ce.pts.push_back(cp);
            s_cached.push_back(std::move(ce));
        }
    }
}

// ── GL rendering ──────────────────────────────────────────────────────────────

void PointsLayer::RenderGL(PlugIn_ViewPort *vp)
{
    if (m_warnings.empty()) return;
    s_cached.clear();

    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT | GL_LINE_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);

    const int N = 24;

    for (int wi = 0; wi < (int)m_warnings.size(); ++wi) {
        const S124Warning &w = m_warnings[wi];
        wxColour col = WarningColor(w.warningType);

        for (const auto &g : w.geometries) {
            CachedEntry ce;
            ce.warnIdx  = wi;
            ce.geomType = g.type;

            for (const auto &c : g.coords)
                ce.pts.push_back(LatLonToScreen(c.first, c.second, vp));

            if (g.type == S124Geometry::POINT && !ce.pts.empty()) {
                float x = (float)ce.pts[0].x, y = (float)ce.pts[0].y;
                float r = (float)DRAW_RADIUS;
                glColor4ub(col.Red(), col.Green(), col.Blue(), 220);
                glBegin(GL_TRIANGLE_FAN);
                glVertex2f(x, y);
                for (int i = 0; i <= N; i++) {
                    float a = (float)(i * 2.0 * M_PI / N);
                    glVertex2f(x + r * cosf(a), y + r * sinf(a));
                }
                glEnd();
                glColor4ub(255, 255, 255, 200);
                glLineWidth(1.5f);
                glBegin(GL_LINE_LOOP);
                for (int i = 0; i < N; i++) {
                    float a = (float)(i * 2.0 * M_PI / N);
                    glVertex2f(x + (r + 1) * cosf(a), y + (r + 1) * sinf(a));
                }
                glEnd();

            } else if (g.type == S124Geometry::CURVE && ce.pts.size() >= 2) {
                glColor4ub(col.Red(), col.Green(), col.Blue(), 220);
                glLineWidth(3.0f);
                glBegin(GL_LINE_STRIP);
                for (const auto &p : ce.pts) glVertex2f((float)p.x, (float)p.y);
                glEnd();

            } else if (g.type == S124Geometry::SURFACE && ce.pts.size() >= 3) {
                // Filled polygon (semi-transparent)
                glColor4ub(col.Red(), col.Green(), col.Blue(), 50);
                glBegin(GL_POLYGON);
                for (const auto &p : ce.pts) glVertex2f((float)p.x, (float)p.y);
                glEnd();
                // Outline
                glColor4ub(col.Red(), col.Green(), col.Blue(), 220);
                glLineWidth(2.0f);
                glBegin(GL_LINE_LOOP);
                for (const auto &p : ce.pts) glVertex2f((float)p.x, (float)p.y);
                glEnd();
            }

            if (!ce.pts.empty()) s_cached.push_back(std::move(ce));
        }

        // Centroid diamond
        if (w.geometries.size() > 1 ||
            (w.geometries.size() == 1 && w.geometries[0].type == S124Geometry::SURFACE)) {
            wxPoint cp = LatLonToScreen(w.centroidLat, w.centroidLon, vp);
            float x = (float)cp.x, y = (float)cp.y, r = 8.f;
            glColor4ub(255, 255, 255, 220);
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(x, y - r); glVertex2f(x + r, y);
            glVertex2f(x, y + r); glVertex2f(x - r, y);
            glVertex2f(x, y - r);
            glEnd();
            glColor4ub(col.Red(), col.Green(), col.Blue(), 220);
            glLineWidth(2.0f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(x, y - r); glVertex2f(x + r, y);
            glVertex2f(x, y + r); glVertex2f(x - r, y);
            glEnd();

            CachedEntry ce;
            ce.warnIdx  = wi;
            ce.geomType = S124Geometry::POINT;
            ce.pts.push_back(cp);
            s_cached.push_back(std::move(ce));
        }
    }

    glPopAttrib();
}

// ── hit testing ───────────────────────────────────────────────────────────────

int PointsLayer::HitTest(int sx, int sy, PlugIn_ViewPort * /*vp*/)
{
    // Iterate in reverse so topmost-drawn features win
    for (int i = (int)s_cached.size() - 1; i >= 0; --i) {
        const CachedEntry &ce = s_cached[i];
        if (ce.geomType == S124Geometry::POINT) {
            if (ce.pts.empty()) continue;
            int dx = sx - ce.pts[0].x, dy = sy - ce.pts[0].y;
            if (dx * dx + dy * dy <= HIT_RADIUS * HIT_RADIUS)
                return ce.warnIdx;
        } else if (ce.geomType == S124Geometry::SURFACE) {
            if (PointInPolygon(sx, sy, ce.pts))
                return ce.warnIdx;
        } else if (ce.geomType == S124Geometry::CURVE) {
            for (size_t j = 1; j < ce.pts.size(); ++j) {
                if (DistToSegSq(sx, sy, ce.pts[j-1], ce.pts[j]) <=
                    (long)HIT_RADIUS * HIT_RADIUS)
                    return ce.warnIdx;
            }
        }
    }
    return -1;
}
