#include "SecomClient.h"
#include "S124Parser.h"
#include <wx/base64.h>
#include <wx/log.h>

#ifdef HAVE_CURL
#  include <curl/curl.h>

static size_t CurlWrite(void *ptr, size_t size, size_t nmemb, std::string *out)
{
    out->append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

bool SecomClient::GetRaw(const wxString &url, wxString &body, wxString &errorMsg)
{
    CURL *curl = curl_easy_init();
    if (!curl) { errorMsg = _("Failed to initialize HTTP client (curl)."); return false; }

    std::string response;
    std::string urlStr = url.ToStdString();

    curl_easy_setopt(curl, CURLOPT_URL,           urlStr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       (long)m_cfg.timeoutSec);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,     "xmlpoints_pi/1.0 (S-124)");

    if (!m_cfg.sslVerify) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    if (!m_cfg.caBundle.IsEmpty())
        curl_easy_setopt(curl, CURLOPT_CAINFO,
                         m_cfg.caBundle.ToStdString().c_str());
    if (!m_cfg.certFile.IsEmpty())
        curl_easy_setopt(curl, CURLOPT_SSLCERT,
                         m_cfg.certFile.ToStdString().c_str());
    if (!m_cfg.keyFile.IsEmpty())
        curl_easy_setopt(curl, CURLOPT_SSLKEY,
                         m_cfg.keyFile.ToStdString().c_str());

    struct curl_slist *headers = nullptr;
    std::string authHeader;
    if (!m_cfg.apiKey.IsEmpty()) {
        authHeader = "Authorization: Bearer " + m_cfg.apiKey.ToStdString();
        headers = curl_slist_append(headers, authHeader.c_str());
    }
    headers = curl_slist_append(headers,
        "Accept: application/gml+xml, application/xml, application/json, */*");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        errorMsg = wxString::Format(_("HTTP request failed: %s"),
                                    curl_easy_strerror(res));
        return false;
    }
    if (httpCode >= 400) {
        errorMsg = wxString::Format(
            _("SECOM endpoint returned HTTP %ld."), httpCode);
        return false;
    }

    body = wxString::FromUTF8(response.c_str(), response.size());
    return true;
}

#else // !HAVE_CURL

bool SecomClient::GetRaw(const wxString & /*url*/, wxString & /*body*/,
                          wxString &errorMsg)
{
    errorMsg = _("SECOM support requires libcurl. "
                 "Rebuild the plugin with HAVE_CURL defined.");
    return false;
}

#endif // HAVE_CURL

// Decode the HTTP body.  Accepts:
//   • Direct GML/XML strings (starts with '<')
//   • SECOM JSON envelope: {"data":"<base64-gml>", ...}
bool SecomClient::DecodeResponse(const wxString &body, wxString &gml)
{
    wxString trimmed = body; trimmed.Trim(false);
    if (trimmed.StartsWith(wxT("<"))) { gml = body; return true; }

    // Look for "data" or "exchangeData" field in JSON
    for (const wxString &key : {wxString("\"data\""), wxString("\"exchangeData\"")}) {
        int pos = body.Find(key);
        if (pos == wxNOT_FOUND) continue;

        int colon = body.find(':', pos);
        if (colon == wxNOT_FOUND) continue;
        int q1 = body.find('"', colon + 1);
        if (q1 == wxNOT_FOUND) continue;
        // Find closing quote, skipping escaped quotes
        size_t q2 = q1 + 1;
        while (q2 < body.size()) {
            if (body[q2] == '"' && body[q2 - 1] != '\\') break;
            ++q2;
        }
        if (q2 >= body.size()) continue;

        wxString b64 = body.Mid(q1 + 1, q2 - q1 - 1);
        if (b64.IsEmpty()) continue;

        wxMemoryBuffer decoded = wxBase64Decode(b64);
        gml = wxString::FromUTF8(
            static_cast<const char*>(decoded.GetData()), decoded.GetDataLen());
        if (!gml.IsEmpty()) return true;
    }
    return false;
}

bool SecomClient::Fetch(std::vector<S124Warning> &out, wxString &errorMsg)
{
    wxString url = m_cfg.baseUrl;
    if (!m_cfg.dataReference.IsEmpty()) {
        if (!url.EndsWith(wxT("/"))) url += wxT("/");
        url += m_cfg.dataReference;
    }

    wxString body;
    if (!GetRaw(url, body, errorMsg)) return false;

    wxString gml;
    if (!DecodeResponse(body, gml)) {
        errorMsg = _("Could not interpret SECOM response.\n"
                     "Expected S-124 GML or a JSON envelope with a base64 'data' field.");
        return false;
    }

    return S124Parser::ParseString(gml, out, errorMsg);
}
