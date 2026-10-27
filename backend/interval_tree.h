#ifndef INTERVAL_TREE_H
#define INTERVAL_TREE_H

#include <vector>
#include "config.h"

using namespace std;

struct Event {
    int id;
    string batch;
    string subject;
    string type;
    string faculty;
    string room;
    string source;
    int start;
    int end;
};

struct Node {
    Event event;
    int maxEnd;
    Node* left;
    Node* right;
    Node(const Event& e) : event(e), maxEnd(e.end), left(nullptr), right(nullptr) {}
};

inline int max3(int a, int b, int c) {
    int m = a > b ? a : b;
    return m > c ? m : c;
}

inline void pull(Node* node) {
    if (!node) return;
    int leftMax = node->left ? node->left->maxEnd : -1;
    int rightMax = node->right ? node->right->maxEnd : -1;
    node->maxEnd = max3(node->event.end, leftMax, rightMax);
}

Node* insertNode(Node* root, const Event& event);
bool anyOverlap(Node* root, int start, int end);
void overlapQuery(Node* root, int start, int end, vector<Event*>& results);
Node* deleteNode(Node* root, int eventId);
Node* findMin(Node* root);
void freeTree(Node* root);
void inorderCollect(Node* root, vector<Event*>& out);

#endif