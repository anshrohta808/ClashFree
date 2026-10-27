# ClashFree: DSA Notes for College Lab Evaluation

This document explains the data structures and algorithms used in the ClashFree timetable conflict detector. It is written for understanding and viva preparation, not just for describing the files.

## 1. What the project does

ClashFree loads timetable data, converts each class into a time interval, stores the selected classes in an interval tree, and provides:

- timetable clash detection;
- all events overlapping a requested time range;
- free periods for a day;
- valid runs of college slots;
- the nearest free suggestions using a min-heap.

The main data flow is:

```text
CSV text -> CSV parser -> Event objects -> merge consecutive slots
         -> interval tree -> overlap queries / free gaps / heap suggestions
```

Important files:

| File | Role |
|---|---|
| `backend/interval_tree.h/.cpp` | Core augmented BST and interval queries |
| `backend/scheduler.h/.cpp` | Free gaps, slot candidates, and heap suggestions |
| `backend/csv_loader.h/.cpp` | CSV parsing, validation, and event merging |
| `backend/config.h` | Time constants and slot boundaries |
| `backend/test_tree.cpp` | Interval-tree tests |
| `backend/test_scheduler.cpp` | Scheduling and heap tests |
| `backend/test_csv.cpp` | CSV and merging tests |

---

## 2. Basic model: an event is an interval

Every class is represented by an `Event`:

```cpp
struct Event {
    int id;
    string batch, subject, type, faculty, room, source;
    int start;
    int end;
};
```

The important DSA fields are `start` and `end`. They describe a time interval `[start, end)`.

### Absolute time representation

The program stores time as minutes from Monday 00:00:

```text
absolute time = day number * 1440 + minutes after midnight
```

Examples:

```text
Monday 09:00 = 0 * 1440 + 540 = 540
Tuesday 09:00 = 1 * 1440 + 540 = 1980
```

This makes comparison easy: events from different days can be compared using ordinary integer comparisons.

### Half-open intervals

The project uses `[start, end)`: start is included, end is excluded.

Two intervals overlap if and only if:

```cpp
first.start < second.end && second.start < first.end
```

For a query `[qStart, qEnd)` the code checks:

```cpp
event.start < qEnd && qStart < event.end
```

Therefore `[09:00, 09:55)` and `[09:55, 10:55)` do not overlap. A class ending at 09:55 does not clash with a class beginning at 09:55.

---

## 3. Main DSA: augmented interval tree

### What is an interval tree?

An interval tree is a binary search tree designed for intervals. It finds events overlapping a requested range without scanning every event whenever pruning is possible.

This implementation is an **unbalanced BST augmented with `maxEnd`**. It is not an AVL or red-black tree.

```cpp
struct Node {
    Event event;
    int maxEnd;
    Node* left;
    Node* right;
};
```

The BST ordering key is `event.start`:

- smaller start time goes left;
- equal or larger start time goes right.

`maxEnd` is extra information stored at every node:

```text
maxEnd(node) = max(node.event.end,
                   maxEnd(node.left),
                   maxEnd(node.right))
```

This is an **augmented data structure**: normal BST data is combined with derived subtree information to make queries faster.

### Why `maxEnd` is necessary

If the maximum end time in the left subtree is less than or equal to `qStart`, every interval in that subtree finishes before the query starts. The whole subtree can be skipped.

```cpp
if (root->left && root->left->maxEnd > start)
    overlapQuery(root->left, start, end, results);
```

One stored value gives information about all intervals below a node.

### Insertion

`insertNode` recursively follows the BST rule:

1. If the current pointer is null, allocate a new node.
2. Compare the new event's start with the current event's start.
3. Insert into the left or right subtree.
4. Recalculate `maxEnd` using `pull(root)`.
5. Return the root pointer because recursion may change a child pointer.

The invariant is: after every insertion, `maxEnd` is correct for every ancestor of the new node.

### Example

Insert these events in this order:

```text
A = [540, 595)  Monday 09:00-09:55
B = [600, 655)  Monday 10:00-10:55
C = [570, 625)  Monday 09:30-10:25
D = [840, 895)  Monday 14:00-14:55
```

The BST ordering is by start time. `C` becomes part of the left side of `B`, while `D` is to the right of events starting before it. The exact shape depends on insertion order; `maxEnd` is the maximum end time in each subtree, not just the current event.

### Overlap query

`overlapQuery(root, start, end, results)` returns every event overlapping `[start, end)`.

At each node it:

1. searches the left subtree only when `left.maxEnd > start`;
2. tests the current event with the half-open overlap formula;
3. searches the right subtree only when `root.event.start < end`.

The right condition is safe because every event in the right subtree has a start at least as large as the current node's start. If `root.start >= queryEnd`, the entire right subtree starts too late.

### `anyOverlap` versus `overlapQuery`

Both functions use the same pruning logic:

- `anyOverlap` stops as soon as it finds one conflict;
- `overlapQuery` continues and collects every conflicting event.

The first is useful for a yes/no check. The second is used when the API must display all conflicting classes.

### Delete and memory management

The intended BST deletion cases are:

- leaf: delete the node;
- one child: return the child to the parent;
- two children: copy the inorder successor, then delete that successor.

`freeTree` recursively deletes all nodes when the tree is rebuilt or the program exits, preventing memory leaks from `new Node(...)`.

### Important implementation observation

The tree is inserted using `event.start` as its BST key, but the current `deleteNode` searches using `event.id`. Those are different keys. Standard BST deletion must search using the same key used during insertion, or maintain a separate index by ID.

For a robust fix, delete by `(start, id)` or recursively search for the ID. This is a valuable limitation to mention in a viva. The supplied tests pass because their IDs and insertion order make the tested cases simple.

---

## 4. Dry run of an overlap query

Query `[615, 645)` (Monday 10:15-10:45) against the example events:

| Event | Overlap test | Result |
|---|---|---|
| `[540,595)` | `540 < 645` true, `615 < 595` false | no |
| `[600,655)` | both comparisons true | yes |
| `[570,625)` | both comparisons true | yes |
| `[840,895)` | `840 < 645` false | no |

The result contains `[600,655)` and `[570,625)`.

The two pruning rules are:

```text
left.maxEnd <= queryStart  -> left subtree cannot overlap
node.start >= queryEnd     -> right subtree cannot overlap
```

---

## 5. Scheduler algorithm: finding free time

The interval tree stores busy events. `freeGaps` finds free time for one day by:

1. querying events in the day's working range;
2. clipping them to the day boundaries;
3. storing their `(start, end)` pairs in a vector;
4. sorting intervals by start time;
5. merging overlapping or touching busy intervals;
6. sweeping from `DAY_START` with a `cursor`;
7. recording `[cursor, interval.start)` whenever there is a gap;
8. moving the cursor to the end of the merged interval;
9. recording the final gap up to `DAY_END`.

Example:

```text
Busy:   [09:00,09:55), [10:00,10:55), [10:30,11:30)
Merge:  [09:00,09:55), [10:00,11:30)
Free:   gaps before, between, and after these intervals
```

This is a classic **sort-and-sweep algorithm**. The cursor represents the end of all busy time processed so far.

If there are `m` events for the day, sorting costs `O(m log m)` and merging/sweeping costs `O(m)`. Sorting is the dominant step after collection.

---

## 6. Slot candidates and min-heap suggestions

The allowed slot starts are in `SLOT_STARTS`. For `n` consecutive slots:

```text
run length = n * 60 - 5 minutes
```

The five-minute reduction accounts for the transition between consecutive 55-minute periods.

`validSlotRuns` keeps a candidate only if:

- the complete run is inside the working day;
- it does not overlap lunch when lunch exclusion is enabled;
- it lies completely inside one free gap.

### Why a priority queue?

`suggest` uses a `priority_queue` configured as a min-heap with `greater<SlotCandidate>`.

Each candidate has a distance:

```cpp
distance = abs(candidateStart - preferredStart);
```

The smallest distance is at the top. Repeatedly calling `top()` and `pop()` returns the nearest `k` suggestions.

For `c` candidates:

- inserting all candidates: `O(c log c)`;
- removing `k` suggestions: `O(k log c)`;
- total: `O(c log c + k log c)`.

---

## 7. CSV parser and other useful concepts

The CSV loader is not the main DSA, but it contains useful algorithmic ideas.

### State machine for quoted CSV

`parseCSVLine` keeps an `inQuotes` Boolean:

- outside quotes, a comma ends the current field;
- inside quotes, a comma is ordinary text;
- `""` inside a quoted field represents one literal quote;
- a closing quote returns to normal mode.

This is a small finite-state machine. A simple `split(line, ',')` would fail when a field itself contains commas.

### Maps, sorting, and merging

The loader uses maps to associate a slot start and suffix with a role: room, subject, faculty, or ignore. `mergeEvents` sorts by batch and start time, then merges records with the same batch, subject, faculty, and room when the gap is at most five minutes.

For example:

```text
09:00-09:55 + 10:00-10:55 -> 09:00-10:55
```

This reduces event count and removes artificial gaps between adjacent periods of one class.

Room and faculty values are validated with regular expressions. If more than 10% of room cells fail validation, the loader reports a scraper-layout error instead of producing unreliable events.

---

## 8. Single mode and group mode

The selected events are inserted into one tree in both modes.

### Single mode

One student is assumed to attend all selected batches. Overlapping events from different selected batches are reported as clashes.

### Group mode

Different people may represent different batches. The union of busy intervals is used to find periods when everyone is free. An overlap between two different people's classes is not automatically a personal clash.

The same interval tree works for both modes; only the interpretation of the selected events changes.

---

## 9. Complexity summary

Let `n` be the number of events, `h` the tree height, and `k` the number of reported overlaps.

| Operation | Typical case | Worst case for this BST |
|---|---:|---:|
| Insert | `O(log n)` | `O(n)` |
| Any overlap | approximately `O(log n)` | `O(n)` |
| Report all overlaps | approximately `O(log n + k)` | `O(n)` |
| Delete | `O(log n)` if keyed correctly | `O(n)` |
| Sort-and-sweep free gaps | `O(m log m)` | `O(m log m)` |
| Heap suggestions | `O(c log c + k log c)` | same |

The tree is not self-balancing. Sorted insertion can turn it into a chain of height `n`, so a search may visit every node.

Possible improvements are an AVL tree, red-black tree, randomized treap, or another balanced interval-tree implementation. For a timetable with a few hundred events, the current structure is usually fast enough and easier to demonstrate.

---

## 10. Viva questions and short answers

### Q1. Why use an interval tree instead of a vector scan?

A vector scan checks every event for every query: `O(n)`. The interval tree uses BST ordering and `maxEnd` pruning, giving approximately `O(log n + k)` when the tree is reasonably shaped.

### Q2. What does `maxEnd` mean?

It is the maximum end time of any event in that node's subtree. It tells us whether that subtree can possibly reach the query start.

### Q3. Why is the overlap condition `start < end2 && start2 < end`?

It is the overlap test for half-open intervals and correctly treats touching intervals as non-overlapping.

### Q4. Explain the pruning conditions.

`left.maxEnd <= queryStart` means every left-subtree event ends too early. `node.start >= queryEnd` means every right-subtree event starts too late.

### Q5. What happens if two events have the same start time?

The new event goes right because the insertion code uses `<` for the left branch and otherwise chooses right. Both events remain stored.

### Q6. What are the three BST deletion cases?

Delete a leaf directly; replace a one-child node with its child; for two children, copy the inorder successor and delete that successor.

### Q7. Why call `pull()` after insertion or deletion?

Changing a subtree can change the maximum end time of every ancestor. `pull()` restores the `maxEnd` invariant on the path back to the root.

### Q8. How are free gaps found?

Collect busy intervals, sort them, merge overlaps, then sweep from the beginning of the day and record spaces between merged intervals.

### Q9. Why use a min-heap for suggestions?

The heap keeps the candidate with the smallest distance from the preferred time at the top, so the nearest suggestions can be removed efficiently.

### Q10. Why merge consecutive timetable slots?

They represent one continuous class. Merging reduces the number of events and prevents artificial gaps between adjacent periods.

### Q11. What is the biggest limitation of this interval tree?

It is unbalanced, so sorted insertion can make it a chain. Also, deletion should use the same ordering key as insertion; the current code inserts by start but searches for deletion by ID.

### Q12. How would you improve it?

Use a balanced interval tree or treap, and make deletion search by `(start, id)` or maintain a separate ID-to-node/index mapping.

---

## 11. How to present the project in the mid evaluation

Use this order when explaining it:

1. Each class is represented as a half-open time interval.
2. Selected events are stored in a BST ordered by start time.
3. Every node stores `maxEnd`, the maximum end time in its subtree.
4. During overlap queries, `maxEnd` allows impossible subtrees to be pruned.
5. Free time is found by sorting and merging busy intervals, then sweeping the gaps.
6. Nearest free slots are selected with a min-heap ordered by distance from the preferred time.
7. The tree is unbalanced, so its worst case is linear; a treap or AVL-style structure would improve it.

Simple demonstration:

```text
Existing class: [540,595)
New class:      [570,625)

540 < 625 and 570 < 595 -> overlap
```

Touching demonstration:

```text
[540,595) and [595,650)
540 < 650 is true, but 595 < 595 is false -> no overlap
```

These two examples demonstrate both the overlap formula and the reason for half-open intervals.
