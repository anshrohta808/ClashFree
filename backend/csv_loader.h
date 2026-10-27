#ifndef CSV_LOADER_H
#define CSV_LOADER_H

#include <string>
#include <vector>
#include <map>
#include "interval_tree.h"

using namespace std;

struct ParseResult {
    vector<Event> events;
    vector<string> warnings;
    bool layoutError;
    string layoutErrorMsg;
};

ParseResult parseRawCSV(const string& csvText);
vector<Event> mergeEvents(const vector<Event>& rawEvents);
int timeStrToMinutes(const string& timeStr);
string extractBatchFromURL(const string& url);
int dayStrToIndex(const string& dayStr);

#endif