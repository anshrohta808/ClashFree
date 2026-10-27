#include "scheduler.h"
#include <queue>
#include <algorithm>
#include <climits>

bool isLunchOverlap(int start, int end) {
    int day = start / 1440;
    int lunchStartAbs = day * 1440 + LUNCH_START;
    int lunchEndAbs = day * 1440 + LUNCH_END;
    return !(end <= lunchStartAbs || start >= lunchEndAbs);
}

vector<FreeGap> freeGaps(Node* root, int day) {
    int dayStart = day * 1440 + DAY_START;
    int dayEnd = day * 1440 + DAY_END;
    
    vector<Event*> busy;
    overlapQuery(root, dayStart, dayEnd, busy);
    
    vector<pair<int, int>> intervals;
    for (Event* e : busy) {
        int s = max(e->start, dayStart);
        int eEnd = min(e->end, dayEnd);
        if (s < eEnd) intervals.push_back({s, eEnd});
    }
    
    sort(intervals.begin(), intervals.end());
    
    vector<pair<int, int>> merged;
    for (auto& iv : intervals) {
        if (merged.empty() || iv.first > merged.back().second) {
            merged.push_back(iv);
        } else {
            merged.back().second = max(merged.back().second, iv.second);
        }
    }
    
    vector<FreeGap> gaps;
    int cursor = dayStart;
    for (auto& iv : merged) {
        if (cursor < iv.first) {
            gaps.push_back({cursor, iv.first});
        }
        cursor = max(cursor, iv.second);
    }
    if (cursor < dayEnd) {
        gaps.push_back({cursor, dayEnd});
    }
    
    return gaps;
}

vector<SlotCandidate> validSlotRuns(const vector<FreeGap>& gaps, int day, int numSlots) {
    vector<SlotCandidate> candidates;
    int runLen = numSlots * 60 - 5;
    
    for (int slotStart : SLOT_STARTS) {
        int absStart = day * 1440 + slotStart;
        int absEnd = absStart + runLen;
        
        if (absEnd > day * 1440 + DAY_END) continue;
        
        if (EXCLUDE_LUNCH_FROM_SUGGESTIONS && isLunchOverlap(absStart, absEnd)) continue;
        
        for (const FreeGap& gap : gaps) {
            if (absStart >= gap.start && absEnd <= gap.end) {
                int preferredAbs = 0 * 1440 + DAY_START;
                int distance = abs(absStart - preferredAbs);
                candidates.push_back({absStart, absEnd, day, distance});
                break;
            }
        }
    }
    
    return candidates;
}

vector<int> suggest(Node* root, int preferredStart, int numSlots, int k) {
    priority_queue<SlotCandidate, vector<SlotCandidate>, greater<SlotCandidate>> pq;
    
    for (int day = 0; day < 6; day++) {
        vector<FreeGap> gaps = freeGaps(root, day);
        vector<SlotCandidate> candidates = validSlotRuns(gaps, day, numSlots);
        for (const SlotCandidate& c : candidates) {
            int distance = abs(c.start - preferredStart);
            pq.push({c.start, c.end, c.day, distance});
        }
    }
    
    vector<int> results;
    while (!pq.empty() && (int)results.size() < k) {
        results.push_back(pq.top().start);
        pq.pop();
    }
    
    return results;
}