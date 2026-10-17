#ifndef TYPES_H
#define TYPES_H

#include "config.h"

/* ---------------- Node classification ---------------- */
typedef enum { NODE_JUNCTION = 0, NODE_HOSPITAL, NODE_STATION } NodeType;

/* ---------------- Ambulance ---------------- */
typedef enum { AMB_AVAILABLE = 0, AMB_BUSY, AMB_MAINTENANCE } AmbulanceStatus;
typedef enum { AMB_GENERAL = 0, AMB_ICU } AmbulanceType;

/* ---------------- Emergency ---------------- */
typedef enum { SEV_LOW = 0, SEV_MEDIUM, SEV_HIGH, SEV_CRITICAL } Severity;
typedef enum { EMG_NEED_GENERAL = 0, EMG_NEED_ICU } EmergencyType;

typedef enum {
    EMG_QUEUED = 0,          /* waiting for an ambulance            */
    EMG_ASSIGNED,            /* ambulance on the way to the scene   */
    EMG_TRANSPORTING,        /* patient on the way to the hospital  */
    EMG_COMPLETED,           /* patient delivered                    */
    EMG_FAILED_NO_AMBULANCE, /* never dispatched                     */
    EMG_FAILED_NO_HOSPITAL,  /* reached scene, no valid hospital     */
    EMG_FAILED_UNREACHABLE   /* scene became unreachable             */
} EmergencyStatus;

/* ---------------- Road (4-level traffic, Phase 1) ---------------- */
typedef enum { ROAD_NORMAL = 0, ROAD_TRAFFIC, ROAD_HEAVY, ROAD_BLOCKED } RoadCondition;

/* ---------------- Graph node (shared by DSA + map) ---------------- */
typedef struct {
    int id;                  /* 0-based sequential: id == array index */
    char name[MAX_NAME];
    int x, y;                /* coordinates used ONLY for drawing     */
    NodeType type;
} Node;

/* ---------------- Adjacency-list edge (linked list) ---------------- */
typedef struct Edge {
    int destination;
    int weight;              /* current travel time in minutes */
    int blocked;
    struct Edge *next;
} Edge;

/* ---------------- Road master record ---------------- */
typedef struct Road {
    int id;
    int from, to;
    int baseTime;
    int currentTime;
    int blocked;
} Road;

/* ---------------- Hospital ---------------- */
typedef struct Hospital {
    int id;
    int nodeId;
    char name[MAX_NAME];
    int beds;
    int totalBeds;
    int emergencyService;
    int icuAvailable;
} Hospital;

/* ---------------- Emergency ---------------- */
typedef struct Emergency {
    int id;
    int locationNode;
    Severity severity;
    EmergencyType type;
    int patients;
    int assignedAmbulance;   /* -1 = none */
    int assignedHospital;    /* -1 = none */
    EmergencyStatus status;
    long createdAt;          /* simulation minute */
    long completedAt;        /* -1 until finished */
    int rerouteCount;
    int warnedNoAmbulance;
} Emergency;

/* ---------------- Ambulance (live routing state) ---------------- */
typedef struct Ambulance {
    int id;
    int currentNode;         /* last node physically reached */
    AmbulanceStatus status;
    AmbulanceType type;
    int assignedEmergency;
    int *route;              /* node ids of the active route */
    int routeLen;
    int routeIndex;          /* index of the NEXT node to reach */
    int edgeRemaining;       /* minutes left until route[routeIndex] */
    int edgeTotal;           /* weight of the edge being traversed */
    int destinationNode;
    int destinationIsHospital;
} Ambulance;

#endif
