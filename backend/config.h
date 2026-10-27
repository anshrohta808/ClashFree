#ifndef CONFIG_H
#define CONFIG_H

#include <vector>
#include <string>

using namespace std;

const int SLOT_LEN = 55;
const int DAY_START = 540;
const int DAY_END = 1080;
const int LUNCH_START = 720;
const int LUNCH_END = 775;
const bool EXCLUDE_LUNCH_FROM_SUGGESTIONS = true;

const vector<int> SLOT_STARTS = {540, 600, 660, 720, 780, 840, 900, 960, 1020};

inline int minutesSinceMonday(int day, int hour, int minute) {
    return day * 1440 + hour * 60 + minute;
}

inline int slotRunEnd(int startSlot, int numSlots) {
    return startSlot + numSlots * 60 - 5;
}

#endif