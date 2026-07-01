#include "SecomClient.h"
#include "S124Parser.h"
#include <wx/base64.h>
#include <wx/log.h>
#include <wx/mstream.h>
#include <wx/zstream.h>
#include <wx/zipstrm.h>

#ifdef HAVE_CURL
#  include <curl/curl.h>

static size_t CurlWrite(void *ptr, size_t size, size_t nmemb, std::string *out)
{
    out->append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

// ── lightweight JSON helpers (no external library) ────────────────────────────

// Content between the first [ ] for the given key, without brackets.
static wxString JArray(const wxString &json, const wxString &key)
{
    wxString needle = wxT("\"") + key + wxT("\"");
    size_t kp = json.find(needle);
    if (kp == wxString::npos) return wxEmptyString;
    size_t br = json.find('[', kp + needle.size());
    if (br == wxString::npos) return wxEmptyString;
    int depth = 0;
    for (size_t i = br; i < json.size(); ++i) {
        if      (json[i] == '[') ++depth;
        else if (json[i] == ']' && !--depth)
            return json.Mid(br + 1, i - br - 1);
    }
    return wxEmptyString;
}

// Split array body string into top-level { } objects.
static std::vector<wxString> JObjects(const wxString &arr)
{
    std::vector<wxString> v;
    int depth = 0;
    size_t start = wxString::npos;
    for (size_t i = 0; i < arr.size(); ++i) {
        if (arr[i] == '{') { if (!depth++) start = i; }
        else if (arr[i] == '}' && !--depth && start != wxString::npos) {
            v.push_back(arr.Mid(start, i - start + 1));
            start = wxString::npos;
        }
    }
    return v;
}

// Value of a string field (first match).
static wxString JString(const wxString &json, const wxString &key)
{
    wxString needle = wxT("\"") + key + wxT("\"");
    size_t kp = json.find(needle);
    if (kp == wxString::npos) return wxEmptyString;
    size_t col = json.find(':', kp + needle.size());
    if (col == wxString::npos) return wxEmptyString;
    size_t q1 = json.find('"', col + 1);
    if (q1 == wxString::npos) return wxEmptyString;
    size_t q2 = q1 + 1;
    while (q2 < json.size() && !(json[q2] == '"' && json[q2 - 1] != '\\')) ++q2;
    if (q2 >= json.size()) return wxEmptyString;
    return json.Mid(q1 + 1, q2 - q1 - 1);
}

// Value of an integer field, or -1 if absent.
static int JInt(const wxString &json, const wxString &key)
{
    wxString needle = wxT("\"") + key + wxT("\"");
    size_t kp = json.find(needle);
    if (kp == wxString::npos) return -1;
    size_t col = json.find(':', kp + needle.size());
    if (col == wxString::npos) return -1;
    size_t i = col + 1;
    while (i < json.size() &&
           (json[i] == ' ' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r'))
        ++i;
    size_t j = i;
    while (j < json.size() && json[j] >= '0' && json[j] <= '9') ++j;
    if (j == i) return -1;
    long v = 0;
    json.Mid(i, j - i).ToLong(&v);
    return (int)v;
}

// ── curl helpers ──────────────────────────────────────────────────────────────

static void CommonCurlOpts(CURL *curl, const SecomConfig &cfg, std::string *buf)
{
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  CurlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        (long)cfg.timeoutSec);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,  1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "xmlpoints_pi/1.0 (S-124)");
    if (!cfg.sslVerify) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    if (!cfg.caBundle.IsEmpty())
        curl_easy_setopt(curl, CURLOPT_CAINFO,
                         cfg.caBundle.ToStdString().c_str());
    if (!cfg.certFile.IsEmpty())
        curl_easy_setopt(curl, CURLOPT_SSLCERT,
                         cfg.certFile.ToStdString().c_str());
    if (!cfg.keyFile.IsEmpty())
        curl_easy_setopt(curl, CURLOPT_SSLKEY,
                         cfg.keyFile.ToStdString().c_str());
}

static struct curl_slist *BuildHeaders(const SecomConfig &cfg,
                                        const char *contentType,
                                        const char *accept)
{
    struct curl_slist *h = nullptr;
    if (!cfg.apiKey.IsEmpty()) {
        std::string auth = "Authorization: Bearer " + cfg.apiKey.ToStdString();
        h = curl_slist_append(h, auth.c_str());
    }
    if (contentType) h = curl_slist_append(h, contentType);
    if (accept)      h = curl_slist_append(h, accept);
    return h;
}

static bool RunCurl(CURL *curl, struct curl_slist *headers,
                     std::string &raw, wxString &body, wxString &err)
{
    CURLcode res  = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        err = wxString::Format(_("HTTP error: %s"), curl_easy_strerror(res));
        return false;
    }
    if (httpCode >= 400) {
        err = wxString::Format(_("Server returned HTTP %ld."), httpCode);
        return false;
    }
    body = wxString::FromUTF8(raw.c_str(), raw.size());
    return true;
}

bool SecomClient::HttpGet(const wxString &url, wxString &body, wxString &err)
{
    CURL *curl = curl_easy_init();
    if (!curl) { err = _("Failed to init HTTP client."); return false; }

    std::string raw;
    curl_easy_setopt(curl, CURLOPT_URL, url.ToStdString().c_str());
    CommonCurlOpts(curl, m_cfg, &raw);
    struct curl_slist *h = BuildHeaders(m_cfg, nullptr,
        "Accept: application/gml+xml, application/xml, application/json, */*");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h);
    return RunCurl(curl, h, raw, body, err);
}

bool SecomClient::HttpPost(const wxString &url, const wxString &payload,
                            wxString &body, wxString &err)
{
    CURL *curl = curl_easy_init();
    if (!curl) { err = _("Failed to init HTTP client."); return false; }

    std::string raw;
    std::string post = payload.ToStdString();
    curl_easy_setopt(curl, CURLOPT_URL,           url.ToStdString().c_str());
    curl_easy_setopt(curl, CURLOPT_POST,           1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     post.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,  (long)post.size());
    CommonCurlOpts(curl, m_cfg, &raw);
    struct curl_slist *h = BuildHeaders(m_cfg,
        "Content-Type: application/json",
        "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, h);
    return RunCurl(curl, h, raw, body, err);
}

#else  // !HAVE_CURL

bool SecomClient::HttpGet(const wxString &, wxString &, wxString &err)
{
    err = _("SECOM requires libcurl. Rebuild the plugin with HAVE_CURL defined.");
    return false;
}
bool SecomClient::HttpPost(const wxString &, const wxString &, wxString &, wxString &err)
{
    err = _("SECOM requires libcurl. Rebuild the plugin with HAVE_CURL defined.");
    return false;
}

#endif  // HAVE_CURL

// ── response decoders ─────────────────────────────────────────────────────────

// Convert raw bytes to a GML string; handles ZIP (S-100 Exchange Set), gzip, or plain GML.
static bool BytesToGml(const void *data, size_t n, wxString &gml, wxString &err)
{
    const unsigned char *p = static_cast<const unsigned char*>(data);

    // ZIP archive (S-100 Exchange Set): PK\x03\x04 magic
    if (n >= 4 && p[0] == 'P' && p[1] == 'K' && p[2] == 0x03 && p[3] == 0x04) {
        wxMemoryInputStream mis(data, n);
        wxZipInputStream   zis(mis);
        wxZipEntry *entry;
        while ((entry = zis.GetNextEntry()) != nullptr) {
            wxString name = entry->GetName();
            delete entry;
            if (name.Lower().EndsWith(wxT(".gml"))) {
                std::string content;
                char tmp[4096];
                while (!zis.Eof()) {
                    zis.Read(tmp, sizeof(tmp));
                    size_t r = zis.LastRead();
                    if (!r) break;
                    content.append(tmp, r);
                }
                gml = wxString::FromUTF8(content.c_str(), content.size());
                if (!gml.IsEmpty()) return true;
            }
        }
        err = _("No GML file found in ZIP archive from SECOM response.");
        return false;
    }

    // gzip-compressed GML: \x1f\x8b magic
    if (n >= 2 && p[0] == 0x1f && p[1] == 0x8b) {
        wxMemoryInputStream mis(data, n);
        wxZlibInputStream  zis(mis, wxZLIB_GZIP);
        std::string decompressed;
        char tmp[4096];
        while (zis.CanRead()) {
            zis.Read(tmp, sizeof(tmp));
            size_t r = zis.LastRead();
            if (!r) break;
            decompressed.append(tmp, r);
        }
        gml = wxString::FromUTF8(decompressed.c_str(), decompressed.size());
        if (gml.IsEmpty()) {
            err = _("GML decompression produced no output (invalid gzip or encoding).");
            return false;
        }
        return true;
    }

    // Plain GML/XML bytes
    gml = wxString::FromUTF8(static_cast<const char*>(data), n);
    if (gml.IsEmpty()) {
        err = _("Empty GML after base64 decode.");
        return false;
    }
    return true;
}

bool SecomClient::DecodeItem(const wxString &b64OrGml,
                              std::vector<S124Warning> &out, wxString &err)
{
    wxString gml;
    wxString trimmed = b64OrGml;
    trimmed.Trim(false);
    if (trimmed.StartsWith(wxT("<"))) {
        gml = b64OrGml;
    } else {
        wxMemoryBuffer buf = wxBase64Decode(b64OrGml);
        if (!BytesToGml(buf.GetData(), buf.GetDataLen(), gml, err))
            return false;
    }
    return S124Parser::ParseString(gml, out, err);
}

bool SecomClient::ParseSearchPage(const wxString &json,
                                   std::vector<S124Warning> &out,
                                   int page, int &remaining, wxString &err)
{
    // Known array field names across SECOM implementations:
    //   dataResponseObject – PING portal /v1/object endpoint
    //   responseObject     – standard SECOM /v1/searchService
    //   returnedObject / datasets – other implementations
    wxString arrContent;
    for (const wxString &k : { wxString("dataResponseObject"),
                                 wxString("responseObject"),
                                 wxString("returnedObject"),
                                 wxString("datasets") }) {
        arrContent = JArray(json, k);
        if (!arrContent.IsEmpty()) break;
    }

    if (arrContent.IsEmpty()) {
        // Zero-result responses may omit the array entirely.
        if (JInt(json, "totalItems") == 0 || JInt(json, "total") == 0) {
            remaining = 0; return true;
        }
        err = _("Unrecognised SECOM response: no data array found "
                "(expected 'dataResponseObject', 'responseObject', etc.).");
        return false;
    }

    std::vector<wxString> objs = JObjects(arrContent);
    int successCount = 0;
    wxString lastItemErr;

    for (const wxString &obj : objs) {
        wxString b64;
        for (const wxString &dk : { wxString("data"), wxString("exchangeData") }) {
            b64 = JString(obj, dk);
            if (!b64.IsEmpty()) break;
        }
        if (b64.IsEmpty()) continue;

        wxString itemErr;
        std::vector<S124Warning> itemOut;
        if (!DecodeItem(b64, itemOut, itemErr)) {
            wxLogWarning("SECOM: skipping item – %s", itemErr);
            lastItemErr = itemErr;
        } else {
            out.insert(out.end(), itemOut.begin(), itemOut.end());
            ++successCount;
        }
    }

    if (!objs.empty() && successCount == 0 && !lastItemErr.IsEmpty()) {
        err = lastItemErr;
        return false;
    }

    // Compute remaining items to fetch.
    // Standard SECOM: "remainingObject" integer at root level.
    remaining = JInt(json, "remainingObject");
    if (remaining < 0) {
        // PING portal pagination: {"pagination":{"totalItems":N,"maxItemsPerPage":M}}
        int totalItems  = JInt(json, "totalItems");
        int maxPerPage  = JInt(json, "maxItemsPerPage");
        if (totalItems >= 0 && maxPerPage > 0)
            remaining = std::max(0, totalItems - page * maxPerPage);
        else
            remaining = 0;
    }
    return true;
}

static bool IsHtml(const wxString &body)
{
    wxString t = body.Left(30).Lower();
    return t.StartsWith(wxT("<!doctype")) || t.StartsWith(wxT("<html"));
}

// Decode a single-object response: raw GML or a SECOM JSON envelope.
bool SecomClient::DecodeResponse(const wxString &body, wxString &gml)
{
    wxString trimmed = body;
    trimmed.Trim(false);
    // Accept XML but never HTML (HTML also starts with '<')
    if (trimmed.StartsWith(wxT("<")) && !IsHtml(trimmed)) { gml = body; return true; }

    for (const wxString &key : { wxString("data"), wxString("exchangeData") }) {
        wxString b64 = JString(body, key);
        if (b64.IsEmpty()) continue;
        wxMemoryBuffer buf = wxBase64Decode(b64);
        wxString dummy;
        if (BytesToGml(buf.GetData(), buf.GetDataLen(), gml, dummy) && !gml.IsEmpty())
            return true;
    }
    return false;
}

bool SecomClient::FetchViaDirect(std::vector<S124Warning> &out, wxString &err)
{
    wxString url = m_cfg.baseUrl;
    if (!m_cfg.dataReference.IsEmpty()) {
        if (!url.EndsWith(wxT("/"))) url += wxT("/");
        url += m_cfg.dataReference;
    }

    wxString body;
    if (!HttpGet(url, body, err)) return false;

    if (IsHtml(body)) {
        err = _(
            "The server returned an HTML page instead of a SECOM response.\n\n"
            "The URL you entered appears to be a web portal address, not the\n"
            "SECOM API endpoint. Check the service documentation for the\n"
            "correct API base URL (it is often different from the portal URL).");
        return false;
    }

    wxString gml;
    if (!DecodeResponse(body, gml)) {
        wxString snippet = body.Left(300).Trim(true);
        err = wxString::Format(
            _("Could not interpret the server response as S-124 GML.\n\n"
              "Server returned:\n%s%s"),
            snippet,
            body.size() > 300 ? wxT("\n[…]") : wxT(""));
        return false;
    }
    return S124Parser::ParseString(gml, out, err);
}

// ── main Fetch ────────────────────────────────────────────────────────────────

bool SecomClient::Fetch(std::vector<S124Warning> &out, wxString &errorMsg)
{
    wxString base = m_cfg.baseUrl;
    base.Trim(true);
    while (base.EndsWith(wxT("/"))) base.RemoveLast();

    // ── Strategy 1: GET /v1/object  (PING portal SECOM node) ─────────────────
    // Paginated by ?page=N&pageSize=100; response uses "dataResponseObject" array
    // and "pagination.totalItems" / "pagination.maxItemsPerPage".
    {
        wxString objectUrl = base + wxT("/v1/object");
        bool started = false;

        // PING SECOM uses 1-based page numbers; page=0 returns an empty array.
        // Server enforces pageSize <= 40; values above that cause HTTP 400.
        for (int page = 1; ; ++page) {
            wxString url = wxString::Format(
                wxT("%s?page=%d&pageSize=40"), objectUrl, page);
            wxString body, getErr;
            if (!HttpGet(url, body, getErr)) {
                if (!started) break;   // endpoint absent → try strategy 2
                break;                 // error mid-pagination: treat as done
            }
            if (IsHtml(body)) {
                errorMsg = _(
                    "The server returned an HTML page instead of a SECOM response.\n\n"
                    "The URL you entered appears to be a web portal address, not the\n"
                    "SECOM API endpoint. Check the documentation for the correct\n"
                    "API base URL (e.g. https://services.ping-info-nautique.fr/secom/avurnav_local_brest_fr).");
                return false;
            }
            wxString trimmed = body; trimmed.Trim(false).Trim(true);
            if (trimmed.IsSameAs(wxT("PING"), false)) break;  // health-check only

            int remaining = 0;
            wxString pageErr;
            if (!ParseSearchPage(body, out, page, remaining, pageErr)) {
                if (!started) break;   // page 0 unrecognised → try strategy 2
                errorMsg = pageErr;
                return false;
            }
            started = true;
            if (remaining <= 0) break;
        }

        if (started) return true;
    }

    // ── Strategy 2: POST /v1/searchService  (standard SECOM node) ────────────
    // Offset-based; response uses "responseObject" array and "remainingObject".
    {
        wxString searchUrl = base + wxT("/v1/searchService");
        int offset = 0;

        for (;;) {
            wxString payload = wxString::Format(
                wxT("{\"geometry\":null,\"startDate\":null,\"endDate\":null,"
                    "\"offset\":%d,\"limit\":100}"), offset);
            wxString body, postErr;
            if (!HttpPost(searchUrl, payload, body, postErr)) {
                // 405 = endpoint exists but no POST → fall back to direct GET
                if (postErr.Contains(wxT("405")))
                    return FetchViaDirect(out, errorMsg);
                errorMsg = postErr;
                return false;
            }
            if (IsHtml(body)) {
                errorMsg = _(
                    "The server returned an HTML page instead of a SECOM response.\n\n"
                    "The URL you entered appears to be a web portal address, not the\n"
                    "SECOM API endpoint. Check the documentation for the correct\n"
                    "API base URL (e.g. https://services.ping-info-nautique.fr/secom/avurnav_local_brest_fr).");
                return false;
            }
            wxString trimmed = body; trimmed.Trim(false).Trim(true);
            if (trimmed.IsSameAs(wxT("PING"), false))
                return FetchViaDirect(out, errorMsg);

            int remaining = 0;
            if (!ParseSearchPage(body, out, 0, remaining, errorMsg))
                return false;
            if (remaining <= 0) break;
            offset += 100;
        }
        return true;
    }
}
