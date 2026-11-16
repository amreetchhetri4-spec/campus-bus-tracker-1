// Graphic Era Hill University - Campus Bus Tracker (C++17, console)
// DSA used: min-heap (priority queue), hash table (chaining), graph + Dijkstra,
// circular queue, merge sort, binary search.
// Compile: g++ -std=c++17 -o bus_tracker bus_tracker.cpp
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <climits>
using namespace std;

// ---------- Data (placeholders: replace with your real stops, routes, buses) ----------
const int N = 11;
const char* STOP[N] = {"Campus Gate", "Clement Town", "ISBT", "Railway Station", "Ghanta Ghar",
                       "Rajpur Road", "Prem Nagar", "Ballupur Chowk", "Rispana Bridge",
                       "Mall of Dehradun", "Paltan Bazaar"};
enum { AV, ON, FULL, OFF, MAINT };
const char* STATUS[] = {"Available", "On route", "Full", "Off campus", "Maintenance"};
struct Bus { string id; int dest, st, load, eta; };   // eta = minutes until it leaves the gate
vector<Bus> B = {
    {"GEHU-101", 1, AV, 62, 4},   {"GEHU-102", 6, ON, 39, 8},  {"GEHU-103", 7, FULL, 100, 13},
    {"GEHU-104", 4, AV, 48, 6},   {"GEHU-105", 2, OFF, 0, 0},  {"GEHU-106", 1, AV, 25, 10},
    {"GEHU-107", 6, FULL, 100, 17}, {"GEHU-108", 7, ON, 74, 12}, {"GEHU-109", 4, AV, 55, 15},
    {"GEHU-110", 2, MAINT, 0, 0}, {"GEHU-111", 3, AV, 35, 5},  {"GEHU-112", 5, ON, 58, 9}};
int now = 8 * 60;   // clock in minutes after midnight

// ---------- 1. Min-heap ----------
struct MinHeap {
    vector<pair<int, int>> a;   // (key, value)
    bool empty() const { return a.empty(); }
    void push(int k, int v) {
        a.push_back({k, v});
        int i = a.size() - 1;
        while (i > 0 && a[(i - 1) / 2].first > a[i].first) { swap(a[i], a[(i - 1) / 2]); i = (i - 1) / 2; }
    }
    pair<int, int> pop() {
        pair<int, int> top = a[0];
        a[0] = a.back(); a.pop_back();
        int i = 0, n = a.size();
        while (true) {
            int l = 2 * i + 1, r = l + 1, m = i;
            if (l < n && a[l].first < a[m].first) m = l;
            if (r < n && a[r].first < a[m].first) m = r;
            if (m == i) break;
            swap(a[i], a[m]); i = m;
        }
        return top;
    }
};

// ---------- 2. Hash table with separate chaining (bus id -> index in B) ----------
struct HashMap {
    struct Node { string key; int val; Node* next; };
    static const int M = 31;
    Node* t[M] = {};
    int h(const string& s) const { unsigned x = 5381; for (char c : s) x = x * 33 + c; return x % M; }
    void put(const string& k, int v) {
        int i = h(k);
        for (Node* p = t[i]; p; p = p->next) if (p->key == k) { p->val = v; return; }
        t[i] = new Node{k, v, t[i]};
    }
    int get(const string& k) const {
        for (Node* p = t[h(k)]; p; p = p->next) if (p->key == k) return p->val;
        return -1;
    }
};
HashMap busIndex;

// ---------- 3. Graph (adjacency list) + Dijkstra ----------
vector<pair<int, int>> g[N];   // (neighbour, minutes)
void addRoad(int a, int b, int w) { g[a].push_back({b, w}); g[b].push_back({a, w}); }
vector<int> dijkstra(int src, vector<int>& prev) {
    vector<int> d(N, INT_MAX);
    prev.assign(N, -1);
    MinHeap h; d[src] = 0; h.push(0, src);
    while (!h.empty()) {
        auto [du, u] = h.pop();
        if (du > d[u]) continue;
        for (auto& e : g[u]) {
            int v = e.first, w = e.second;
            if (d[u] + w < d[v]) { d[v] = d[u] + w; prev[v] = u; h.push(d[v], v); }
        }
    }
    return d;
}
void printPath(const vector<int>& prev, int v) {
    if (prev[v] != -1) { printPath(prev, prev[v]); cout << " -> "; }
    cout << STOP[v];
}

// ---------- 4. Circular queue (notifications waiting to be shown) ----------
struct CircularQueue {
    static const int CAP = 50;
    string a[CAP]; int head = 0, count = 0;
    bool push(const string& s) { if (count == CAP) return false; a[(head + count++) % CAP] = s; return true; }
    bool pop(string& s) { if (!count) return false; s = a[head]; head = (head + 1) % CAP; count--; return true; }
};
CircularQueue notes;

// ---------- 5. Merge sort (sort bus indices by key) ----------
void mergeSort(vector<int>& v, const vector<int>& key, int lo, int hi) {
    if (hi - lo < 2) return;
    int mid = (lo + hi) / 2;
    mergeSort(v, key, lo, mid); mergeSort(v, key, mid, hi);
    vector<int> tmp; int i = lo, j = mid;
    while (i < mid && j < hi) tmp.push_back(key[v[i]] <= key[v[j]] ? v[i++] : v[j++]);
    while (i < mid) tmp.push_back(v[i++]);
    while (j < hi) tmp.push_back(v[j++]);
    for (int k = 0; k < (int)tmp.size(); k++) v[lo + k] = tmp[k];
}

// ---------- 6. Binary search over the sorted timetable ----------
vector<int> TT;   // departure times, sorted
void buildTimetable() {
    for (int t = 480; t < 720; t += 20) TT.push_back(t);    // 8:00 - 12:00
    for (int t = 840; t < 960; t += 20) TT.push_back(t);    // 2:00 - 4:00 PM
    TT.push_back(1080);                                     // last bus 6:00 PM
}
int nextDeparture(int t, bool& tomorrow) {   // first departure >= t
    int lo = 0, hi = TT.size();
    while (lo < hi) { int mid = (lo + hi) / 2; if (TT[mid] < t) lo = mid + 1; else hi = mid; }
    tomorrow = (lo == (int)TT.size());
    return tomorrow ? TT[0] : TT[lo];
}

// ---------- Service hours ----------
int svcState(int t) {   // 0 open, 1 midday break, 2 last bus pending, 3 closed
    if ((t >= 480 && t < 720) || (t >= 840 && t < 960)) return 0;
    if (t >= 720 && t < 840) return 1;
    if (t >= 960 && t < 1080) return 2;
    return 3;
}
string hhmm(int t) {
    int h = t / 60 % 24, m = t % 60;
    char b[16]; snprintf(b, sizeof b, "%02d:%02d", h, m);
    return b;
}
bool running(const Bus& b) { return b.st <= FULL && svcState(now) == 0; }

// Best bus to a destination: earliest non-full bus (min-heap), else earliest full bus.
int nextBus(int dest) {
    MinHeap ok, full;
    for (int i = 0; i < (int)B.size(); i++)
        if (B[i].dest == dest && running(B[i])) (B[i].st == FULL ? full : ok).push(B[i].eta, i);
    if (!ok.empty()) return ok.pop().second;
    if (!full.empty()) return full.pop().second;
    return -1;
}

// ---------- Alerts ----------
struct Alert { int dest, lead; bool fired; };
vector<Alert> alerts;
void checkAlerts() {
    if (svcState(now) == 0)
        for (auto& a : alerts) {
            int b = nextBus(a.dest);
            if (b < 0) continue;
            int thr = max(a.lead, 1);
            if (B[b].eta <= thr) {
                if (!a.fired) {
                    a.fired = true;
                    string m = a.lead ? "Bus " + B[b].id + " to " + STOP[a.dest] + " is about " + to_string(B[b].eta) + " min away."
                                      : "Bus " + B[b].id + " to " + STOP[a.dest] + " is arriving now.";
                    notes.push(m);
                }
            } else if (B[b].eta > thr + 2) a.fired = false;
        }
    string m;
    while (notes.pop(m)) cout << "  [ALERT " << hhmm(now) << "] " << m << "\n";
}

// ---------- Actions ----------
void advance(int mins) {
    for (int s = 0; s < mins; s++) {
        now++;
        if (svcState(now) != 0) continue;
        for (size_t i = 0; i < B.size(); i++) {
            if (B[i].st > FULL) continue;
            if (--B[i].eta <= 0) {            // bus left the gate; next trip comes later
                B[i].eta = 12 + (i * 3) % 9;
                B[i].load = (B[i].load * 7 + 23) % 100;
                B[i].st = B[i].load > 60 ? ON : AV;
            }
        }
        checkAlerts();
    }
}
void showService() {
    int s = svcState(now);
    cout << "Time " << hhmm(now) << " (India time)\n";
    const char* msg[] = {"Buses are running now.", "Midday break. Buses resume at 14:00.",
                         "Regular service has ended. Last bus leaves campus at 18:00.",
                         "Closed. Service starts at 08:00."};
    cout << msg[s] << "\n";
    bool tom; int nd = nextDeparture(now, tom);
    cout << "Next scheduled departure: " << hhmm(nd) << (tom ? " (tomorrow)" : "") << "\n";
}
void table() {
    vector<int> idx, key;
    for (int i = 0; i < (int)B.size(); i++) { idx.push_back(i); key.push_back(running(B[i]) ? B[i].eta : 9999); }
    mergeSort(idx, key, 0, idx.size());
    cout << left << setw(10) << "BUS" << setw(18) << "DESTINATION" << setw(8) << "ETA" << setw(8) << "FULL" << "STATUS\n";
    for (int i : idx) {
        bool r = running(B[i]);
        cout << left << setw(10) << B[i].id << setw(18) << STOP[B[i].dest]
             << setw(8) << (r ? to_string(B[i].eta) + " min" : "-")
             << setw(8) << (r ? to_string(B[i].load) + "%" : "-")
             << ((r || B[i].st == MAINT) ? STATUS[B[i].st] : "Off hours") << "\n";
    }
}
int pickDest() {
    for (int i = 1; i < N; i++) cout << (i < 10 ? " " : "") << i << ". " << STOP[i] << "\n";
    cout << "Choose destination: ";
    int d; if (!(cin >> d) || d < 1 || d >= N) { cin.clear(); cin.ignore(1000, '\n'); return -1; }
    return d;
}
void nextBusCmd() {
    int d = pickDest(); if (d < 0) { cout << "Invalid choice.\n"; return; }
    int b = nextBus(d);
    if (b < 0) { cout << "No bus to " << STOP[d] << " right now.\n"; showService(); return; }
    vector<int> prev; vector<int> dist = dijkstra(0, prev);
    cout << "Next bus: " << B[b].id << " leaves in " << B[b].eta << " min, " << B[b].load << "% full ("
         << (B[b].load >= 100 ? "no seats" : B[b].load > 70 ? "seats limited" : "seats likely") << ")\n"
         << "Ride time " << dist[d] << " min, arriving at " << STOP[d] << " about " << hhmm(now + B[b].eta + dist[d]) << "\n";
}
void routeCmd() {
    int d = pickDest(); if (d < 0) { cout << "Invalid choice.\n"; return; }
    vector<int> prev; vector<int> dist = dijkstra(0, prev);
    cout << "Fastest route (" << dist[d] << " min): "; printPath(prev, d); cout << "\n";
}
void searchCmd() {
    cout << "Bus number (e.g. 103 or GEHU-103): ";
    string s; cin >> s;
    bool digits = !s.empty(); for (char c : s) if (c < '0' || c > '9') digits = false;
    if (digits) s = "GEHU-" + s;
    int i = busIndex.get(s);
    if (i < 0) { cout << "No bus with that number.\n"; return; }
    cout << B[i].id << " -> " << STOP[B[i].dest] << ", " << (running(B[i]) ? to_string(B[i].eta) + " min" : "not running")
         << ", " << B[i].load << "% full, " << STATUS[B[i].st] << "\n";
}
void alertCmd() {
    int d = pickDest(); if (d < 0) { cout << "Invalid choice.\n"; return; }
    cout << "Alert when it arrives (0) or minutes before (2, 5, 10): ";
    int lead; if (!(cin >> lead) || lead < 0 || lead > 60) { cin.clear(); cin.ignore(1000, '\n'); cout << "Invalid.\n"; return; }
    for (auto& a : alerts) if (a.dest == d) { a.lead = lead; a.fired = false; checkAlerts(); cout << "Alert updated for " << STOP[d] << ".\n"; return; }
    alerts.push_back({d, lead, false});
    cout << "Alert set for " << STOP[d] << ".\n";
    checkAlerts();
}

int main() {
    addRoad(0, 1, 5);  addRoad(0, 6, 8);  addRoad(0, 7, 10); addRoad(1, 2, 12); addRoad(7, 8, 6);
    addRoad(8, 4, 8);  addRoad(4, 3, 5);  addRoad(4, 10, 3); addRoad(3, 2, 7);  addRoad(4, 5, 9);
    addRoad(5, 9, 6);  addRoad(6, 7, 5);  addRoad(2, 4, 10);
    for (int i = 0; i < (int)B.size(); i++) busIndex.put(B[i].id, i);
    buildTimetable();

    cout << "Graphic Era Hill University - Bus Tracker (demo data)\n";
    cout << "Enter the current time as HH MM (24-hour), e.g. 9 30: ";
    int h, m; if (cin >> h >> m && h >= 0 && h < 24 && m >= 0 && m < 60) now = h * 60 + m; else { cin.clear(); cin.ignore(1000, '\n'); }
    int c = -1;
    while (c != 0) {
        cout << "\n1 Service status   2 Next bus to a destination   3 All buses   4 Search bus\n"
                "5 Fastest route    6 Set alert                   7 Move clock forward   0 Exit\n> ";
        if (!(cin >> c)) { cin.clear(); cin.ignore(1000, '\n'); c = -1; cout << "Enter a number.\n"; continue; }
        switch (c) {
            case 1: showService(); break;
            case 2: nextBusCmd(); break;
            case 3: table(); break;
            case 4: searchCmd(); break;
            case 5: routeCmd(); break;
            case 6: alertCmd(); break;
            case 7: { cout << "Minutes to move forward: "; int k; if (cin >> k && k > 0 && k <= 600) { advance(k); cout << "Time is now " << hhmm(now) << "\n"; } else { cin.clear(); cin.ignore(1000, '\n'); cout << "Enter 1-600.\n"; } break; }
            case 0: cout << "Goodbye.\n"; break;
            default: cout << "Choose 0-7.\n";
        }
    }
    return 0;
}
