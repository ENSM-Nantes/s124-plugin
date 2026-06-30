#include "PointsLayer.h"
#include "XmlLoader.h"
#include <wx/dcmemory.h>
#include <cmath>
#ifdef __APPLE__
#  include <OpenGL/gl.h>
#else
#  include <GL/gl.h>
#endif

// Radius in pixels for hit-testing
static const int HIT_RADIUS = 10;
// Radius for drawing the circle marker
static const int DRAW_RADIUS = 8;

// We cache screen positions from the last render so MouseEventHook can use them
struct CachedPoint {
    int x, y;
};
static std::vector<CachedPoint> s_cached;
static PlugIn_ViewPort s_lastVP;
static bool s_hasVP = false;

PointsLayer::PointsLayer() {}
PointsLayer::~PointsLayer() {}

bool PointsLayer::LoadFromFile(const wxString &filePath)
{
    m_points.clear();
    bool ok = XmlLoader::LoadPoints(filePath, m_points, m_lastError);
    return ok;
}

wxPoint PointsLayer::LatLonToScreen(double lat, double lon, PlugIn_ViewPort *vp)
{
    wxPoint p(0, 0);
    if (vp)
        GetCanvasPixLL(vp, &p, lat, lon);
    return p;
}

void PointsLayer::Render(wxDC &dc, PlugIn_ViewPort *vp)
{
    if (m_points.empty()) return;

    s_cached.clear();
    s_hasVP = (vp != nullptr);
    if (vp) s_lastVP = *vp;

    dc.SetPen(wxPen(wxColour(220, 50, 50), 2));
    dc.SetBrush(wxBrush(wxColour(220, 50, 50, 180)));

    for (const auto &pt : m_points)
    {
        wxPoint sp = LatLonToScreen(pt.latitude, pt.longitude, vp);
        s_cached.push_back({sp.x, sp.y});

        // Draw filled circle
        dc.DrawCircle(sp.x, sp.y, DRAW_RADIUS);

        // Draw a small white border
        dc.SetPen(wxPen(*wxWHITE, 1));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawCircle(sp.x, sp.y, DRAW_RADIUS + 1);

        // Reset pens for next iteration
        dc.SetPen(wxPen(wxColour(220, 50, 50), 2));
        dc.SetBrush(wxBrush(wxColour(220, 50, 50, 180)));
    }
}

void PointsLayer::RenderGL(PlugIn_ViewPort *vp)
{
    if (m_points.empty()) return;

    s_cached.clear();
    s_hasVP = (vp != nullptr);
    if (vp) s_lastVP = *vp;

    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT | GL_LINE_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    const int N = 24;
    const float r = (float)DRAW_RADIUS;

    for (const auto &pt : m_points)
    {
        wxPoint sp = LatLonToScreen(pt.latitude, pt.longitude, vp);
        s_cached.push_back({sp.x, sp.y});

        float x = (float)sp.x;
        float y = (float)sp.y;

        // Filled red circle
        glColor4ub(220, 50, 50, 220);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, y);
        for (int i = 0; i <= N; i++) {
            float a = (float)(i * 2.0 * M_PI / N);
            glVertex2f(x + r * cosf(a), y + r * sinf(a));
        }
        glEnd();

        // White border
        glColor4ub(255, 255, 255, 200);
        glLineWidth(1.5f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < N; i++) {
            float a = (float)(i * 2.0 * M_PI / N);
            glVertex2f(x + (r + 1) * cosf(a), y + (r + 1) * sinf(a));
        }
        glEnd();
    }

    glPopAttrib();
}

int PointsLayer::HitTest(int screenX, int screenY, PlugIn_ViewPort * /*vp*/)
{
    // Use cached screen positions from last render
    for (size_t i = 0; i < s_cached.size(); ++i)
    {
        int dx = screenX - s_cached[i].x;
        int dy = screenY - s_cached[i].y;
        if ((dx * dx + dy * dy) <= (HIT_RADIUS * HIT_RADIUS))
            return static_cast<int>(i);
    }
    return -1;
}
