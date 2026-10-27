#include "httplib.h"
#include "json.hpp"
#include "interval_tree.h"
#include "csv_loader.h"
#include "scheduler.h"
#include "config.h"
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <queue>

using namespace std;
using json = nlohmann::json;

map<string, vector<Event>> batchEvents;
vector<string> batchOrder;
set<string> selectedBatches;
vector<Event> personalEvents;
Node* treeRoot = nullptr;
int nextEventId = 1;
string selectionMode = "single";

void rebuildTree() {
    freeTree(treeRoot);
    treeRoot = nullptr;
    
    for (const string& batch : selectedBatches) {
        auto it = batchEvents.find(batch);
        if (it != batchEvents.end()) {
            for (const Event& e : it->second) {
                Event copy = e;
                copy.id = nextEventId++;
                treeRoot = insertNode(treeRoot, copy);
            }
        }
    }
    
    for (const Event& e : personalEvents) {
        Event copy = e;
        copy.id = nextEventId++;
        treeRoot = insertNode(treeRoot, copy);
    }
}

json eventToJson(const Event& e) {
    json j;
    j["id"] = e.id;
    j["batch"] = e.batch;
    j["subject"] = e.subject;
    j["type"] = e.type;
    j["faculty"] = e.faculty;
    j["room"] = e.room;
    j["source"] = e.source;
    j["start"] = e.start;
    j["end"] = e.end;
    j["day"] = e.start / 1440;
    j["startMin"] = e.start % 1440;
    j["endMin"] = e.end % 1440;
    return j;
}

string minutesToTimeStr(int mins) {
    int hour = mins / 60;
    int minute = mins % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
    return string(buf);
}

string formatEventTime(const Event& e) {
    static const char* days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    int day = e.start / 1440;
    int startMin = e.start % 1440;
    int endMin = e.end % 1440;
    return string(days[day]) + " " + minutesToTimeStr(startMin) + "-" + minutesToTimeStr(endMin);
}

void loadCSVFile(const string& path) {
    ifstream file(path);
    if (!file) return;
    string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    
    ParseResult result = parseRawCSV(content);
    if (result.layoutError) {
        cerr << "Layout error: " << result.layoutErrorMsg << endl;
        return;
    }
    
    vector<Event> merged = mergeEvents(result.events);
    for (Event& e : merged) {
        e.id = 0;
    }
    
    string batch = merged.empty() ? "" : merged[0].batch;
    if (!batch.empty()) {
        if (batchEvents.find(batch) == batchEvents.end()) {
            batchOrder.push_back(batch);
        }
        batchEvents[batch] = merged;
    }
    
    for (const string& w : result.warnings) {
        cerr << "Warning: " << w << endl;
    }
}

int main(int argc, char* argv[]) {
    bool demoMode = false;
    bool consoleMode = false;
    
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--demo") demoMode = true;
        else if (arg == "--console") consoleMode = true;
    }
    
    loadCSVFile("backend/data/timetable.csv");
    if (demoMode) {
        loadCSVFile("backend/data/demo_extra.csv");
    }
    
    httplib::Server svr;
    svr.set_mount_point("/", "../frontend");
    svr.set_payload_max_length(10 * 1024 * 1024);
    
    svr.Post("/api/upload", [](const httplib::Request& req, httplib::Response& res) {
        ParseResult result = parseRawCSV(req.body);
        
        json response;
        if (result.layoutError) {
            response["error"] = result.layoutErrorMsg;
            res.set_content(response.dump(), "application/json");
            return;
        }
        
        vector<Event> merged = mergeEvents(result.events);
        string batch = merged.empty() ? "" : merged[0].batch;
        
        if (!batch.empty()) {
            if (batchEvents.find(batch) == batchEvents.end()) {
                batchOrder.push_back(batch);
            }
            for (Event& e : merged) {
                e.id = 0;
            }
            batchEvents[batch] = merged;
        }
        
        response["batches"] = batchOrder;
        response["skipped"] = result.warnings;
        response["totalEvents"] = (int)merged.size();
        res.set_content(response.dump(), "application/json");
    });
    
    svr.Get("/api/batches", [](const httplib::Request&, httplib::Response& res) {
        json response;
        response["batches"] = batchOrder;
        res.set_content(response.dump(), "application/json");
    });
    
    svr.Post("/api/select", [](const httplib::Request& req, httplib::Response& res) {
        json body = json::parse(req.body);
        selectedBatches.clear();
        if (body.contains("batches")) {
            for (const string& b : body["batches"]) {
                selectedBatches.insert(b);
            }
        }
        if (body.contains("mode")) {
            selectionMode = body["mode"];
        }
        
        rebuildTree();
        
        vector<Event*> allEvents;
        inorderCollect(treeRoot, allEvents);
        
        vector<json> eventsJson;
        vector<json> clashesJson;
        
        if (selectionMode == "single") {
            map<int, vector<Event*>> byBatch;
            for (Event* e : allEvents) {
                if (e->source == "timetable") {
                    byBatch[e->start / 1440].push_back(e);
                }
            }
            
            for (auto& kv : byBatch) {
                vector<Event*>& vec = kv.second;
                for (size_t i = 0; i < vec.size(); i++) {
                    for (size_t j = i + 1; j < vec.size(); j++) {
                        if (vec[i]->start < vec[j]->end && vec[j]->start < vec[i]->end) {
                            json clash;
                            clash["event1"] = eventToJson(*vec[i]);
                            clash["event2"] = eventToJson(*vec[j]);
                            clashesJson.push_back(clash);
                        }
                    }
                }
            }
        }
        
        for (Event* e : allEvents) {
            eventsJson.push_back(eventToJson(*e));
        }
        
        json response;
        response["events"] = eventsJson;
        response["selectionClashes"] = clashesJson;
        res.set_content(response.dump(), "application/json");
    });
    
    svr.Get("/api/events", [](const httplib::Request&, httplib::Response& res) {
        vector<Event*> allEvents;
        inorderCollect(treeRoot, allEvents);
        
        json response;
        vector<json> eventsJson;
        for (Event* e : allEvents) {
            eventsJson.push_back(eventToJson(*e));
        }
        response["events"] = eventsJson;
        res.set_content(response.dump(), "application/json");
    });
    
    svr.Post("/api/events", [](const httplib::Request& req, httplib::Response& res) {
        if (selectedBatches.empty()) {
            res.status = 400;
            json err; err["error"] = "No batch selected";
            res.set_content(err.dump(), "application/json");
            return;
        }
        
        json body = json::parse(req.body);
        bool force = req.has_param("force") && req.get_param_value("force") == "1";
        
        string subject = body.value("subject", "");
        string type = body.value("type", "lecture");
        string faculty = body.value("faculty", "");
        string room = body.value("room", "");
        int day = body.value("day", 0);
        int start = body.value("start", 0);
        int end = body.value("end", 0);
        
        if (subject.empty() || start >= end) {
            res.status = 400;
            json err; err["error"] = "Invalid event data";
            res.set_content(err.dump(), "application/json");
            return;
        }
        
        int absStart = day * 1440 + start;
        int absEnd = day * 1440 + end;
        
        vector<Event*> conflicts;
        overlapQuery(treeRoot, absStart, absEnd, conflicts);
        
        if (!conflicts.empty() && !force) {
            int duration = end - start;
            int slots = (duration + 5) / 60;
            if (slots < 1) slots = 1;
            vector<int> suggestions = suggest(treeRoot, absStart, slots, 3);
            
            json response;
            response["conflicts"] = json::array();
            for (Event* e : conflicts) {
                response["conflicts"].push_back(eventToJson(*e));
            }
            response["suggestions"] = json::array();
            for (int s : suggestions) {
                response["suggestions"].push_back(formatEventTime(Event{0, "", "", "", "", "", "", s, s + slots * 60 - 5}));
            }
            res.status = 409;
            res.set_content(response.dump(), "application/json");
            return;
        }
        
        Event e;
        e.id = nextEventId++;
        e.batch = *selectedBatches.begin();
        e.subject = subject;
        e.type = type;
        e.faculty = faculty;
        e.room = room;
        e.source = "personal";
        e.start = absStart;
        e.end = absEnd;
        
        personalEvents.push_back(e);
        treeRoot = insertNode(treeRoot, e);
        
        json response;
        response["event"] = eventToJson(e);
        res.set_content(response.dump(), "application/json");
    });
    
    svr.Delete(R"(/api/events/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        int id = stoi(req.matches[1]);
        
        auto it = find_if(personalEvents.begin(), personalEvents.end(), [id](const Event& e) {
            return e.id == id;
        });
        
        if (it == personalEvents.end()) {
            res.status = 403;
            json err; err["error"] = "Cannot delete timetable event";
            res.set_content(err.dump(), "application/json");
            return;
        }
        
        personalEvents.erase(it);
        treeRoot = deleteNode(treeRoot, id);
        
        json response;
        response["success"] = true;
        res.set_content(response.dump(), "application/json");
    });
    
    svr.Get("/api/free", [](const httplib::Request& req, httplib::Response& res) {
        int day = req.has_param("day") ? stoi(req.get_param_value("day")) : 0;
        int slots = req.has_param("slots") ? stoi(req.get_param_value("slots")) : 1;
        
        if (day < 0 || day > 5) day = 0;
        if (slots < 1) slots = 1;
        
        vector<FreeGap> gaps = freeGaps(treeRoot, day);
        vector<SlotCandidate> candidates = validSlotRuns(gaps, day, slots);
        
        json response;
        response["gaps"] = json::array();
        for (const FreeGap& g : gaps) {
            json gj;
            gj["start"] = g.start;
            gj["end"] = g.end;
            gj["startStr"] = formatEventTime(Event{0, "", "", "", "", "", "", g.start, g.start});
            gj["endStr"] = formatEventTime(Event{0, "", "", "", "", "", "", g.end, g.end});
            response["gaps"].push_back(gj);
        }
        
        response["candidates"] = json::array();
        for (const SlotCandidate& c : candidates) {
            json cj;
            cj["start"] = c.start;
            cj["end"] = c.end;
            cj["day"] = c.day;
            cj["startStr"] = formatEventTime(Event{0, "", "", "", "", "", "", c.start, c.start});
            cj["endStr"] = formatEventTime(Event{0, "", "", "", "", "", "", c.end, c.end});
            response["candidates"].push_back(cj);
        }
        
        res.set_content(response.dump(), "application/json");
    });
    
    svr.Get("/api/common-free", [](const httplib::Request& req, httplib::Response& res) {
        int slots = req.has_param("slots") ? stoi(req.get_param_value("slots")) : 1;
        int preferStart = req.has_param("preferStart") ? stoi(req.get_param_value("preferStart")) : 0 * 1440 + DAY_START;
        
        if (slots < 1) slots = 1;
        
        priority_queue<SlotCandidate, vector<SlotCandidate>, greater<SlotCandidate>> pq;
        int totalCandidates = 0;
        
        for (int day = 0; day < 6; day++) {
            vector<FreeGap> gaps = freeGaps(treeRoot, day);
            vector<SlotCandidate> candidates = validSlotRuns(gaps, day, slots);
            totalCandidates += candidates.size();
            for (const SlotCandidate& c : candidates) {
                int distance = abs(c.start - preferStart);
                pq.push({c.start, c.end, c.day, distance});
            }
        }
        
        json response;
        response["suggestions"] = json::array();
        int count = 0;
        while (!pq.empty() && count < 3) {
            SlotCandidate c = pq.top(); pq.pop();
            json sj;
            sj["start"] = c.start;
            sj["end"] = c.end;
            sj["day"] = c.day;
            sj["startStr"] = formatEventTime(Event{0, "", "", "", "", "", "", c.start, c.start});
            sj["endStr"] = formatEventTime(Event{0, "", "", "", "", "", "", c.end, c.end});
            response["suggestions"].push_back(sj);
            count++;
        }
        response["totalCandidates"] = totalCandidates;
        
        res.set_content(response.dump(), "application/json");
    });
    
    if (consoleMode) {
        cout << "Console mode - not fully implemented" << endl;
        return 0;
    }
    
    cout << "Server starting on http://localhost:8080" << endl;
    svr.listen("0.0.0.0", 8080);
    return 0;
}