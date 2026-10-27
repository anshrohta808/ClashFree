#include "interval_tree.h"
#include <iostream>
#include <vector>
#include <cassert>

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

Event makeEvent(int id, int start, int end) {
    Event e;
    e.id = id;
    e.start = start;
    e.end = end;
    e.source = "test";
    return e;
}

void testTouching() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 540, 595));
    root = insertNode(root, makeEvent(2, 595, 650));
    
    vector<Event*> results;
    overlapQuery(root, 540, 595, results);
    assertEqual("touching intervals not overlapping", results.size() == 1 && results[0]->id == 1);
    
    results.clear();
    overlapQuery(root, 595, 650, results);
    assertEqual("touching intervals not overlapping (2)", results.size() == 1 && results[0]->id == 2);
    
    bool any = anyOverlap(root, 530, 540);
    assertEqual("anyOverlap before first", !any);
    
    any = anyOverlap(root, 650, 700);
    assertEqual("anyOverlap after last", !any);
    
    freeTree(root);
}

void testNested() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 540, 650));
    root = insertNode(root, makeEvent(2, 570, 600));
    
    vector<Event*> results;
    overlapQuery(root, 560, 580, results);
    assertEqual("nested overlap finds both", results.size() == 2);
    
    freeTree(root);
}

void testIdentical() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 540, 595));
    root = insertNode(root, makeEvent(2, 540, 595));
    
    vector<Event*> results;
    overlapQuery(root, 540, 595, results);
    assertEqual("identical intervals both found", results.size() == 2);
    
    freeTree(root);
}

void testPartialOverlap() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 540, 595));
    root = insertNode(root, makeEvent(2, 570, 625));
    
    vector<Event*> results;
    overlapQuery(root, 560, 600, results);
    assertEqual("partial overlap finds both", results.size() == 2);
    
    freeTree(root);
}

void testNoOverlap() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 540, 595));
    root = insertNode(root, makeEvent(2, 600, 655));
    
    vector<Event*> results;
    overlapQuery(root, 595, 600, results);
    assertEqual("no overlap at boundary", results.empty());
    
    results.clear();
    overlapQuery(root, 500, 530, results);
    assertEqual("no overlap before", results.empty());
    
    freeTree(root);
}

void testDeleteLeaf() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 540, 595));
    root = insertNode(root, makeEvent(2, 600, 655));
    root = insertNode(root, makeEvent(3, 660, 715));
    
    root = deleteNode(root, 2);
    
    vector<Event*> results;
    inorderCollect(root, results);
    assertEqual("delete leaf - remaining count", results.size() == 2);
    assertEqual("delete leaf - correct ids", results[0]->id == 1 && results[1]->id == 3);
    
    bool any = anyOverlap(root, 600, 655);
    assertEqual("delete leaf - gone", !any);
    
    freeTree(root);
}

void testDeleteOneChild() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 540, 595));
    root = insertNode(root, makeEvent(2, 600, 655));
    root = insertNode(root, makeEvent(3, 660, 715));
    root = insertNode(root, makeEvent(4, 720, 775));
    
    root = deleteNode(root, 3);
    
    vector<Event*> results;
    inorderCollect(root, results);
    assertEqual("delete one child - count", results.size() == 3);
    
    bool any = anyOverlap(root, 660, 715);
    assertEqual("delete one child - gone", !any);
    
    freeTree(root);
}

void testDeleteTwoChildren() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 540, 595));
    root = insertNode(root, makeEvent(2, 600, 655));
    root = insertNode(root, makeEvent(3, 660, 715));
    root = insertNode(root, makeEvent(4, 720, 775));
    root = insertNode(root, makeEvent(5, 780, 835));
    
    root = deleteNode(root, 3);
    
    vector<Event*> results;
    inorderCollect(root, results);
    assertEqual("delete two children - count", results.size() == 4);
    bool found3 = false;
    for (Event* e : results) if (e->id == 3) found3 = true;
    assertEqual("delete two children - gone", !found3);
    
    freeTree(root);
}

void testQueryAfterDelete() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 540, 650));
    root = insertNode(root, makeEvent(2, 600, 700));
    root = insertNode(root, makeEvent(3, 720, 780));
    
    root = deleteNode(root, 2);
    
    vector<Event*> results;
    overlapQuery(root, 620, 680, results);
    assertEqual("query after delete - no overlap with deleted", results.size() == 1 && results[0]->id == 1);
    
    freeTree(root);
}

void testMaxEndAfterDelete() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 540, 595));
    root = insertNode(root, makeEvent(2, 600, 655));
    root = insertNode(root, makeEvent(3, 660, 715));
    
    int maxBefore = root->maxEnd;
    root = deleteNode(root, 3);
    int maxAfter = root->maxEnd;
    
    assertEqual("maxEnd updated after delete", maxAfter == 655);
    
    freeTree(root);
}

void testAnyOverlapAgrees() {
    Node* root = nullptr;
    root = insertNode(root, makeEvent(1, 540, 595));
    root = insertNode(root, makeEvent(2, 600, 655));
    root = insertNode(root, makeEvent(3, 660, 715));
    
    for (int s = 500; s < 800; s += 30) {
        for (int e = s + 10; e < 800; e += 40) {
            bool any = anyOverlap(root, s, e);
            vector<Event*> results;
            overlapQuery(root, s, e, results);
            bool query = !results.empty();
            if (any != query) {
                cout << "  Mismatch at [" << s << "," << e << "]: any=" << any << " query=" << query << endl;
            }
            assertEqual("anyOverlap agrees with overlapQuery at [" + to_string(s) + "," + to_string(e) + "]", any == query);
        }
    }
    
    freeTree(root);
}

int main() {
    testTouching();
    testNested();
    testIdentical();
    testPartialOverlap();
    testNoOverlap();
    testDeleteLeaf();
    testDeleteOneChild();
    testDeleteTwoChildren();
    testQueryAfterDelete();
    testMaxEndAfterDelete();
    testAnyOverlapAgrees();
    
    cout << "\nTotal: " << testsPassed << " passed, " << testsFailed << " failed" << endl;
    return testsFailed == 0 ? 0 : 1;
}