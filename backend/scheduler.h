#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <vector>
#include "interval_tree.h"
#include "config.h"

using namespace std;

struct FreeGap {
    int start;
    int end;
};

struct SlotCandidate {
    int start;
    int end;
    int day;
    int distance;
    
    bool operator>(const SlotCandidate& other) const {
        return distance > other.distance;
    }
};

vector<FreeGap> freeGaps(Node* root, int day);
vector<SlotCandidate> validSlotRuns(const vector<FreeGap>& gaps, int day, int numSlots);
vector<int> suggest(Node* root, int preferredStart, int numSlots, int k = 3);
bool isLunchOverlap(int start, int end);

#endif