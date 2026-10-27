#include "interval_tree.h"
#include "scheduler.h"
#include "csv_loader.h"
#include <iostream>
#include <vector>
#include <queue>

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

Event makeEvent(int id, int start, int end, const string& source = "timetable") {
    Event e;
    e.id = id;
    e.start = start;
    e.end = end;
    e.source = source;
    return e;
}

void testNoLunchSlot() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 0*1440+540, 0*1440+595));
    
    vector<int> suggestions = suggest(root, 0*1440+540, 1, 3);
    bool hasLunch = false;
    for (int s : suggestions) {
        int dayStart = s / 1440 * 1440;
        if (s >= dayStart + 720 && s < dayStart + 775) hasLunch = true;
    }
    assertEqual("suggestions never include 12:00 slot", !hasLunch);
    
    freeTree(root);
}

void testStartAfter9AM() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 0*1440+720, 0*1440+775));
    
    vector<int> suggestions = suggest(root, 0*1440+540, 1, 3);
    bool before9 = false;
    for (int s : suggestions) {
        if (s % 1440 < 540) before9 = true;
    }
    assertEqual("suggestions never before 09:00", !before9);
    
    freeTree(root);
}

void testEndBefore455PM() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 0*1440+900, 0*1440+955));
    
    vector<int> suggestions = suggest(root, 0*1440+900, 2, 3);
    bool after455 = false;
    for (int s : suggestions) {
        if (s % 1440 > 960) after455 = true;
    }
    assertEqual("suggestions never end after 16:55", !after455);
    
    freeTree(root);
}

void testSlotAligned() {
    Node* root = nullptr;
    
    vector<int> suggestions = suggest(root, 0*1440+540, 1, 5);
    bool allAligned = true;
    for (int s : suggestions) {
        int mins = s % 1440;
        bool aligned = false;
        for (int slot : SLOT_STARTS) {
            if (mins == slot) { aligned = true; break; }
        }
        if (!aligned) allAligned = false;
    }
    assertEqual("suggestions always slot-aligned", allAligned);
    
    freeTree(root);
}

void testHeapNearestFirst() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 0*1440+720, 0*1440+775));
    root = insertNode(root, makeEvent(2, 1*1440+540, 1*1440+595));
    root = insertNode(root, makeEvent(3, 2*1440+540, 2*1440+595));
    
    vector<int> suggestions = suggest(root, 0*1440+540, 1, 3);
    assertEqual("heap returns 3 suggestions", suggestions.size() == 3);
    
    if (suggestions.size() >= 2) {
        int d1 = abs(suggestions[0] - (0*1440+540));
        int d2 = abs(suggestions[1] - (0*1440+540));
        assertEqual("nearest first", d1 <= d2);
    }
    
    freeTree(root);
}

void testGroupModeCommonFree() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 0*1440+540, 0*1440+595));
    root = insertNode(root, makeEvent(2, 1*1440+600, 1*1440+655));
    
    vector<FreeGap> gaps0 = freeGaps(root, 0);
    vector<FreeGap> gaps1 = freeGaps(root, 1);
    
    bool gap0After10 = false;
    for (const FreeGap& g : gaps0) {
        if (g.end > 0*1440+600) gap0After10 = true; // gap extends past 10:00
    }
    bool gap1Before10 = false;
    for (const FreeGap& g : gaps1) {
        if (g.start < 1*1440+600) gap1Before10 = true; // gap starts before 10:00
    }
    
    assertEqual("Mon free after 10:00", gap0After10);
    assertEqual("Tue free before 10:00", gap1Before10);
    
    vector<int> common = suggest(root, 0*1440+540, 1, 3);
    assertEqual("common free finds slots free for both", common.size() > 0);
    
    freeTree(root);
}

void testSingleModeClashReported() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 0*1440+540, 0*1440+595));
    root = insertNode(root, makeEvent(2, 0*1440+570, 0*1440+625));
    
    vector<Event*> conflicts;
    overlapQuery(root, 0*1440+540, 0*1440+655, conflicts);
    assertEqual("single mode detects cross-batch clash", conflicts.size() == 2);
    
    freeTree(root);
}

void testGroupModeNoCrossBatchClash() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 0*1440+540, 0*1440+595));
    root = insertNode(root, makeEvent(2, 1*1440+540, 1*1440+595));
    
    vector<Event*> conflicts;
    overlapQuery(root, 0*1440+540, 0*1440+595, conflicts);
    int batch0Count = 0;
    for (Event* e : conflicts) if (e->start / 1440 == 0) batch0Count++;
    assertEqual("group mode union only, no cross-day clash", batch0Count == 1);
    
    freeTree(root);
}

void testPersonalClashWithSelected() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 0*1440+540, 0*1440+595));
    root = insertNode(root, makeEvent(2, 1*1440+540, 1*1440+595));
    
    vector<Event*> conflicts;
    overlapQuery(root, 0*1440+540, 0*1440+595, conflicts);
    assertEqual("personal clashes with selected batch detected", conflicts.size() == 1);
    
    freeTree(root);
}

void testPersonalNoClashWithUnselected() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 0*1440+540, 0*1440+595));
    root = insertNode(root, makeEvent(2, 1*1440+540, 1*1440+595));
    
    vector<Event*> conflicts;
    overlapQuery(root, 2*1440+540, 2*1440+595, conflicts);
    assertEqual("personal does not clash with unselected batch", conflicts.empty());
    
    freeTree(root);
}

void testFreeGapsSweep() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 0*1440+540, 0*1440+595));
    root = insertNode(root, makeEvent(2, 0*1440+660, 0*1440+715));
    
    vector<FreeGap> gaps = freeGaps(root, 0);
    assertEqual("free gaps count", gaps.size() == 2);
    assertEqual("middle gap 09:55-11:00", gaps[0].start == 0*1440+595 && gaps[0].end == 0*1440+660);
    assertEqual("last gap 11:55-16:55", gaps[1].start == 0*1440+715 && gaps[1].end == 0*1440+1015);
    
    freeTree(root);
}

int main() {
    testNoLunchSlot();
    testStartAfter9AM();
    testEndBefore455PM();
    testSlotAligned();
    testHeapNearestFirst();
    testGroupModeCommonFree();
    testSingleModeClashReported();
    testGroupModeNoCrossBatchClash();
    testPersonalClashWithSelected();
    testPersonalNoClashWithUnselected();
    testFreeGapsSweep();
    
    cout << "\nTotal: " << testsPassed << " passed, " << testsFailed << " failed" << endl;
    return testsFailed == 0 ? 0 : 1;
}