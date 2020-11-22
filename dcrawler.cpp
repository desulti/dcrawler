#include <iostream>
#include <string>
#include <typeinfo>
#include <cstdarg>
#include <fstream>
#include <algorithm>
#include <regex>
#include <experimental/filesystem>
#include <queue>

#define _WIN32_WINNT 0x0600
#include <sys/types.h>
#include <windows.h>
#include <wininet.h>

#include <errno.h>
#include <fcntl.h>
#include <time.h>

using namespace std;
namespace fs = std::experimental::filesystem;

const DWORD DELAY = 1000;
const int MAXRECV = 140*1024;
const std::wstring WRITE_DIR_PATH = L"result";

class WinHTTP {
    private:
    std::string siteUsername, sitePassword;
    std::wstring UA, host;
    bool bResult, bSent = false;
    HINTERNET hSession, hConnect, hRequest = NULL;
    std::string res = "";
    bool bReceived = false;

    public:
    WinHTTP (std::string myuser, std::string mypass) {
        siteUsername = myuser;
        sitePassword = mypass;
    }

    void httpConnect(std::wstring userAgent, std::wstring myhost, std::wstring mypath, int isHTTPS, std::wstring method) {
        UA = userAgent;
        host = myhost;

        std::wstring acceptTypes = L"text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.81";

        INTERNET_PORT portToUse;
        if (isHTTPS == 1) {
            portToUse = INTERNET_DEFAULT_HTTPS_PORT;
        }
        else {
            portToUse = INTERNET_DEFAULT_HTTP_PORT;
        }

        //initialize http and return session handle
        hSession = InternetOpenW(UA.c_str(), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);

        //make the connection request and open request
        if (hSession) {
            hConnect = InternetConnectW(hSession, host.c_str(), portToUse, L"", L"", INTERNET_SERVICE_HTTP, 0, 0);
            if (hConnect) {
                hRequest = HttpOpenRequestW(hConnect, method.c_str(), mypath.c_str(), NULL, NULL, NULL, INTERNET_FLAG_SECURE, 0);
                if (!hRequest) {
                    cout << "HTTPOpenRequest error" << GetLastError() << endl;
                    closeHandles();
                }
            }
            else {
                cout << "HTTPConnect error: " << GetLastError() << endl;
                closeHandles();
            }
        }
        else {
            cout << "HTTPOpen error: " << GetLastError() << endl;
            closeHandles();
        }
    }

    void httpAddHeader(std::wstring myheader) {
        if (hRequest) {
            bResult = HttpAddRequestHeadersW(hRequest, myheader.c_str(), -1, HTTP_ADDREQ_FLAG_ADD);
        }
        else {
            cout << "Invalid Request Handler" << endl;
        }
    }

    bool httpSend() {
        if (hRequest) {
            bSent = HttpSendRequest(hRequest, NULL, 0, 0, 0);
        }
        else {
            cout << "Invalid Request Handler" << endl;
            return false;
        }

        if (!bSent) {
            cout << "HttpSendRequest error : " << GetLastError() << endl;

            InternetErrorDlg(
                GetDesktopWindow(),
                hRequest,
                ERROR_INTERNET_CLIENT_AUTH_CERT_NEEDED,
                FLAGS_ERROR_UI_FILTER_FOR_ERRORS |
                FLAGS_ERROR_UI_FLAGS_GENERATE_DATA |
                FLAGS_ERROR_UI_FLAGS_CHANGE_OPTIONS,
                NULL
            );
            return false;
        }
        else {
            return true;
        }
    }

    bool httpReceive() {
        if (bSent) {
            char buf[MAXRECV];

            while (true) {
                memset(buf, 0 , MAXRECV);
                DWORD dwBytesRead;
                BOOL bRead;

                bRead = InternetReadFile(hRequest, buf, MAXRECV, &dwBytesRead);

                if (dwBytesRead == 0) break;

                if (!bRead) {
                    cout << "InternetReadFile error : " << GetLastError() << endl;
                }
                else {
                    buf[dwBytesRead] = 0;
                    res.append(buf);
                }
            }

            bReceived = true;
            return true;
        }
        
        return false;
    }

    std::string getResponse() {
        if (bReceived) {
            return res;
        }
        else {
            cout << "Nothing received yet" << endl;
            return "";
        }
    }

    void closeHandles() {
        if (hRequest) InternetCloseHandle(hRequest);
        if (hConnect) InternetCloseHandle(hConnect);
        if (hSession) InternetCloseHandle(hSession);
    }
};

class WebPage {
    private:
    std::wstring hostname;
    std::wstring page;
    std::string res;
    std::string content;
    bool bHasHtml;

    public:
    WebPage() {
        hostname = L"";
        page = L"";
        res = "";
        content = "";
        bHasHtml = false;
    }

    std::string getHost() {
        fs::path hostPath = hostname;
        return hostPath.string();
    }

    std::string getPage() {
        fs::path pagePath = page;
        return pagePath.string();
    }

    bool hasHtml() {
        return bHasHtml;
    }

    std::string getHtml() {
        WinHTTP http("user", "password");

        http.httpConnect(L"Mozilla/5.0", hostname, page, 1, L"GET");

        //HTTP/1.1 defines the "close" connection option for
        //the sender to signal that the connection will be closed
        //after completion of the response
        WCHAR szHeader[MAXRECV] = { 0 };

        lstrcpyW(szHeader, L"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.81\r\n");
        lstrcpyW(szHeader, L"User-Agent: Mozilla/5.0\r\n");
        lstrcpyW(szHeader, L"Connection: close\r\n");
        lstrcpyW(szHeader, L"\r\n");

        http.httpAddHeader(std::wstring(szHeader));

        http.httpSend();
        //End of building the request//

        wcout << "Request: " << hostname << page << endl;
        cout << "=========================" << endl;

        bHasHtml = http.httpReceive();
        res = http.getResponse();
        cout << "Response: ";
        res.length() > 10 ? cout << res.substr(0, 10) << "..." << endl : cout << res << endl;
        cout << "----------------------------" << endl;

        return res;
    }

    bool isContainTagAttValue(std::string tag, std::string att, std::string val) {
        if (res.length() == 0) {
            return false;
        }
        std::string regexptt = "<\\s*";
        regexptt.append(tag);
        //skip characters with [^>]*
        regexptt.append("[^>]*");
        regexptt.append(att);
        regexptt.append("\\s*=\\s*(");
        //case "val"
        regexptt.append("\"");
        regexptt.append(val);
        regexptt.append("\"|");
        //case 'val'
        regexptt.append("'");
        regexptt.append(val);
        regexptt.append("')[^>]*>");
        const regex re(regexptt);

        return regex_search(res, re);
    }

    std::string getContent() {
        if (res.length() == 0) {
            return "";
        }
        std::string regexptt = "<\\s*p[^>]*>(.*?)<\\s*/\\s*p>";
        const regex re(regexptt);
        sregex_token_iterator i(res.begin(), res.end(), re, 1);
        sregex_token_iterator j;
        while (i != j) {
            content.append(i->str());
            content.append("\n");
            i++;
        }

        return content;
    }

    std::string parseHttp(const std::string str) {
        const regex re("(.*)https://([^/]*)/?(.*)");
        smatch what;
        if (regex_match(str, what, re)) {
            std::string hst = what[2];
            for_each(hst.begin(),hst.end(),[](char& c){c=tolower(c);});
            return hst;
        } //End of the if - else//
        return "";
    } //End of method//

    void parseHref(const std::string orig_host, const std::string str) {
        const regex re("(.*)https://([^/]*)(/.*)");
        smatch what;
        if (regex_match(str, what, re)) {
            //We found a full URL, parse out the 'hostname'
            //Then parse out the page
            fs::path hostPath = string(what[2]);
            hostname = hostPath.wstring();
            for_each(hostname.begin(),hostname.end(),[](wchar_t& c){c=towlower(c);});

            fs::path pagePath = string(what[3]);
            page = pagePath.wstring();
        } else {
            //We could not find the 'page' but we can build the hostname
            fs::path hostPath = orig_host;
            hostname = hostPath.wstring();
            page = L"";
        }
    } //End of method//

    void parse(const std::string orig_host, const std::string hrf) {
        const std::string hst = parseHttp(hrf);
        if(!hst.empty()) {
            //If we have a HTTP prefix
            //We could end up with a 'hostname' and page
            parseHref(hst, hrf);
        } else {
            fs::path hostPath = orig_host;
            hostname = hostPath.wstring();
            fs::path hrfpath = hrf;
            page = hrfpath.wstring();
        }
        //hostname and page are constructed,
        //perform post analysis
        if (page.length() == 0) {
            page = L"/";
        } //End of the if//
    } //End of the method
}; //End of the class

class DCrawler {
    private:
    std::queue<std::string> urlQueue;
    std::wstring writingDir;

    std::string clean_href(const std::string host, const std::string path) {
        //Clean the href to save to file//
        std::string full_url = host;
        full_url.append(path);
        const regex rmv_all("[^a-zA-Z0-9]");
        const std::string s2 = regex_replace(full_url, rmv_all, "_");
        cout << "clean_href : " << s2 << endl;
        return s2;
    } //End of the function

    public:
    std::string getNextUrl() {
        if (!urlQueue.empty()) {
            std::string nextUrl = urlQueue.front();
            urlQueue.pop();

            return nextUrl;
        }
        return "";
    }

    void extractAllLinks(const std::string html) {
        const regex re("<\\s*a[^>]*href\\s*=\\s*(?:(\"([^\"]*)\")|('([^']*)'))[^>]*>");
        const int subs[] = { 2, 4};
        sregex_token_iterator i(html.begin(), html.end(), re, subs);
        sregex_token_iterator j;
        while(i != j) {
            const std::string href = *i;
            if (href.length() != 0) {
                urlQueue.push(i->str());
            }
            i++;
        }
    }

    void setWritingDir(const std::wstring dir) {
        writingDir = dir;
        fs::path WritingPath = writingDir;
        if (!fs::exists(WritingPath)) {
            fs::create_directory(WritingPath);
        }
    }

    void writeResult(std::string host, std::string path, std::string result) {
        fs::path WritingPath = writingDir;
        WritingPath.append("\\");
        WritingPath.append(clean_href(host, path));
        WritingPath.replace_extension(".txt");
        cout << "Writing to file : " << WritingPath << endl;
        ofstream outfile(WritingPath.string());
        outfile << result << endl;

        outfile.close();
    }
};

class DStringArray {
    private:
    std::queue<std::string> stringArray;

    public:
    void append(std::string inString) {
        stringArray.push(inString);
    }

    std::string pop() {
        std::string outString = stringArray.front();
        stringArray.pop();

        return outString;
    }

    bool isContain(std::string stringToCheck) {
        std::queue<std::string> tempArray = stringArray;
        while(!tempArray.empty()) {
            if (tempArray.front() == stringToCheck) {
                return true;
            }
            tempArray.pop();
        }

        return false;
    }

    int getSize() {
        return stringArray.size();
    }
};

class HTMLString {
    private:
    std::string html;

    public:
    HTMLString (std::string myhtml) {
        html = myhtml;
    }

    void removeTagAndContents(std::string tag) {
        //regex ptt: <(white-space?)tag...>content<(white-space?)/(white-space?)tag>
        std::string regexptt = "<\\s*";
        regexptt.append(tag);
        regexptt.append("[^>]*>.*?<\\s*/\\s*");
        regexptt.append(tag);
        regexptt.append(">");
        const regex rmv_all(regexptt);
        const std::string result = regex_replace(html, rmv_all, "");
        html = result;
    }

    void removeAllHtmlTags() {
        //regex ptt: <...>
        const regex rmv_all("<[^>]*>");
        const std::string result = regex_replace(html, rmv_all, "");
        html = result;
    }

    bool isNotBlank() {
        const regex re("\\S");

        return regex_search(html, re);
    }

    std::string getFilteredHtml() {
        return html;
    }
};

int main() {
    cout << "Lauching program" << endl;
    DCrawler crawler;
    std::string targetHost = "vnexpress.net";
    DStringArray seenPaths;
    std::string nextUrl ="/";
    int count = 0;
    crawler.setWritingDir(WRITE_DIR_PATH);
    while (count < 100) {
        WebPage page;
        page.parse(targetHost, nextUrl);
        if ((page.getHost() != targetHost) || (seenPaths.isContain(page.getPage()))
        || (page.getPage().find("#box_comment") != std::string::npos)) {
            nextUrl = crawler.getNextUrl();
            continue;
        }
        std::string pageHtml = page.getHtml();
        seenPaths.append(page.getPage());
        if (page.hasHtml()) {
            if (page.isContainTagAttValue("a", "data-medium", "Menu-BongDa")) {
                HTMLString content(page.getContent());
                content.removeTagAndContents("span");
                content.removeAllHtmlTags();
                if (content.isNotBlank()) {
                    crawler.writeResult(page.getHost(), page.getPage(), content.getFilteredHtml());
                    count++;
                }
            }
            crawler.extractAllLinks(pageHtml);
        }
        nextUrl = crawler.getNextUrl();
        Sleep(DELAY);
    }
    cout << "Done" << endl;
    return 0;
} // End of the function//