#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "map.h"
#include "simulation.h"
#include "ambulance.h"
#include "emergency.h"
#include "road.h"
#include "hospital.h"

/* ---------------- cell model (drives ANSI colors) ---------------- */
enum {
    CELL_EMPTY = 0,
    CELL_ROAD, CELL_TRAFFIC, CELL_HEAVY, CELL_BLOCKED,
    CELL_ROUTE,
    CELL_NODE_J, CELL_NODE_H, CELL_NODE_S,
    CELL_EMERGENCY, CELL_AMBULANCE
};

static char g_cells[MAP_HEIGHT][MAP_WIDTH];
static int  g_attrs[MAP_HEIGHT][MAP_WIDTH];

/* What may draw over what: roads < routes < nodes < emergency < ambulance */
static int cellPrio(int attr) {
    switch (attr) {
        case CELL_ROAD: case CELL_TRAFFIC:
        case CELL_HEAVY: case CELL_BLOCKED:            return 1;
        case CELL_ROUTE:                               return 2;
        case CELL_NODE_J: case CELL_NODE_H: case CELL_NODE_S: return 3;
        case CELL_EMERGENCY:                           return 4;
        case CELL_AMBULANCE:                           return 5;
        default:                                       return 0;   /* EMPTY */
    }
}

static void setCell(int row, int col, char ch, int attr) {
    if (row < 0 || row >= MAP_HEIGHT || col < 0 || col >= MAP_WIDTH) return;
    if (cellPrio(attr) >= cellPrio(g_attrs[row][col])) {
        g_cells[row][col] = ch;
        g_attrs[row][col] = attr;
    }
}

typedef struct { int row, col; } Pos;

static char lineChar(Pos a, Pos b) {
    return (abs(b.col - a.col) >= abs(b.row - a.row)) ? '-' : '|';
}

/* Bresenham line on the character grid */
static void drawLine(int r0, int c0, int r1, int c1, char ch, int attr) {
    int dc = abs(c1 - c0), dr = -abs(r1 - r0);
    int sc = c0 < c1 ? 1 : -1, sr = r0 < r1 ? 1 : -1;
    int err = dc + dr;
    for (;;) {
        setCell(r0, c0, ch, attr);
        if (r0 == r1 && c0 == c1) break;
        int e2 = 2 * err;
        if (e2 >= dr) { err += dr; c0 += sc; }
        if (e2 <= dc) { err += dc; r0 += sr; }
    }
}

/* Independent x/y scaling so the city fills the terminal grid */
static void computeTransform(const Graph *g, double *sx, double *sy,
                             int *mnxOut, int *mnyOut) {
    int mnx = 0, mny = 0, mxx = 1, mxy = 1;
    if (g->numNodes > 0) {
        mnx = mxx = g->nodes[0].x;
        mny = mxy = g->nodes[0].y;
        for (int i = 1; i < g->numNodes; i++) {
            if (g->nodes[i].x < mnx) mnx = g->nodes[i].x;
            if (g->nodes[i].x > mxx) mxx = g->nodes[i].x;
            if (g->nodes[i].y < mny) mny = g->nodes[i].y;
            if (g->nodes[i].y > mxy) mxy = g->nodes[i].y;
        }
    }
    double spanX = (mxx > mnx) ? (double)(mxx - mnx) : 1.0;
    double spanY = (mxy > mny) ? (double)(mxy - mny) : 1.0;
    *sx = (MAP_WIDTH - 10.0) / spanX;
    *sy = (MAP_HEIGHT - 4.0)  / spanY;
    *mnxOut = mnx;
    *mnyOut = mny;
}

static Pos nodePos(const Node *n, double sx, double sy, int mnx, int mny) {
    Pos p;
    int c = 3 + (int)((n->x - mnx) * sx + 0.5);
    int r = 1 + (int)((n->y - mny) * sy + 0.5);
    if (c < 1) c = 1;
    if (c > MAP_WIDTH - 2)  c = MAP_WIDTH - 2;
    if (r < 0) r = 0;
    if (r > MAP_HEIGHT - 1) r = MAP_HEIGHT - 1;
    p.row = r; p.col = c;
    return p;
}

static int roundHalf(double v) { return (int)(v < 0 ? v - 0.5 : v + 0.5); }

/* Interpolated position along the edge being driven right now */
static Pos ambPos(const World *w, const Ambulance *a,
                  double sx, double sy, int mnx, int mny) {
    if (a->status == AMB_BUSY && a->route && a->routeIndex < a->routeLen
        && a->edgeTotal > 0) {
        Node *from = graph_getNode(&w->graph, a->currentNode);
        Node *to   = graph_getNode(&w->graph, a->route[a->routeIndex]);
        if (from && to) {
            Pos p1 = nodePos(from, sx, sy, mnx, mny);
            Pos p2 = nodePos(to,   sx, sy, mnx, mny);
            double t = 1.0 - (double)a->edgeRemaining / (double)a->edgeTotal;
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            Pos p;
            p.row = roundHalf(p1.row + (p2.row - p1.row) * t);
            p.col = roundHalf(p1.col + (p2.col - p1.col) * t);
            return p;
        }
    }
    Node *at = graph_getNode(&w->graph, a->currentNode);
    if (!at) { Pos p = { 0, 0 }; return p; }
    return nodePos(at, sx, sy, mnx, mny);
}

/* Two ambulances at one station: offset the second one sideways */
static void placeAmbulance(Pos p, int id) {
    char ch = (char)('0' + (id % 10));
    if (g_attrs[p.row][p.col] != CELL_AMBULANCE) {
        setCell(p.row, p.col, ch, CELL_AMBULANCE);
        return;
    }
    if (p.col + 1 < MAP_WIDTH && g_attrs[p.row][p.col + 1] != CELL_AMBULANCE) {
        setCell(p.row, p.col + 1, ch, CELL_AMBULANCE);
        return;
    }
    if (p.col - 1 >= 0 && g_attrs[p.row][p.col - 1] != CELL_AMBULANCE) {
        setCell(p.row, p.col - 1, ch, CELL_AMBULANCE);
        return;
    }
    setCell(p.row, p.col, ch, CELL_AMBULANCE);
}

static const char *colorOf(int attr) {
    switch (attr) {
        case CELL_ROAD:      return "\033[90m";   /* gray    */
        case CELL_TRAFFIC:   return "\033[33m";   /* yellow  */
        case CELL_HEAVY:     return "\033[93m";   /* bright yellow */
        case CELL_BLOCKED:   return "\033[31m";   /* red     */
        case CELL_ROUTE:     return "\033[36m";   /* cyan    */
        case CELL_NODE_J:    return "\033[97m";   /* white   */
        case CELL_NODE_H:    return "\033[95m";   /* magenta */
        case CELL_NODE_S:    return "\033[94m";   /* blue    */
        case CELL_EMERGENCY: return "\033[91m";   /* bright red   */
        case CELL_AMBULANCE: return "\033[92m";   /* bright green */
        default:             return "";
    }
}

static void printHeader(const World *w) {
    int active = 0;
    for (ListNode *n = w->emergencies.head; n; n = n->next)
        if (emergency_isActive((Emergency *)n->data)) active++;

    int freeAmb  = ambulance_countByStatus(&w->ambulances, AMB_AVAILABLE);
    int totalAmb = list_size(&w->ambulances);
    int beds = 0, totalBeds = 0;
    for (ListNode *n = w->hospitals.head; n; n = n->next) {
        Hospital *h = (Hospital *)n->data;
        beds += h->beds;
        totalBeds += h->totalBeds;
    }
    int blocked = road_countByCondition(&w->roads, ROAD_BLOCKED);
    int traffic = road_countByCondition(&w->roads, ROAD_TRAFFIC)
                + road_countByCondition(&w->roads, ROAD_HEAVY);

    printf("=============================================================================================\n");
    printf("  DYNAMIC EMERGENCY RESPONSE SYSTEM   |   TIME: %ld min\n", w->clock);
    printf("  Emergencies: %d active / %d queued   |   Ambulances: %d/%d free\n",
           active, pq_size(&w->pq), freeAmb, totalAmb);
    printf("  Hospital beds: %d/%d free   |   Roads: %d blocked, %d in traffic   |   Re-routes: %d\n",
           beds, totalBeds, blocked, traffic, w->reroutes);
    printf("=============================================================================================\n");
}

static void printLegend(void) {
    printf("--------------------------------------------- LEGEND ----------------------------------------\n");
    printf("  roads: '-' normal   '~' traffic   '=' heavy   'x' BLOCKED      '*' active ambulance route\n");
    printf("  nodes: 'o' junction 'H' hospital  'S' station\n");
    printf("  \033[92mgreen digit\033[0m = ambulance id    \033[91mred digit\033[0m = emergency id (at scene)\n");
    printf("=============================================================================================\n");
}

static void printNodeIndex(const World *w) {
    printf("-------------------------------------------- NODE INDEX -------------------------------------\n");
    int col = 0;
    char buf[40];
    for (int i = 0; i < w->graph.numNodes; i++) {
        Node *n = &w->graph.nodes[i];
        if (n->type == NODE_HOSPITAL) {
            Hospital *h = hospital_findByNode(&w->hospitals, i);
            snprintf(buf, sizeof buf, "%d:%s b:%d", i, n->name, h ? h->beds : 0);
        } else if (n->type == NODE_STATION) {
            snprintf(buf, sizeof buf, "%d:%s [S]", i, n->name);
        } else {
            snprintf(buf, sizeof buf, "%d:%s", i, n->name);
        }
        printf("%-30s", buf);
        if (++col == 3) { printf("\n"); col = 0; }
    }
    if (col) printf("\n");
}

void map_render(const World *w) {
    printf("\033[2J\033[H");                       /* clear screen, cursor home */
    memset(g_cells, ' ', sizeof g_cells);
    memset(g_attrs, 0,  sizeof g_attrs);           /* CELL_EMPTY = 0 */

    double sx, sy;
    int mnx, mny;
    computeTransform(&w->graph, &sx, &sy, &mnx, &mny);

    /* 1. roads, colored by condition */
    for (ListNode *n = w->roads.head; n; n = n->next) {
        Road *r = (Road *)n->data;
        Pos a = nodePos(&w->graph.nodes[r->from], sx, sy, mnx, mny);
        Pos b = nodePos(&w->graph.nodes[r->to],   sx, sy, mnx, mny);
        char ch;
        int attr;
        switch (road_conditionOf(r)) {
            case ROAD_NORMAL:  ch = lineChar(a, b); attr = CELL_ROAD;    break;
            case ROAD_TRAFFIC: ch = '~';            attr = CELL_TRAFFIC; break;
            case ROAD_HEAVY:   ch = '=';            attr = CELL_HEAVY;   break;
            default:           ch = 'x';            attr = CELL_BLOCKED; break;
        }
        drawLine(a.row, a.col, b.row, b.col, ch, attr);
    }

    /* 2. active routes (from the edge currently being driven) */
    for (ListNode *n = w->ambulances.head; n; n = n->next) {
        Ambulance *amb = (Ambulance *)n->data;
        if (amb->status != AMB_BUSY || !amb->route) continue;
        int start = amb->routeIndex - 1;
        if (start < 0) start = 0;
        for (int k = start; k + 1 < amb->routeLen; k++) {
            Node *a = graph_getNode(&w->graph, amb->route[k]);
            Node *b = graph_getNode(&w->graph, amb->route[k + 1]);
            if (!a || !b) continue;
            Pos pa = nodePos(a, sx, sy, mnx, mny);
            Pos pb = nodePos(b, sx, sy, mnx, mny);
            drawLine(pa.row, pa.col, pb.row, pb.col, '*', CELL_ROUTE);
        }
    }

    /* 3. nodes */
    for (int i = 0; i < w->graph.numNodes; i++) {
        Node *node = &w->graph.nodes[i];
        Pos p = nodePos(node, sx, sy, mnx, mny);
        char ch;
        int attr;
        switch (node->type) {
            case NODE_HOSPITAL: ch = 'H'; attr = CELL_NODE_H; break;
            case NODE_STATION:  ch = 'S'; attr = CELL_NODE_S; break;
            default:            ch = 'o'; attr = CELL_NODE_J; break;
        }
        setCell(p.row, p.col, ch, attr);
    }

    /* 4. emergencies waiting at the scene (patient not yet loaded) */
    for (ListNode *n = w->emergencies.head; n; n = n->next) {
        Emergency *e = (Emergency *)n->data;
        if (e->status != EMG_QUEUED && e->status != EMG_ASSIGNED) continue;
        Node *at = graph_getNode(&w->graph, e->locationNode);
        if (!at) continue;
        Pos p = nodePos(at, sx, sy, mnx, mny);
        setCell(p.row, p.col, (char)('0' + (e->id % 10)), CELL_EMERGENCY);
    }

    /* 5. ambulances (interpolated while driving) */
    for (ListNode *n = w->ambulances.head; n; n = n->next) {
        Ambulance *amb = (Ambulance *)n->data;
        Pos p = ambPos(w, amb, sx, sy, mnx, mny);
        placeAmbulance(p, amb->id);
    }

    /* ---- output ---- */
    printHeader(w);
    for (int r = 0; r < MAP_HEIGHT; r++) {
        for (int c = 0; c < MAP_WIDTH; c++) {
            if (g_attrs[r][c] == CELL_EMPTY) {
                putchar(' ');
            } else {
                printf("%s%c\033[0m", colorOf(g_attrs[r][c]), g_cells[r][c]);
            }
        }
        putchar('\n');
    }
    printLegend();
    printNodeIndex(w);
}