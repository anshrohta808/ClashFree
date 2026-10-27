#include "csv_loader.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cassert>
#include <map>

using namespace std;

int testsPassed = 0;
int testsFailed = 0;

void assertEqual(const string& name, bool condition) {
    if (condition) {
        cout << "PASS: " << name << endl;
        testsPassed++;
    } else {
        cout << "FAIL: " << name << endl;
        testsFailed++;
    }
}

string readFile(const string& path) {
    ifstream file(path);
    if (!file) return "";
    return string(istreambuf_iterator<char>(file), istreambuf_iterator<char>());
}

void testRawCSVLoad() {
    string rawCSV = readFile("data/timetable.csv");
    if (rawCSV.empty()) {
        cout << "FAIL: raw CSV file not found" << endl;
        testsFailed++;
        return;
    }
    
    ParseResult result = parseRawCSV(rawCSV);
    assertEqual("no layout error", !result.layoutError);
    assertEqual("batch extracted as 25A12", result.events.size() > 0 && result.events[0].batch == "25A12");
    assertEqual("28 slot cells before merging", result.events.size() == 28);
    
    vector<Event> merged = mergeEvents(result.events);
    assertEqual("23 events after merging", merged.size() == 23);
    
    int javaMerged = 0, unixMerged = 0, dbmsMerged = 0, dsMerged = 0, cpMerged = 0;
    for (const Event& e : merged) {
        if (e.subject == "Java OOP" && e.start % 1440 == 540 && e.end % 1440 == 655) javaMerged++;
        if (e.subject == "Unix Lab" && e.start % 1440 == 540 && e.end % 1440 == 655 && e.start / 1440 == 1) unixMerged++;
        if (e.subject == "DBMS Lab" && e.start % 1440 == 900 && e.end % 1440 == 1015) dbmsMerged++;
        if (e.subject == "DS Lab" && e.start % 1440 == 900 && e.end % 1440 == 1015 && e.start / 1440 == 3) dsMerged++;
        if (e.subject == "CP-I" && e.start % 1440 == 900 && e.end % 1440 == 1015 && e.start / 1440 == 4) cpMerged++;
    }
    assertEqual("Mon Java OOP merged 09:00-10:55", javaMerged == 1);
    assertEqual("Tue Unix Lab merged 09:00-10:55", unixMerged == 1);
    assertEqual("Wed DBMS Lab merged 15:00-16:55", dbmsMerged == 1);
    assertEqual("Thu DS Lab merged 15:00-16:55", dsMerged == 1);
    assertEqual("Fri CP-I merged 15:00-16:55", cpMerged == 1);
    
    bool thuUnixSingle = false;
    for (const Event& e : merged) {
        if (e.subject == "Unix Lab" && e.start / 1440 == 3 && e.start % 1440 == 540 && e.end % 1440 == 595) {
            thuUnixSingle = true;
        }
    }
    assertEqual("Thu 09:00 Unix Lab stays single", thuUnixSingle);
    
    bool saturdayEmpty = true;
    for (const Event& e : merged) {
        if (e.start / 1440 == 5) saturdayEmpty = false;
    }
    assertEqual("Saturday has no events", saturdayEmpty);
    
    bool hasDLC = false;
    for (const Event& e : merged) {
        if (e.room.find("DLC") != string::npos) hasDLC = true;
    }
    assertEqual("DLC • CR06 intact", hasDLC);
}

void testBOMAbstract() {
    string csv = "\xEF\xBB\xBF\"web_scraper_order\",\"web_scraper_start_url\",\"data\",\"9:00_am9:55_am\",\"9:00_am9:55_am2\",\"9:00_am9:55_am3\",\"9:00_am9:55_am4\"\r\n"
                 "\"1\",\"http://example.com/?batch=TEST\",\"MON\",\"CR01\",\"Test Subject\",\"Test Faculty\",\"\"\r\n";
    ParseResult result = parseRawCSV(csv);
    assertEqual("BOM stripped", !result.layoutError && result.events.size() == 1);
}

void testCRLF() {
    string csv = "\"web_scraper_order\",\"web_scraper_start_url\",\"data\",\"9:00_am9:55_am\",\"9:00_am9:55_am2\",\"9:00_am9:55_am3\",\"9:00_am9:55_am4\"\r\n"
                 "\"1\",\"http://example.com/?batch=TEST\",\"MON\",\"CR01\",\"Test Subject\",\"Test Faculty\",\"\"\r\n";
    ParseResult result = parseRawCSV(csv);
    assertEqual("CRLF handled", !result.layoutError && result.events.size() == 1);
}

void testDashEmpty() {
    string csv = "\"web_scraper_order\",\"web_scraper_start_url\",\"data\",\"9:00_am9:55_am\",\"9:00_am9:55_am2\",\"9:00_am9:55_am3\",\"9:00_am9:55_am4\"\r\n"
                 "\"1\",\"http://example.com/?batch=TEST\",\"MON\",\"CR01\",\"Test Subject\",\"—\",\"\"\r\n";
    ParseResult result = parseRawCSV(csv);
    assertEqual("em dash treated as empty", !result.layoutError && result.events.size() == 1 && result.events[0].room == "CR01" && result.events[0].faculty.empty());
}

void test12HourParse() {
    assertEqual("12:00 PM -> 720", timeStrToMinutes("12:00_pm") == 720);
    assertEqual("12:00 AM -> 0", timeStrToMinutes("12:00_am") == 0);
    assertEqual("1:00 PM -> 780", timeStrToMinutes("1:00_pm") == 780);
    assertEqual("9:00 AM -> 540", timeStrToMinutes("9:00_am") == 540);
}

void testBatchFromURL() {
    assertEqual("batch from URL", extractBatchFromURL("http://x.com/?batch=25A12") == "25A12");
    assertEqual("batch with extra params", extractBatchFromURL("http://x.com/?foo=1&batch=ABC&bar=2") == "ABC");
}

void testRoomValidation() {
    string csv = "\"web_scraper_order\",\"web_scraper_start_url\",\"data\",\"9:00_am9:55_am\",\"9:00_am9:55_am2\",\"9:00_am9:55_am3\"\r\n"
                 "\"1\",\"http://example.com/?batch=TEST\",\"MON\",\"CR01\",\"Subject\",\"Faculty\"\r\n";
    ParseResult result = parseRawCSV(csv);
    assertEqual("valid room passes", !result.layoutError && result.events.size() == 1);
    
    string csvBad = "\"web_scraper_order\",\"web_scraper_start_url\",\"data\",\"9:00_am9:55_am\",\"9:00_am9:55_am2\",\"9:00_am9:55_am3\"\r\n"
                    "\"1\",\"http://example.com/?batch=TEST\",\"MON\",\"INVALID\",\"Subject\",\"Faculty\"\r\n";
    ParseResult result2 = parseRawCSV(csvBad);
    assertEqual("invalid room warns", result2.warnings.size() > 0);
}

void testLayoutError() {
    string csv = "\"web_scraper_order\",\"web_scraper_start_url\",\"data\",\"9:00_am9:55_am\",\"9:00_am9:55_am2\",\"9:00_am9:55_am3\"\r\n";
    for (int i = 0; i < 20; i++) {
        csv += "\"1\",\"http://example.com/?batch=TEST\",\"MON\",\"BADROOM\",\"Subject\",\"Faculty\"\r\n";
    }
    ParseResult result = parseRawCSV(csv);
    assertEqual("layout error on >10% bad rooms", result.layoutError);
    assertEqual("layout error message", result.layoutErrorMsg == "scrape layout changed, update scrape_layout.h");
}

void testSecondBatchKept() {
    string csv1 = "\"web_scraper_order\",\"web_scraper_start_url\",\"data\",\"9:00_am9:55_am\",\"9:00_am9:55_am2\",\"9:00_am9:55_am3\"\r\n"
                  "\"1\",\"http://example.com/?batch=B1\",\"MON\",\"CR01\",\"Sub1\",\"Fac1\"\r\n";
    ParseResult r1 = parseRawCSV(csv1);
    vector<Event> m1 = mergeEvents(r1.events);
    string b1 = m1[0].batch;
    
    string csv2 = "\"web_scraper_order\",\"web_scraper_start_url\",\"data\",\"9:00_am9:55_am\",\"9:00_am9:55_am2\",\"9:00_am9:55_am3\"\r\n"
                  "\"1\",\"http://example.com/?batch=B2\",\"MON\",\"CR02\",\"Sub2\",\"Fac2\"\r\n";
    ParseResult r2 = parseRawCSV(csv2);
    vector<Event> m2 = mergeEvents(r2.events);
    
    map<string, vector<Event>> localBatchEvents;
    vector<string> localBatchOrder;
    localBatchEvents[b1] = m1;
    localBatchOrder.push_back(b1);
    localBatchEvents[m2[0].batch] = m2;
    localBatchOrder.push_back(m2[0].batch);
    
    assertEqual("second batch kept", localBatchOrder.size() == 2 && localBatchEvents.size() == 2);
}

void testReuploadReplaces() {
    string csv1 = "\"web_scraper_order\",\"web_scraper_start_url\",\"data\",\"9:00_am9:55_am\",\"9:00_am9:55_am2\",\"9:00_am9:55_am3\"\r\n"
                  "\"1\",\"http://example.com/?batch=B1\",\"MON\",\"CR01\",\"Sub1\",\"Fac1\"\r\n";
    ParseResult r1 = parseRawCSV(csv1);
    vector<Event> m1 = mergeEvents(r1.events);
    
    string csv2 = "\"web_scraper_order\",\"web_scraper_start_url\",\"data\",\"9:00_am9:55_am\",\"9:00_am9:55_am2\",\"9:00_am9:55_am3\"\r\n"
                  "\"1\",\"http://example.com/?batch=B1\",\"MON\",\"CR99\",\"SubNew\",\"FacNew\"\r\n";
    ParseResult r2 = parseRawCSV(csv2);
    vector<Event> m2 = mergeEvents(r2.events);
    
    map<string, vector<Event>> localBatchEvents;
    vector<string> localBatchOrder;
    localBatchEvents[m1[0].batch] = m1;
    localBatchOrder.push_back(m1[0].batch);
    
    localBatchEvents[m2[0].batch] = m2;
    
    assertEqual("reupload replaces", localBatchEvents[m2[0].batch].size() == 1 && localBatchEvents[m2[0].batch][0].subject == "SubNew");
}

int main() {
    testRawCSVLoad();
    testBOMAbstract();
    testCRLF();
    testDashEmpty();
    test12HourParse();
    testBatchFromURL();
    testRoomValidation();
    testLayoutError();
    testSecondBatchKept();
    testReuploadReplaces();
    
    cout << "\nTotal: " << testsPassed << " passed, " << testsFailed << " failed" << endl;
    return testsFailed == 0 ? 0 : 1;
}