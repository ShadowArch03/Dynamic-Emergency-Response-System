# Dynamic Emergency Response and Route Optimization System

A simulation-based smart-city emergency management system written in **pure C (C99)** with **zero external libraries** — built entirely on core Data Structures & Algorithms: **Graphs, Dijkstra's shortest path, Binary Heaps, and Generic Linked Lists**.

> Citizens report emergencies → the most critical patient is always served first (priority queue) → the nearest/fastest ambulance is dispatched (Dijkstra under live traffic) → the most suitable hospital is chosen (beds + ICU + reachability + minimum ETA) → and when roads change mid-journey, every ambulance **re-routes automatically from its current location**.

---

## Key Features

| Feature | How it works |
|---|---|
| **Severity-based dispatch** | Emergencies enter a binary max-heap priority queue — higher severity first, older emergencies break ties |
| **Optimal ambulance selection** | Dijkstra runs from every free ambulance's location; minimum-ETA ambulance wins |
| **Smart hospital selection** | Filter (emergency service, enough beds, ICU when needed) + rank by shortest live route; bed is **reserved at selection time** to prevent double-booking |
| **4-level live traffic** | NORMAL (x1), TRAFFIC (x2), HEAVY (x3), BLOCKED (invisible to routing) |
| **Dynamic re-routing** | When a road blocks or a faster path appears, ambulances re-route from their **current node** — never the original start. Equal-cost routes are kept to prevent flip-flopping |
| **Hospital re-selection** | If a hospital becomes completely unreachable, its bed is refunded and a new hospital is selected mid-journey |
| **Role-based terminal UI** | ADMIN (full control), CITIZEN (report & track), DRIVER (briefing, route, report road problems) with 3-attempt login lockout |
| **Live ANSI city map** | Colour-coded roads, active routes (`*`), emergencies and interpolated ambulance positions |
| **Turn-based simulation** | 1 tick = 1 minute: clock → dispatch → re-route → movement |
| **Scripted demo mode** | 7 scenarios covering every feature (`./build/ers --demo`) |
| **Automated tests** | 5 unit/integration suites, all passing (`ctest`) |

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Layer 4 — Presentation:  auth.c · map.c · main.c       │  (reads World, never mutates)
├─────────────────────────────────────────────────────────┤
│  Layer 3 — Engine:  dispatch.c · simulation.c · report  │  (decision making, tick loop)
├─────────────────────────────────────────────────────────┤
│  Layer 2 — Managers:  road · hospital · ambulance ·     │  (domain rules, state machines)
│                       emergency                         │
├─────────────────────────────────────────────────────────┤
│  Layer 1 — Core DSA:  graph · dijkstra ·                │  (the data structures)
│                       priority_queue · linked_list      │
└─────────────────────────────────────────────────────────┘
        City data loaded from CSV:  data/*.csv
```

The UI layer only **reads** the shared `World` state, so a graphical interface (Phase 2, raylib) can be plugged in with **zero engine changes**.

## Data Structures Used

| Structure | Implementation | Used for |
|---|---|---|
| **Graph** | Undirected weighted adjacency list (`nodes[]` array + linked edge lists, id = index for O(1) lookup) | The city: 15 nodes, 25 roads |
| **Dijkstra + Binary Min-Heap** | O((V+E) log V); blocked edges skipped entirely; `prev[]` path reconstruction | Route finding for ambulances and hospital selection |
| **Priority Queue** | Binary max-heap; comparator = severity DESC, then age ASC | Emergency ordering |
| **Linked List** | Generic `void*` list | Registries: roads, hospitals, ambulances, emergencies, report log |

---

## Build & Run

Requirements: any C99 compiler + CMake.

```bash
# from the project root
cmake -B build
cmake --build build

# interactive application (run from project root — loads data/*.csv)
./build/ers

# scripted demo: 7 scenarios, no interaction needed
./build/ers --demo

# run the test suites
ctest --test-dir build
```

### Login Roles

| Role | Login | Capabilities |
|---|---|---|
| **Admin** | password: `admin` (3 attempts, then lockout) | View map/roads/hospitals/ambulances, update traffic, advance time, view reports |
| **Citizen** | any name | Create emergencies, track status |
| **Driver** | ambulance id | Assignment briefing, route instructions, report road problems (auto re-route) |

> The simulation is **turn-based**: time advances only when Admin uses *Advance Time*. One tick = one minute.

---

## Project Structure

```
├── include/          15 headers
├── src/
│   ├── Core DSA:     graph.c · dijkstra.c · priority_queue.c · linked_list.c
│   ├── Managers:     road.c · hospital.c · ambulance.c · emergency.c
│   ├── Engine:       dispatch.c · simulation.c · report.c
│   └── UI:           auth.c · map.c · main.c
├── data/             nodes.csv · roads.csv · hospitals.csv · ambulances.csv · emergencies.csv
├── tests/            5 test suites (graph, dijkstra, priority queue, managers, dispatch)
├── CMakeLists.txt
└── run.sh
```

## Testing

```bash
ctest --test-dir build
```

| Suite | Covers |
|---|---|
| test_graph | Node/edge insertion, bidirectional weights, blocked-edge sync |
| test_dijkstra | Shortest path, alternate route on blockage, unreachable detection |
| test_priority_queue | Severity ordering, age tie-break, 100-item stress test |
| test_managers | Traffic cycle, hospital candidate rules, route ownership |
| test_dispatch | Best-route selection, mid-journey re-route, hospital fallback, ICU matching |

All 5 suites pass.

---

## Roadmap

- [x] **Phase 1** — Complete engine, DSA layer, role-based terminal UI, live map, traffic system, tests, demo mode *(done)*
- [ ] **Phase 2** — raylib GUI with animated ambulances, FIFO event queue, ambulance maintenance, emergency cancellation, auto-dispatch toggle


