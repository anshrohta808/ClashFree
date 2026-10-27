#include "interval_tree.h"

Node* insertNode(Node* root, const Event& event) {
    if (!root) return new Node(event);
    if (event.start < root->event.start) {
        root->left = insertNode(root->left, event);
    } else {
        root->right = insertNode(root->right, event);
    }
    pull(root);
    return root;
}

bool anyOverlap(Node* root, int start, int end) {
    if (!root) return false;
    if (root->event.start < end && start < root->event.end) return true;
    if (root->left && root->left->maxEnd > start) {
        if (anyOverlap(root->left, start, end)) return true;
    }
    if (root->event.start < end) {
        if (anyOverlap(root->right, start, end)) return true;
    }
    return false;
}

void overlapQuery(Node* root, int start, int end, vector<Event*>& results) {
    if (!root) return;
    if (root->left && root->left->maxEnd > start) {
        overlapQuery(root->left, start, end, results);
    }
    if (root->event.start < end && start < root->event.end) {
        results.push_back(&root->event);
    }
    if (root->event.start < end) {
        overlapQuery(root->right, start, end, results);
    }
}

Node* findMin(Node* root) {
    while (root && root->left) root = root->left;
    return root;
}

Node* deleteNode(Node* root, int eventId) {
    if (!root) return nullptr;
    if (eventId < root->event.id) {
        root->left = deleteNode(root->left, eventId);
    } else if (eventId > root->event.id) {
        root->right = deleteNode(root->right, eventId);
    } else {
        if (!root->left) {
            Node* right = root->right;
            delete root;
            return right;
        } else if (!root->right) {
            Node* left = root->left;
            delete root;
            return left;
        } else {
            Node* succ = findMin(root->right);
            root->event = succ->event;
            root->right = deleteNode(root->right, succ->event.id);
        }
    }
    pull(root);
    return root;
}

void freeTree(Node* root) {
    if (!root) return;
    freeTree(root->left);
    freeTree(root->right);
    delete root;
}

void inorderCollect(Node* root, vector<Event*>& out) {
    if (!root) return;
    inorderCollect(root->left, out);
    out.push_back(&root->event);
    inorderCollect(root->right, out);
}