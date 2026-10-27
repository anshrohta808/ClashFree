#include "csv_loader.h"
#include "scrape_layout.h"
#include "config.h"
#include <regex>
#include <sstream>
#include <algorithm>
#include <cctype>

static string stripBOM(const string& s) {
    if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) {
        return s.substr(3);
    }
    return s;
}

static vector<string> parseCSVLine(const string& line, size_t& pos) {
    vector<string> fields;
    string field;
    bool inQuotes = false;
    
    while (pos < line.size()) {
        char c = line[pos++];
        if (inQuotes) {
            if (c == '"') {
                if (pos < line.size() && line[pos] == '"') {
                    field += '"';
                    pos++;
                } else {
                    inQuotes = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == ',') {
                fields.push_back(field);
                field.clear();
            } else if (c == '\r' || c == '\n') {
                fields.push_back(field);
                field.clear();
                while (pos < line.size() && (line[pos] == '\r' || line[pos] == '\n')) pos++;
                break;
            } else {
                field += c;
            }
        }
    }
    if (!field.empty() || pos >= line.size()) {
        fields.push_back(field);
    }
    return fields;
}

static vector<vector<string>> parseCSV(const string& csvText) {
    string text = stripBOM(csvText);
    vector<vector<string>> rows;
    size_t pos = 0;
    
    while (pos < text.size()) {
        vector<string> row = parseCSVLine(text, pos);
        if (!row.empty() || pos >= text.size()) {
            rows.push_back(row);
        }
    }
    return rows;
}

int timeStrToMinutes(const string& timeStr) {
    regex pattern(R"((\d{1,2}):(\d{2})\s*_(am|pm))", regex::icase);
    smatch match;
    if (!regex_match(timeStr, match, pattern)) return -1;
    int hour = stoi(match[1]);
    int minute = stoi(match[2]);
    string ampm = match[3];
    transform(ampm.begin(), ampm.end(), ampm.begin(), ::tolower);
    if (ampm == "pm" && hour != 12) hour += 12;
    if (ampm == "am" && hour == 12) hour = 0;
    return hour * 60 + minute;
}

string extractBatchFromURL(const string& url) {
    size_t pos = url.find("batch=");
    if (pos == string::npos) return "";
    pos += 6;
    size_t end = url.find_first_of("&?", pos);
    if (end == string::npos) end = url.size();
    return url.substr(pos, end - pos);
}

int dayStrToIndex(const string& dayStr) {
    string d = dayStr;
    transform(d.begin(), d.end(), d.begin(), ::toupper);
    if (d == "MON") return 0;
    if (d == "TUE") return 1;
    if (d == "WED") return 2;
    if (d == "THU") return 3;
    if (d == "FRI") return 4;
    if (d == "SAT") return 5;
    if (d == "SUN") return 6;
    return -1;
}

static bool isEmptyCell(const string& s) {
    return s.empty() || s == "—" || s == "\u2014";
}

static string inferType(const string& subject, const string& faculty, const string& room) {
    if (subject.find("Lab") != string::npos) return "lab";
    if (!room.empty() && room.rfind("CL", 0) == 0) return "lab";
    if (!faculty.empty() && regex_match(faculty, regex(".*_RS\\d+$"))) return "tutorial";
    if (!room.empty() && room.rfind("TR", 0) == 0) return "tutorial";
    return "lecture";
}

static bool validateRoom(const string& room) {
    static const regex roomRegex("^(CR|CL|LT|TR|DLC).*");
    return regex_match(room, roomRegex);
}

static bool validateFaculty(const string& faculty) {
    static const regex facultyRegex("^[A-Z]{2,5}(_RS\\d+)?$");
    return regex_match(faculty, facultyRegex);
}

static bool looksLikeRoom(const string& s) {
    static const regex roomLike("^(CR|CL|LT|TR|DLC).*");
    return regex_match(s, roomLike);
}

ParseResult parseRawCSV(const string& csvText) {
    ParseResult result;
    result.layoutError = false;
    
    vector<vector<string>> rows = parseCSV(csvText);
    if (rows.empty()) return result;
    
    vector<string> headers = rows[0];
    size_t batchCol = string::npos, urlCol = string::npos, dataCol = string::npos;
    for (size_t i = 0; i < headers.size(); i++) {
        if (headers[i] == "web_scraper_order") batchCol = i;
        else if (headers[i] == "web_scraper_start_url") urlCol = i;
        else if (headers[i] == "data") dataCol = i;
    }
    if (batchCol == string::npos || urlCol == string::npos || dataCol == string::npos) {
        result.warnings.push_back("Missing required columns");
        return result;
    }
    
    map<string, string> slotHeaderMap;
    vector<string> slotNames;
    for (size_t i = 0; i < headers.size(); i++) {
        if ((int)i == (int)batchCol || (int)i == (int)urlCol || (int)i == (int)dataCol) continue;
        string h = headers[i];
        slotNames.push_back(h);
    }
    
    regex slotPattern(R"((\d{1,2}:\d{2}_(?:am|pm))(\d{1,2}:\d{2}_(?:am|pm))(\d*))", regex::icase);
    map<string, vector<pair<int, string>>> slotToCols;
    
    for (size_t i = 0; i < slotNames.size(); i++) {
        smatch match;
        if (regex_match(slotNames[i], match, slotPattern)) {
            string start = match[1];
            string end = match[2];
            string suffixStr = match[3];
            int suffix = suffixStr.empty() ? 1 : stoi(suffixStr);
            string key = start + "-" + end;
            slotToCols[key].push_back({suffix, slotNames[i]});
        } else {
            result.warnings.push_back("Header '" + slotNames[i] + "' does not match slot pattern, skipping");
        }
    }
    
    int totalRoomCells = 0;
    int validRoomCells = 0;
    
    for (size_t r = 1; r < rows.size(); r++) {
        const vector<string>& row = rows[r];
        if (row.size() <= max({batchCol, urlCol, dataCol})) continue;
        
        string batch = extractBatchFromURL(row[urlCol]);
        if (batch.empty()) {
            result.warnings.push_back("Row " + to_string(r) + ": could not extract batch from URL");
            continue;
        }
        
        int day = dayStrToIndex(row[dataCol]);
        if (day < 0) {
            result.warnings.push_back("Row " + to_string(r) + ": invalid day '" + row[dataCol] + "'");
            continue;
        }
        
        for (auto& kv : slotToCols) {
            string slotKey = kv.first;
            size_t dashPos = slotKey.find('-');
            string startStr = slotKey.substr(0, dashPos);
            string endStr = slotKey.substr(dashPos + 1);
            
            int startMin = timeStrToMinutes(startStr);
            int endMin = timeStrToMinutes(endStr);
            if (startMin < 0 || endMin < 0) {
                result.warnings.push_back(batch + "/" + row[dataCol] + "/" + slotKey + ": invalid time format");
                continue;
            }
            
            int absStart = day * 1440 + startMin;
            int absEnd = day * 1440 + endMin;
            if (absStart >= absEnd) {
                result.warnings.push_back(batch + "/" + row[dataCol] + "/" + slotKey + ": start >= end");
                continue;
            }
            
            struct SlotValues {
                string subject = "";
                string faculty = "";
                string room = "";
                bool hasSubject = false;
                vector<pair<string, string>> roomVals; // (val, slotKey+suffix) for validation
                vector<pair<string, string>> facultyVals;
            };
            SlotValues vals;
            
            for (auto& sc : kv.second) {
                int suffix = sc.first;
                string colName = sc.second;
                size_t colIdx = string::npos;
                for (size_t i = 0; i < headers.size(); i++) {
                    if (headers[i] == colName) { colIdx = i; break; }
                }
                if (colIdx == string::npos || colIdx >= row.size()) continue;
                
                string val = row[colIdx];
                if (isEmptyCell(val)) continue;
                
                SlotRole role = getRole(startStr, suffix);
                if (role.role == "unknown") {
                    result.warnings.push_back(batch + "/" + row[dataCol] + "/" + slotKey + " suffix " + to_string(suffix) + ": unknown role");
                    continue;
                }
                
                if (role.role == "subject") {
                    vals.subject = val;
                    vals.hasSubject = true;
                } else if (role.role == "faculty") {
                    vals.faculty = val;
                    vals.facultyVals.push_back({val, slotKey + " suffix " + to_string(suffix)});
                } else if (role.role == "room") {
                    vals.room = val;
                    vals.roomVals.push_back({val, slotKey + " suffix " + to_string(suffix)});
                }
            }
            
            if (!vals.hasSubject) continue;
            
            for (auto& rv : vals.roomVals) {
                totalRoomCells++;
                if (validateRoom(rv.first)) validRoomCells++;
                else {
                    result.warnings.push_back(batch + "/" + row[dataCol] + "/" + rv.second + ": room validation failed: " + rv.first);
                }
            }
            for (auto& fv : vals.facultyVals) {
                if (!validateFaculty(fv.first)) {
                    result.warnings.push_back(batch + "/" + row[dataCol] + "/" + fv.second + ": faculty format unusual: " + fv.first);
                }
            }
            
            if (isSubjectRole(getRole(startStr, 1).role) && looksLikeRoom(vals.subject)) {
                result.warnings.push_back(batch + "/" + row[dataCol] + "/" + slotKey + ": subject looks like a room");
            }
            
            string type = inferType(vals.subject, vals.faculty, vals.room);
            Event e;
            e.id = 0;
            e.batch = batch;
            e.subject = vals.subject;
            e.type = type;
            e.faculty = vals.faculty;
            e.room = vals.room;
            e.source = "timetable";
            e.start = absStart;
            e.end = absEnd;
            result.events.push_back(e);
        }
    }
    
    if (totalRoomCells > 0) {
        double validRatio = (double)validRoomCells / totalRoomCells;
        if (validRatio < 1.0 - ROOM_VALIDATION_THRESHOLD) {
            result.layoutError = true;
            result.layoutErrorMsg = "scrape layout changed, update scrape_layout.h";
            result.events.clear();
        }
    }
    
    return result;
}

vector<Event> mergeEvents(const vector<Event>& rawEvents) {
    if (rawEvents.empty()) return {};
    
    vector<Event> sorted = rawEvents;
    sort(sorted.begin(), sorted.end(), [](const Event& a, const Event& b) {
        if (a.batch != b.batch) return a.batch < b.batch;
        if (a.start != b.start) return a.start < b.start;
        return a.end < b.end;
    });
    
    vector<Event> merged;
    Event current = sorted[0];
    current.id = 0;
    
    for (size_t i = 1; i < sorted.size(); i++) {
        const Event& e = sorted[i];
        bool sameClass = (e.batch == current.batch && e.subject == current.subject && 
                         e.faculty == current.faculty && e.room == current.room);
        int gap = e.start - current.end;
        
        if (sameClass && gap >= 0 && gap <= 5) {
            current.end = e.end;
        } else {
            merged.push_back(current);
            current = e;
            current.id = 0;
        }
    }
    merged.push_back(current);
    
    for (size_t i = 0; i < merged.size(); i++) {
        merged[i].id = (int)i + 1;
    }
    
    return merged;
}