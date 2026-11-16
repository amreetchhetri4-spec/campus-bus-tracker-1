// Graphic Era Hill University - Bus Tracker SERVER (C++17)
// Runs the DSA logic (min-heap, hash table, graph + Dijkstra, circular queue-free API,
// merge sort, binary search) and serves it as JSON so the website can call it.
//
// Build (Linux/macOS): g++ -std=c++17 -O2 -o bus_server bus_server.cpp
// Build (Windows/MinGW): g++ -std=c++17 -O2 -o bus_server.exe bus_server.cpp -lws2_32
// Run:  ./bus_server            (http://127.0.0.1:8080, this computer only)
//       ./bus_server 8080 0.0.0.0   (reachable from other devices)
//
// Endpoints:  /api/status   /api/stops   /api/buses   /api/bus?id=GEHU-103
//             /api/nextbus?dest=0   /api/route?dest=0
// "dest" numbers match the website's destination cards (0 = Nepali Farm ... 23 = Maldevta).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET sock_t;
  #define CLOSESOCK closesocket
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  typedef int sock_t;
  #define CLOSESOCK close
  #define INVALID_SOCKET (-1)
#endif
using namespace std;

// ---------------- Data (placeholders: replace with your real stops, roads, buses) ----------------
const int ND = 24;                       // destinations (website cards)
const int NN = ND + 1;                   // graph nodes: 0 = campus gate, d+1 = destination d
const char* DEST[ND] = {"Nepali Farm", "Garhi Cantt", "Bangali Kothi", "Ghanta Ghar", "ISBT", "Railway Station",
    "Clement Town", "Rajpur Road", "Prem Nagar", "Ballupur Chowk", "Paltan Bazaar", "Rispana Bridge", "Dalanwala",
    "Mall of Dehradun", "Sahastradhara Road", "Majra", "Forest Research Institute", "Saharanpur Chowk", "Dharampur",
    "Race Course", "Jolly Grant Airport", "Selaqui", "IT Park", "Maldevta"};
const char* GATE[ND] = {"Gate 2", "Gate 1", "Gate 3", "Gate 1", "Gate 3", "Gate 2", "Gate 1", "Gate 3", "Gate 2", "Gate 1",
    "Gate 1", "Gate 2", "Gate 3", "Gate 2", "Gate 3", "Gate 1", "Gate 1", "Gate 3", "Gate 2", "Gate 1", "Gate 3", "Gate 2", "Gate 3", "Gate 1"};
enum { AV, ON, FULL, OFF, MAINT };
const char* CODE[] = {"av", "on", "fu", "of", "mt"};   // same codes the website uses
struct Bus { string id; int dest, st, load; double etaSec; };   // etaSec = simulated seconds until it leaves the gate
vector<Bus> B = {
 {"GEHU-101",0,AV,62,240},{"GEHU-102",1,ON,39,480},{"GEHU-103",2,FULL,100,780},{"GEHU-104",3,AV,48,360},{"GEHU-105",4,OFF,0,0},
 {"GEHU-106",0,AV,25,600},{"GEHU-107",1,FULL,100,1020},{"GEHU-108",2,ON,74,720},{"GEHU-109",3,AV,55,900},{"GEHU-110",4,MAINT,0,0},
 {"GEHU-111",5,AV,35,300},{"GEHU-112",6,ON,58,540},{"GEHU-113",7,AV,20,660},{"GEHU-114",8,AV,44,420},{"GEHU-115",9,ON,80,840},
 {"GEHU-116",5,AV,66,1080},{"GEHU-117",6,AV,30,960},{"GEHU-118",10,AV,28,360},{"GEHU-119",11,ON,52,600},{"GEHU-120",12,AV,40,480},
 {"GEHU-121",13,AV,33,720},{"GEHU-122",14,ON,71,540},{"GEHU-123",15,AV,18,840},{"GEHU-124",16,AV,36,420},{"GEHU-125",17,ON,64,660},
 {"GEHU-126",18,AV,22,540},{"GEHU-127",19,AV,47,780},{"GEHU-128",20,ON,58,1140},{"GEHU-129",21,AV,31,900},{"GEHU-130",22,AV,27,480},
 {"GEHU-131",23,ON,69,720}};
const double SPEED = 6.0;   // demo: 1 real second = 6 simulated seconds

// ---------------- 1. Min-heap ----------------
struct MinHeap {
    vector<pair<int,int>> a;
    bool empty() const { return a.empty(); }
    void push(int k, int v) {
        a.push_back({k, v}); int i = a.size() - 1;
        while (i > 0 && a[(i-1)/2].first > a[i].first) { swap(a[i], a[(i-1)/2]); i = (i-1)/2; }
    }
    pair<int,int> pop() {
        pair<int,int> top = a[0]; a[0] = a.back(); a.pop_back();
        int i = 0, n = a.size();
        for (;;) {
            int l = 2*i+1, r = l+1, m = i;
            if (l < n && a[l].first < a[m].first) m = l;
            if (r < n && a[r].first < a[m].first) m = r;
            if (m == i) break;
            swap(a[i], a[m]); i = m;
        }
        return top;
    }
};

// ---------------- 2. Hash table (separate chaining): bus id -> index ----------------
struct HashMap {
    struct Node { string key; int val; Node* next; };
    static const int M = 61;
    Node* t[M] = {};
    int h(const string& s) const { unsigned x = 5381; for (char c : s) x = x * 33 + (unsigned char)c; return x % M; }
    void put(const string& k, int v) {
        int i = h(k);
        for (Node* p = t[i]; p; p = p->next) if (p->key == k) { p->val = v; return; }
        t[i] = new Node{k, v, t[i]};
    }
    int get(const string& k) const {
        for (Node* p = t[h(k)]; p; p = p->next) if (p->key == k) return p->val;
        return -1;
    }
} busIndex;

// ---------------- 3. Graph + Dijkstra (campus gate -> every stop) ----------------
vector<pair<int,int>> G[NN];
void road(int a, int b, int w) { G[a].push_back({b, w}); G[b].push_back({a, w}); }
int distTo[NN], prevN[NN];
void dijkstra(int src) {
    for (int i = 0; i < NN; i++) { distTo[i] = INT_MAX; prevN[i] = -1; }
    MinHeap h; distTo[src] = 0; h.push(0, src);
    while (!h.empty()) {
        pair<int,int> p = h.pop(); int du = p.first, u = p.second;
        if (du > distTo[u]) continue;
        for (auto& e : G[u]) if (distTo[u] + e.second < distTo[e.first]) {
            distTo[e.first] = distTo[u] + e.second; prevN[e.first] = u; h.push(distTo[e.first], e.first);
        }
    }
}
const char* nodeName(int n) { return n == 0 ? "Campus Gate" : DEST[n - 1]; }

// ---------------- 4. Merge sort (bus indices by key) ----------------
void mergeSort(vector<int>& v, const vector<int>& key, int lo, int hi) {
    if (hi - lo < 2) return;
    int mid = (lo + hi) / 2; mergeSort(v, key, lo, mid); mergeSort(v, key, mid, hi);
    vector<int> tmp; int i = lo, j = mid;
    while (i < mid && j < hi) tmp.push_back(key[v[i]] <= key[v[j]] ? v[i++] : v[j++]);
    while (i < mid) tmp.push_back(v[i++]);
    while (j < hi) tmp.push_back(v[j++]);
    for (size_t k = 0; k < tmp.size(); k++) v[lo + k] = tmp[k];
}

// ---------------- 5. Binary search over the timetable ----------------
vector<int> TT;
void buildTimetable() {
    for (int t = 480; t < 720; t += 20) TT.push_back(t);
    for (int t = 840; t < 960; t += 20) TT.push_back(t);
    TT.push_back(1080);
}
int nextDeparture(int t, bool& tomorrow) {
    int lo = 0, hi = TT.size();
    while (lo < hi) { int mid = (lo + hi) / 2; if (TT[mid] < t) lo = mid + 1; else hi = mid; }
    tomorrow = (lo == (int)TT.size());
    return tomorrow ? TT[0] : TT[lo];
}

// ---------------- Clock + service hours (India time) ----------------
int istMinutes() { long s = (long)time(nullptr) + 19800; return (int)((s / 60) % 1440); }
int svcState(int t) {   // 0 open, 1 midday break, 2 last bus pending, 3 closed
    if ((t >= 480 && t < 720) || (t >= 840 && t < 960)) return 0;
    if (t >= 720 && t < 840) return 1;
    if (t >= 960 && t < 1080) return 2;
    return 3;
}
string hhmm(int t) { char b[16]; snprintf(b, sizeof b, "%02d:%02d", t / 60 % 24, t % 60); return b; }
bool running(const Bus& b) { return b.st <= FULL && svcState(istMinutes()) == 0; }

// Move simulated buses forward by the real time that has passed.
double lastTick = 0;
void updateBuses() {
    double nowS = (double)time(nullptr);
    if (lastTick == 0) lastTick = nowS;
    double dt = (nowS - lastTick) * SPEED; lastTick = nowS;
    if (svcState(istMinutes()) != 0) return;
    for (size_t i = 0; i < B.size(); i++) {
        if (B[i].st > FULL) continue;
        B[i].etaSec -= dt;
        while (B[i].etaSec <= 0) {          // bus left the gate; next trip comes later
            B[i].etaSec += (12 + (i * 3) % 9) * 60;
            B[i].load = (B[i].load * 7 + 23) % 100;
            B[i].st = B[i].load > 60 ? ON : AV;
        }
    }
}
int etaMin(const Bus& b) { return (int)((b.etaSec + 59) / 60); }
int nextBus(int dest) {      // earliest non-full bus, else earliest full bus (min-heap)
    MinHeap ok, full;
    for (int i = 0; i < (int)B.size(); i++)
        if (B[i].dest == dest && running(B[i])) (B[i].st == FULL ? full : ok).push((int)B[i].etaSec, i);
    if (!ok.empty()) return ok.pop().second;
    if (!full.empty()) return full.pop().second;
    return -1;
}

// ---------------- JSON helpers ----------------
string q(const string& s) { return "\"" + s + "\""; }
string busJson(const Bus& b) {
    bool r = running(b);
    return "{\"id\":" + q(b.id) + ",\"dest\":" + to_string(b.dest) + ",\"destName\":" + q(DEST[b.dest]) +
           ",\"gate\":" + q(GATE[b.dest]) + ",\"status\":" + q(CODE[b.st]) + ",\"load\":" + to_string(r ? b.load : 0) +
           ",\"running\":" + (r ? "true" : "false") + ",\"etaSec\":" + to_string(r ? (int)b.etaSec : 0) +
           ",\"etaMin\":" + to_string(r ? etaMin(b) : -1) + "}";
}
string param(const string& query, const string& key) {
    size_t p = 0;
    while (p < query.size()) {
        size_t e = query.find('&', p); if (e == string::npos) e = query.size();
        string kv = query.substr(p, e - p); size_t eq = kv.find('=');
        if (eq != string::npos && kv.substr(0, eq) == key) return kv.substr(eq + 1);
        p = e + 1;
    }
    return "";
}
bool parseDest(const string& query, int& d) {
    string s = param(query, "dest");
    if (s.empty() || s.size() > 2) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    d = atoi(s.c_str()); return d >= 0 && d < ND;
}

// ---------------- Routing of requests ----------------
string handle(const string& path, const string& query, int& code) {
    code = 200; updateBuses();
    if (path == "/api/status") {
        int t = istMinutes(), s = svcState(t); bool tom; int nd = nextDeparture(t, tom);
        const char* msg[] = {"Buses are running now.", "Midday break. Buses resume at 2:00 PM.",
            "Regular service has ended. The last bus leaves campus at 6:00 PM.", "Closed. Service starts at 8:00 AM."};
        return "{\"time\":" + q(hhmm(t)) + ",\"open\":" + (s == 0 ? "true" : "false") + ",\"state\":" + to_string(s) +
               ",\"message\":" + q(msg[s]) + ",\"nextDeparture\":" + q(hhmm(nd)) + ",\"tomorrow\":" + (tom ? "true" : "false") + "}";
    }
    if (path == "/api/stops") {
        string o = "[";
        for (int i = 0; i < ND; i++) o += string(i ? "," : "") + "{\"dest\":" + to_string(i) + ",\"name\":" + q(DEST[i]) + ",\"gate\":" + q(GATE[i]) + "}";
        return o + "]";
    }
    if (path == "/api/buses") {
        vector<int> idx, key;
        for (int i = 0; i < (int)B.size(); i++) { idx.push_back(i); key.push_back(running(B[i]) ? (int)B[i].etaSec : INT_MAX); }
        mergeSort(idx, key, 0, idx.size());
        string o = "[";
        for (size_t k = 0; k < idx.size(); k++) o += (k ? "," : "") + busJson(B[idx[k]]);
        return o + "]";
    }
    if (path == "/api/bus") {
        string id = param(query, "id"), full = id;
        bool digits = !id.empty(); for (char c : id) if (c < '0' || c > '9') digits = false;
        if (digits) full = "GEHU-" + id;
        int i = busIndex.get(full);
        if (i < 0) { code = 404; return "{\"error\":\"No bus with that number\"}"; }
        return busJson(B[i]);
    }
    if (path == "/api/nextbus" || path == "/api/route") {
        int d; if (!parseDest(query, d)) { code = 400; return "{\"error\":\"dest must be 0-23\"}"; }
        int node = d + 1;
        vector<int> chain; for (int v = node; v != -1; v = prevN[v]) chain.push_back(v);
        reverse(chain.begin(), chain.end());
        string route = "[";
        for (size_t k = 0; k < chain.size(); k++) route += (k ? "," : "") + q(nodeName(chain[k]));
        route += "]";
        if (path == "/api/route")
            return "{\"dest\":" + to_string(d) + ",\"name\":" + q(DEST[d]) + ",\"minutes\":" + to_string(distTo[node]) + ",\"path\":" + route + "}";
        int b = nextBus(d);
        if (b < 0) return "{\"found\":false,\"dest\":" + to_string(d) + ",\"rideMin\":" + to_string(distTo[node]) + "}";
        int arrive = istMinutes() + etaMin(B[b]) + distTo[node];
        return "{\"found\":true,\"bus\":" + busJson(B[b]) + ",\"rideMin\":" + to_string(distTo[node]) +
               ",\"arriveAt\":" + q(hhmm(arrive)) + ",\"path\":" + route + "}";
    }
    code = 404; return "{\"error\":\"Not found\"}";
}

// ---------------- Tiny HTTP server ----------------
void sendAll(sock_t c, const string& s) {
    size_t off = 0;
    while (off < s.size()) {
        int n = (int)send(c, s.data() + off, (int)(s.size() - off), 0);
        if (n <= 0) return;
        off += n;
    }
}
void serveOne(sock_t c) {
    char buf[4096]; int n = (int)recv(c, buf, sizeof buf - 1, 0);
    if (n <= 0) return;
    buf[n] = 0;
    string req(buf), method, target;
    size_t sp1 = req.find(' '), sp2 = req.find(' ', sp1 + 1);
    if (sp1 == string::npos || sp2 == string::npos) return;
    method = req.substr(0, sp1); target = req.substr(sp1 + 1, sp2 - sp1 - 1);
    string cors = "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, OPTIONS\r\n"
                  "Access-Control-Allow-Headers: Content-Type\r\nAccess-Control-Allow-Private-Network: true\r\n";
    if (method == "OPTIONS") { sendAll(c, "HTTP/1.1 204 No Content\r\n" + cors + "Content-Length: 0\r\nConnection: close\r\n\r\n"); return; }
    if (method != "GET") { string b = "{\"error\":\"Only GET\"}"; sendAll(c, "HTTP/1.1 405 Method Not Allowed\r\n" + cors + "Content-Type: application/json\r\nContent-Length: " + to_string(b.size()) + "\r\nConnection: close\r\n\r\n" + b); return; }
    string path = target, query; size_t qm = target.find('?');
    if (qm != string::npos) { path = target.substr(0, qm); query = target.substr(qm + 1); }
    int code; string body = handle(path, query, code);
    const char* txt = code == 200 ? "OK" : code == 400 ? "Bad Request" : "Not Found";
    sendAll(c, "HTTP/1.1 " + to_string(code) + " " + txt + "\r\n" + cors + "Content-Type: application/json\r\nContent-Length: " +
               to_string(body.size()) + "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n" + body);
}

int main(int argc, char** argv) {
    int port = argc > 1 ? atoi(argv[1]) : 8080;
    string host = argc > 2 ? argv[2] : "127.0.0.1";
    if (port < 1 || port > 65535) { fprintf(stderr, "Port must be 1-65535\n"); return 1; }

    // Roads (minutes) - placeholders. Node 0 = campus gate, node d+1 = destination d.
    int R[][3] = {{0,7,5},{0,9,8},{0,10,10},{0,1,12},{7,5,12},{5,6,7},{6,4,5},{4,11,3},{4,8,9},{8,14,6},{10,12,6},{12,4,8},
        {9,10,5},{4,2,6},{2,3,4},{10,13,9},{13,20,4},{8,15,12},{15,23,8},{5,18,6},{18,19,8},{7,17,10},{17,16,7},{1,24,9},
        {16,22,11},{19,21,25},{0,13,14}};
    for (auto& r : R) road(r[0], r[1], r[2]);
    dijkstra(0);
    for (int i = 0; i < (int)B.size(); i++) busIndex.put(B[i].id, i);
    buildTimetable();

#ifdef _WIN32
    WSADATA w; if (WSAStartup(MAKEWORD(2, 2), &w) != 0) { fprintf(stderr, "WSAStartup failed\n"); return 1; }
#endif
    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) { fprintf(stderr, "Could not create socket\n"); return 1; }
    int yes = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);
    sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host.c_str(), &a.sin_addr) != 1) { fprintf(stderr, "Bad host address: %s\n", host.c_str()); return 1; }
    if (bind(s, (sockaddr*)&a, sizeof a) < 0 || listen(s, 16) < 0) { fprintf(stderr, "Could not listen on %s:%d (port busy?)\n", host.c_str(), port); return 1; }
    printf("Bus tracker server running at http://%s:%d  (Ctrl+C to stop)\n", host.c_str(), port);
    fflush(stdout);
    for (;;) {
        sock_t c = accept(s, nullptr, nullptr);
        if (c == INVALID_SOCKET) continue;
#ifdef _WIN32
        DWORD to = 2000; setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char*)&to, sizeof to);
#else
        timeval to{2, 0}; setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char*)&to, sizeof to);
#endif
        serveOne(c);
        CLOSESOCK(c);
    }
}
