#ifndef SCRAPE_LAYOUT_H
#define SCRAPE_LAYOUT_H

#include <string>
#include <map>
#include <vector>

using namespace std;

struct SlotRole {
    string role;
    bool required;
};

static const map<string, map<int, SlotRole>> SCRAPE_LAYOUT = {
    {"9:00_am", {{1, {"room", true}}, {2, {"subject", true}}, {3, {"faculty", true}}, {4, {"ignore", false}}}},
    {"10:00_am", {{1, {"subject", true}}, {2, {"faculty", true}}, {3, {"room", true}}}},
    {"11:00_am", {{1, {"subject", true}}, {2, {"faculty", true}}, {3, {"room", true}}}},
    {"12:00_pm", {{1, {"ignore", false}}}},
    {"1:00_pm", {{1, {"room", true}}, {2, {"subject", true}}, {3, {"faculty", true}}}},
    {"2:00_pm", {{1, {"subject", true}}, {2, {"faculty", true}}, {3, {"room", true}}}},
    {"3:00_pm", {{1, {"subject", true}}, {2, {"room", true}}, {3, {"faculty", true}}}},
    {"4:00_pm", {{1, {"subject", true}}, {2, {"faculty", true}}, {3, {"room", true}}, {4, {"ignore", false}}}}
};

inline SlotRole getRole(const string& slotStart, int suffix) {
    auto it = SCRAPE_LAYOUT.find(slotStart);
    if (it == SCRAPE_LAYOUT.end()) return {"unknown", false};
    auto it2 = it->second.find(suffix);
    if (it2 == it->second.end()) return {"unknown", false};
    return it2->second;
}

inline bool isRoomRole(const string& role) { return role == "room"; }
inline bool isSubjectRole(const string& role) { return role == "subject"; }
inline bool isFacultyRole(const string& role) { return role == "faculty"; }
inline bool isIgnoreRole(const string& role) { return role == "ignore"; }

const double ROOM_VALIDATION_THRESHOLD = 0.10;
const string ROOM_PREFIXES = "CR|CL|LT|TR|DLC";
const string FACULTY_PATTERN = "^[A-Z]{2,5}(_RS\\d+)?$";

#endif