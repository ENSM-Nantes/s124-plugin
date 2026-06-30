#ifndef SECOM_CLIENT_H
#define SECOM_CLIENT_H

#include <wx/wx.h>
#include <vector>
#include "S124Warning.h"

struct SecomConfig {
    wxString baseUrl;
    wxString dataReference;  // optional MRN/UUID appended to URL
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

    // Fetch S-124 warnings from the configured endpoint.
    // Handles both direct GML responses and SECOM JSON envelopes (base64 data field).
    bool Fetch(std::vector<S124Warning> &out, wxString &errorMsg);

private:
    bool GetRaw(const wxString &url, wxString &body, wxString &errorMsg);
    bool DecodeResponse(const wxString &body, wxString &gml);

    SecomConfig m_cfg;
};

#endif // SECOM_CLIENT_H
