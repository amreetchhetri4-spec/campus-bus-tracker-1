/* Graphic Era Hill University - Campus Bus Tracker (C99, console)
 * DSA used: min-heap, hash table (chaining), graph + Dijkstra,
 * circular queue, merge sort, binary search.
 * Compile: gcc -std=c99 -Wall -o bus_tracker bus_tracker.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* ---------- Data (placeholders: replace with your real stops, routes, buses) ---------- */
#define N 11
static const char *STOP[N] = {"Campus Gate", "Clement Town", "ISBT", "Railway Station", "Ghanta Ghar",
                              "Rajpur Road", "Prem Nagar", "Ballupur Chowk", "Rispana Bridge",
                              "Mall of Dehradun", "Paltan Bazaar"};
enum { AV, ON, FULL, OFF, MAINT };
static const char *STATUS[] = {"Available", "On route", "Full", "Off campus", "Maintenance"};
typedef struct { char id[16]; int dest, st, load, eta; } Bus;
#define NB 12
static Bus B[NB] = {
    {"GEHU-101", 1, AV, 62, 4},   {"GEHU-102", 6, ON, 39, 8},  {"GEHU-103", 7, FULL, 100, 13},
    {"GEHU-104", 4, AV, 48, 6},   {"GEHU-105", 2, OFF, 0, 0},  {"GEHU-106", 1, AV, 25, 10},
    {"GEHU-107", 6, FULL, 100, 17}, {"GEHU-108", 7, ON, 74, 12}, {"GEHU-109", 4, AV, 55, 15},
    {"GEHU-110", 2, MAINT, 0, 0}, {"GEHU-111", 3, AV, 35, 5},  {"GEHU-112", 5, ON, 58, 9}};
static int now = 8 * 60;

/* ---------- 1. Min-heap ---------- */
typedef struct { int key, val; } HItem;
typedef struct { HItem a[256]; int n; } MinHeap;
static void swapi(HItem *x, HItem *y) { HItem t = *x; *x = *y; *y = t; }
static void hpush(MinHeap *h, int k, int v) {
    int i = h->n++;
    h->a[i].key = k; h->a[i].val = v;
    while (i > 0 && h->a[(i - 1) / 2].key > h->a[i].key) { swapi(&h->a[i], &h->a[(i - 1) / 2]); i = (i - 1) / 2; }
}
static HItem hpop(MinHeap *h) {
    HItem top = h->a[0];
    h->a[0] = h->a[--h->n];
    int i = 0;
    for (;;) {
        int l = 2 * i + 1, r = l + 1, m = i;
        if (l < h->n && h->a[l].key < h->a[m].key) m = l;
        if (r < h->n && h->a[r].key < h->a[m].key) m = r;
        if (m == i) break;
        swapi(&h->a[i], &h->a[m]); i = m;
    }
    return top;
}

/* ---------- 2. Hash table with separate chaining ---------- */
#define M 31
typedef struct Node { char key[16]; int val; struct Node *next; } Node;
static Node *htab[M];
static unsigned hashf(const char *s) { unsigned x = 5381; while (*s) x = x * 33 + (unsigned char)*s++; return x % M; }
static void hput(const char *k, int v) {
    unsigned i = hashf(k);
    for (Node *p = htab[i]; p; p = p->next) if (!strcmp(p->key, k)) { p->val = v; return; }
    Node *nn = malloc(sizeof *nn);
    if (!nn) exit(1);
    strncpy(nn->key, k, 15); nn->key[15] = 0; nn->val = v; nn->next = htab[i]; htab[i] = nn;
}
static int hget(const char *k) {
    for (Node *p = htab[hashf(k)]; p; p = p->next) if (!strcmp(p->key, k)) return p->val;
    return -1;
}

/* ---------- 3. Graph (adjacency matrix of minutes) + Dijkstra ---------- */
static int W[N][N];
static void addRoad(int a, int b, int w) { W[a][b] = W[b][a] = w; }
static void dijkstra(int src, int dist[N], int prev[N]) {
    MinHeap h; h.n = 0;
    for (int i = 0; i < N; i++) { dist[i] = INT_MAX; prev[i] = -1; }
    dist[src] = 0; hpush(&h, 0, src);
    while (h.n) {
        HItem it = hpop(&h); int u = it.val;
        if (it.key > dist[u]) continue;
        for (int v = 0; v < N; v++)
            if (W[u][v] && dist[u] + W[u][v] < dist[v]) { dist[v] = dist[u] + W[u][v]; prev[v] = u; hpush(&h, dist[v], v); }
    }
}
static void printPath(const int prev[N], int v) {
    if (prev[v] != -1) { printPath(prev, prev[v]); printf(" -> "); }
    printf("%s", STOP[v]);
}

/* ---------- 4. Circular queue ---------- */
#define CAP 50
static char qa[CAP][120];
static int qhead = 0, qcount = 0;
static int qpush(const char *s) { if (qcount == CAP) return 0; snprintf(qa[(qhead + qcount++) % CAP], 120, "%s", s); return 1; }
static int qpop(char *out) { if (!qcount) return 0; strcpy(out, qa[qhead]); qhead = (qhead + 1) % CAP; qcount--; return 1; }

/* ---------- 5. Merge sort (indices by key) ---------- */
static void mergeSort(int v[], const int key[], int lo, int hi) {
    if (hi - lo < 2) return;
    int mid = (lo + hi) / 2, tmp[NB], i = lo, j = mid, k = 0;
    mergeSort(v, key, lo, mid); mergeSort(v, key, mid, hi);
    while (i < mid && j < hi) tmp[k++] = key[v[i]] <= key[v[j]] ? v[i++] : v[j++];
    while (i < mid) tmp[k++] = v[i++];
    while (j < hi) tmp[k++] = v[j++];
    for (k = 0; k < hi - lo; k++) v[lo + k] = tmp[k];
}

/* ---------- 6. Binary search over sorted timetable ---------- */
static int TT[64], ntt = 0;
static void buildTimetable(void) {
    for (int t = 480; t < 720; t += 20) TT[ntt++] = t;
    for (int t = 840; t < 960; t += 20) TT[ntt++] = t;
    TT[ntt++] = 1080;
}
static int nextDeparture(int t, int *tomorrow) {
    int lo = 0, hi = ntt;
    while (lo < hi) { int mid = (lo + hi) / 2; if (TT[mid] < t) lo = mid + 1; else hi = mid; }
    *tomorrow = (lo == ntt);
    return *tomorrow ? TT[0] : TT[lo];
}

/* ---------- Service hours ---------- */
static int svcState(int t) {
    if ((t >= 480 && t < 720) || (t >= 840 && t < 960)) return 0;
    if (t >= 720 && t < 840) return 1;
    if (t >= 960 && t < 1080) return 2;
    return 3;
}
static const char *hhmm(int t) { static char b[8]; snprintf(b, sizeof b, "%02d:%02d", t / 60 % 24, t % 60); return b; }
static int running(const Bus *b) { return b->st <= FULL && svcState(now) == 0; }

static int nextBus(int dest) {
    MinHeap ok, full; ok.n = full.n = 0;
    for (int i = 0; i < NB; i++)
        if (B[i].dest == dest && running(&B[i])) hpush(B[i].st == FULL ? &full : &ok, B[i].eta, i);
    if (ok.n) return hpop(&ok).val;
    if (full.n) return hpop(&full).val;
    return -1;
}

/* ---------- Alerts ---------- */
typedef struct { int dest, lead, fired; } Alert;
static Alert alerts[N]; static int nalerts = 0;
static void checkAlerts(void) {
    char m[120];
    if (svcState(now) == 0)
        for (int k = 0; k < nalerts; k++) {
            Alert *a = &alerts[k];
            int b = nextBus(a->dest);
            if (b < 0) continue;
            int thr = a->lead > 1 ? a->lead : 1;
            if (B[b].eta <= thr) {
                if (!a->fired) {
                    a->fired = 1;
                    if (a->lead) snprintf(m, sizeof m, "Bus %s to %s is about %d min away.", B[b].id, STOP[a->dest], B[b].eta);
                    else snprintf(m, sizeof m, "Bus %s to %s is arriving now.", B[b].id, STOP[a->dest]);
                    qpush(m);
                }
            } else if (B[b].eta > thr + 2) a->fired = 0;
        }
    while (qpop(m)) printf("  [ALERT %s] %s\n", hhmm(now), m);
}

/* ---------- Actions ---------- */
static void advance(int mins) {
    for (int s = 0; s < mins; s++) {
        now++;
        if (svcState(now) != 0) continue;
        for (int i = 0; i < NB; i++) {
            if (B[i].st > FULL) continue;
            if (--B[i].eta <= 0) {
                B[i].eta = 12 + (i * 3) % 9;
                B[i].load = (B[i].load * 7 + 23) % 100;
                B[i].st = B[i].load > 60 ? ON : AV;
            }
        }
        checkAlerts();
    }
}
static void showService(void) {
    static const char *msg[] = {"Buses are running now.", "Midday break. Buses resume at 14:00.",
                                "Regular service has ended. Last bus leaves campus at 18:00.",
                                "Closed. Service starts at 08:00."};
    int tom, nd = nextDeparture(now, &tom);
    printf("Time %s (India time)\n%s\n", hhmm(now), msg[svcState(now)]);
    printf("Next scheduled departure: %s%s\n", hhmm(nd), tom ? " (tomorrow)" : "");
}
static void table(void) {
    int idx[NB], key[NB];
    for (int i = 0; i < NB; i++) { idx[i] = i; key[i] = running(&B[i]) ? B[i].eta : 9999; }
    mergeSort(idx, key, 0, NB);
    printf("%-10s%-18s%-8s%-8s%s\n", "BUS", "DESTINATION", "ETA", "FULL", "STATUS");
    for (int k = 0; k < NB; k++) {
        const Bus *b = &B[idx[k]]; int r = running(b); char e[16] = "-", l[16] = "-";
        if (r) { snprintf(e, sizeof e, "%d min", b->eta); snprintf(l, sizeof l, "%d%%", b->load); }
        printf("%-10s%-18s%-8s%-8s%s\n", b->id, STOP[b->dest], e, l, (r || b->st == MAINT) ? STATUS[b->st] : "Off hours");
    }
}
static int readInt(int *out) {
    char buf[64];
    if (!fgets(buf, sizeof buf, stdin)) return 0;
    char *end; long v = strtol(buf, &end, 10);
    if (end == buf) return -1;
    *out = (int)v; return 1;
}
static int pickDest(void) {
    for (int i = 1; i < N; i++) printf("%2d. %s\n", i, STOP[i]);
    printf("Choose destination: ");
    int d; if (readInt(&d) != 1 || d < 1 || d >= N) return -1;
    return d;
}
static void nextBusCmd(void) {
    int d = pickDest(); if (d < 0) { printf("Invalid choice.\n"); return; }
    int b = nextBus(d);
    if (b < 0) { printf("No bus to %s right now.\n", STOP[d]); showService(); return; }
    int dist[N], prev[N]; dijkstra(0, dist, prev);
    printf("Next bus: %s leaves in %d min, %d%% full (%s)\n", B[b].id, B[b].eta, B[b].load,
           B[b].load >= 100 ? "no seats" : B[b].load > 70 ? "seats limited" : "seats likely");
    printf("Ride time %d min, arriving at %s about %s\n", dist[d], STOP[d], hhmm(now + B[b].eta + dist[d]));
}
static void routeCmd(void) {
    int d = pickDest(); if (d < 0) { printf("Invalid choice.\n"); return; }
    int dist[N], prev[N]; dijkstra(0, dist, prev);
    printf("Fastest route (%d min): ", dist[d]); printPath(prev, d); printf("\n");
}
static void searchCmd(void) {
    char s[32], id[48]; int digits = 1;
    printf("Bus number (e.g. 103 or GEHU-103): ");
    if (!fgets(s, sizeof s, stdin)) return;
    s[strcspn(s, "\r\n")] = 0;
    if (!*s) digits = 0;
    for (char *p = s; *p; p++) if (*p < '0' || *p > '9') digits = 0;
    if (digits) snprintf(id, sizeof id, "GEHU-%s", s); else snprintf(id, sizeof id, "%s", s);
    int i = hget(id);
    if (i < 0) { printf("No bus with that number.\n"); return; }
    char e[24]; if (running(&B[i])) snprintf(e, sizeof e, "%d min", B[i].eta); else snprintf(e, sizeof e, "not running");
    printf("%s -> %s, %s, %d%% full, %s\n", B[i].id, STOP[B[i].dest], e, B[i].load, STATUS[B[i].st]);
}
static void alertCmd(void) {
    int d = pickDest(); if (d < 0) { printf("Invalid choice.\n"); return; }
    printf("Alert when it arrives (0) or minutes before (2, 5, 10): ");
    int lead; if (readInt(&lead) != 1 || lead < 0 || lead > 60) { printf("Invalid.\n"); return; }
    for (int k = 0; k < nalerts; k++)
        if (alerts[k].dest == d) { alerts[k].lead = lead; alerts[k].fired = 0; checkAlerts(); printf("Alert updated for %s.\n", STOP[d]); return; }
    alerts[nalerts].dest = d; alerts[nalerts].lead = lead; alerts[nalerts].fired = 0; nalerts++;
    printf("Alert set for %s.\n", STOP[d]);
    checkAlerts();
}

int main(void) {
    addRoad(0, 1, 5);  addRoad(0, 6, 8);  addRoad(0, 7, 10); addRoad(1, 2, 12); addRoad(7, 8, 6);
    addRoad(8, 4, 8);  addRoad(4, 3, 5);  addRoad(4, 10, 3); addRoad(3, 2, 7);  addRoad(4, 5, 9);
    addRoad(5, 9, 6);  addRoad(6, 7, 5);  addRoad(2, 4, 10);
    for (int i = 0; i < NB; i++) hput(B[i].id, i);
    buildTimetable();

    printf("Graphic Era Hill University - Bus Tracker (demo data)\n");
    printf("Enter the current time as HH (24-hour), then MM. Hour: ");
    int h, m;
    if (readInt(&h) == 1 && h >= 0 && h < 24) { printf("Minute: "); if (readInt(&m) == 1 && m >= 0 && m < 60) now = h * 60 + m; }
    int c = -1;
    while (c != 0) {
        printf("\n1 Service status   2 Next bus to a destination   3 All buses   4 Search bus\n"
               "5 Fastest route    6 Set alert                   7 Move clock forward   0 Exit\n> ");
        int r = readInt(&c);
        if (r == 0) break;                       /* end of input */
        if (r < 0) { c = -1; printf("Enter a number.\n"); continue; }
        switch (c) {
            case 1: showService(); break;
            case 2: nextBusCmd(); break;
            case 3: table(); break;
            case 4: searchCmd(); break;
            case 5: routeCmd(); break;
            case 6: alertCmd(); break;
            case 7: { printf("Minutes to move forward: "); int k; if (readInt(&k) == 1 && k > 0 && k <= 600) { advance(k); printf("Time is now %s\n", hhmm(now)); } else printf("Enter 1-600.\n"); break; }
            case 0: printf("Goodbye.\n"); break;
            default: printf("Choose 0-7.\n");
        }
    }
    return 0;
}
