#ifndef SECOM_CLIENT_H
#define SECOM_CLIENT_H

#include <wx/wx.h>
#include <vector>
#include "S124Warning.h"

struct SecomConfig {
    wxString name;           // display label to tell connections apart
    wxString baseUrl;
    wxString dataReference;  // appended to URL for direct GET fallback
    wxString apiKey;         // Bearer token (Authorization header)
    wxString certFile;       // path to client cert PEM (optional)
    wxString keyFile;        // path to client key PEM (optional)
    wxString caBundle;       // path to CA bundle (optional)
    bool     sslVerify = true;
    int      timeoutSec = 30;
};

class SecomClient
{
public:
    explicit SecomClient(const SecomConfig &cfg) : m_cfg(cfg) {}

    // Fetch S-124 warnings. Tries SECOM searchService POST with pagination
    // first; falls back to direct GET if the server does not support POST.
    bool Fetch(std::vector<S124Warning> &out, wxString &errorMsg);

private:
    bool HttpGet(const wxString &url, wxString &body, wxString &err);
    bool HttpPost(const wxString &url, const wxString &payload,
                  wxString &body, wxString &err);

    // Parse one page of a SECOM JSON response (searchService or object endpoint).
    // 'page' is the 0-based page index, used to compute remaining items from
    // PING-style pagination (totalItems / maxItemsPerPage).
    // Sets 'remaining' to the number of items still to fetch after this page.
    bool ParseSearchPage(const wxString &json,
                         std::vector<S124Warning> &out,
                         int page, int &remaining, wxString &err);

    // Decode a base64 GML blob (or raw GML) and parse it into warnings.
    bool DecodeItem(const wxString &b64OrGml,
                    std::vector<S124Warning> &out, wxString &err);

    // Fallback: GET baseUrl[/dataReference] and decode single-object response.
    bool FetchViaDirect(std::vector<S124Warning> &out, wxString &err);

    // Decode a response that is either raw GML or a single SECOM JSON envelope.
    bool DecodeResponse(const wxString &body, wxString &gml);

    SecomConfig m_cfg;
};

#endif // SECOM_CLIENT_H
