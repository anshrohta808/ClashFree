# ClashFree - College Timetable Conflict Detector

A C++17 web application for detecting timetable conflicts and finding free slots.

## Project Structure

```
clashfree/
├── backend/
│   ├── config.h              # Constants (slot times, day bounds, etc.)
│   ├── scrape_layout.h       # Scraper column role mapping (inferred from data)
│   ├── interval_tree.h/cpp   # Augmented BST interval tree (DSA core)
│   ├── csv_loader.h/cpp      # RFC-4180 CSV parser + merger
│   ├── scheduler.h/cpp       # Free gaps, slot candidates, heap suggestions
│   ├── main.cpp              # HTTP server + API routes
│   ├── test_tree.cpp         # Interval tree unit tests
│   ├── test_csv.cpp          # CSV parser unit tests
│   ├── test_scheduler.cpp    # Scheduler unit tests
│   ├── httplib.h             # Single-header HTTP library
│   ├── json.hpp              # Single-header JSON library
│   └── data/
│       ├── timetable.csv     # Raw scraper export (25A12)
│       └── demo_extra.csv    # Two demo batches (DEMO-B1, DEMO-B2)
├── frontend/
│   ├── index.html            # Single-page UI
│   ├── style.css             # Styling
│   └── app.js                # Vanilla JS frontend logic
├── README.md
└── DSA_NOTES.md
```

## Build

### Prerequisites
- g++ with C++17 support
- pthread library (`-lpthread` on Linux, `-lws2_32` on Windows/MinGW)

### Build Server
```bash
cd backend
g++ -std=c++17 -O2 -pthread main.cpp interval_tree.cpp csv_loader.cpp scheduler.cpp -o clashfree
# On Windows/MinGW:
g++ -std=c++17 -O2 -pthread main.cpp interval_tree.cpp csv_loader.cpp scheduler.cpp -o clashfree.exe -lws2_32
```

### Build Tests
```bash
# Interval tree tests
g++ -std=c++17 -O2 test_tree.cpp interval_tree.cpp -o test_tree

# CSV loader tests
g++ -std=c++17 -O2 test_csv.cpp csv_loader.cpp interval_tree.cpp -o test_csv

# Scheduler tests
g++ -std=c++17 -O2 test_scheduler.cpp scheduler.cpp interval_tree.cpp csv_loader.cpp -o test_scheduler
```

## Run

### HTTP Server (default)
```bash
./clashfree
# Server runs on http://localhost:8080
# Auto-loads backend/data/timetable.csv if present
```

### Demo Mode
```bash
./clashfree --demo
# Loads timetable.csv + demo_extra.csv
# Pre-selects 25A12 in single mode
# Adds 2 personal events (one clashing)
```

### Console Mode (fallback)
```bash
./clashfree --console
# Text menu for batch selection, event management, free slots
```

## API Reference

All endpoints return JSON. POST bodies are JSON unless noted.

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/upload` | Upload raw CSV (text/plain body). Returns `{batches, skipped[], totalEvents}` or layout error. |
| GET | `/api/batches` | Ordered batch list. |
| POST | `/api/select` | `{batches:[], mode:"single"|"group"}`. Returns merged events + selectionClashes. |
| GET | `/api/events` | Merged list (selected batches + personal), sorted by start. |
| POST | `/api/events` | Add personal event. Body: `subject, type, day, start, end, faculty?, room?`. `start`/`end` = minutes of day (0-1439). Returns conflict + suggestions if clash. Use `?force=1` to add anyway. |
| DELETE | `/api/events/:id` | Delete personal event. 403 for timetable events. |
| GET | `/api/free?day=0-5&slots=N` | Free gaps + valid slot-run candidates for day. |
| GET | `/api/common-free?slots=N&preferStart=M` | Top 3 slot runs across Mon-Sat via heap. Default `preferStart` = Monday 09:00. |

### curl Examples

```bash
# Upload CSV
curl -X POST -H "Content-Type: text/plain" --data-binary @backend/data/timetable.csv http://localhost:8080/api/upload

# List batches
curl http://localhost:8080/api/batches

# Select batches (single mode)
curl -X POST -H "Content-Type: application/json" -d '{"batches":["25A12"],"mode":"single"}' http://localhost:8080/api/select

# Select batches (group mode)
curl -X POST -H "Content-Type: application/json" -d '{"batches":["25A12","DEMO-B1"],"mode":"group"}' http://localhost:8080/api/select

# Get merged events
curl http://localhost:8080/api/events

# Add personal event (clash detection)
curl -X POST -H "Content-Type: application/json" -d '{"subject":"Club Meeting","type":"lecture","day":0,"start":540,"end":595}' http://localhost:8080/api/events

# Add personal event forced
curl -X POST -H "Content-Type: application/json" -d '{"subject":"Club Meeting","type":"lecture","day":0,"start":540,"end":595}' "http://localhost:8080/api/events?force=1"

# Delete personal event
curl -X DELETE http://localhost:8080/api/events/123

# Free slots for Monday, 2-slot runs
curl "http://localhost:8080/api/free?day=0&slots=2"

# Common free slot for 2 slots
curl "http://localhost:8080/api/common-free?slots=2"
```

## CSV Format Notes

The parser expects the **raw web-scraper export** format:
- UTF-8 with BOM, CRLF line endings
- All fields quoted
- Named columns: `web_scraper_order`, `web_scraper_start_url`, `data`
- `batch` = query param in `web_scraper_start_url` (`.../?batch=25A12`)
- `data` = day (MON..SAT)
- Slot columns named like `9:00_am9:55_am`, `9:00_am9:55_am2`, etc.
  - Regex: `(\d{1,2}:\d{2}_(?:am|pm))(\d{1,2}:\d{2}_(?:am|pm))(\d*)`
  - Suffix 1..4 maps to different roles per slot start time

### Updating `scrape_layout.h`

If a new scrape changes column roles:
1. Inspect the raw CSV headers
2. Edit `SCRAPE_LAYOUT` map in `backend/scrape_layout.h`
3. Key = slot start time (`"9:00"`), value = map of suffix → `{role, required}`
4. Roles: `"room"`, `"subject"`, `"faculty"`, `"ignore"`
5. Rebuild and test

The parser validates room values against `^(CR|CL|LT|TR|DLC)`. If >10% fail, it returns a layout error.

## Time Model

- Day index 0..5 = MON..SAT
- Time = minutes since Monday 00:00
- College slots: 55-min periods starting at 09:00,10:00,11:00,12:00,13:00,14:00,15:00,16:00
- Lunch 12:00-12:55 excluded from suggestions
- N consecutive slots span N*60-5 minutes
- Intervals are half-open [start, end)

## DSA Implementation

See `DSA_NOTES.md` for:
- Interval tree dry run
- Complexity analysis
- Viva questions
- Design rationale