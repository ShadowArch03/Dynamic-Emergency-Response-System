#include <stdio.h>
#include <stdlib.h>
#include "emergency.h"

static int matchEmergencyId(void *data, void *context) {
    return ((Emergency *)data)->id == *(int *)context;
}

Emergency *emergency_create(List *emergencies, int id, int locationNode,
                            Severity severity, EmergencyType type, int patients, long now) {
    Emergency *e = calloc(1, sizeof(Emergency));
    if (!e) return NULL;
    e->id = id;
    e->locationNode = locationNode;
    e->severity = severity;
    e->type = type;
    e->patients = patients < 1 ? 1 : patients;   /* at least 1 patient */
    e->assignedAmbulance = -1;
    e->assignedHospital = -1;
    e->status = EMG_QUEUED;
    e->createdAt = now;
    e->completedAt = -1;
    e->rerouteCount = 0;
    e->warnedNoAmbulance = 0;
    if (list_push_back(emergencies, e) != 0) { free(e); return NULL; }
    return e;
}

Emergency *emergency_findById(const List *emergencies, int id) {
    return (Emergency *)list_find(emergencies, matchEmergencyId, &id);
}

int emergency_isActive(const Emergency *e) {
    return e->status == EMG_QUEUED ||
           e->status == EMG_ASSIGNED ||
           e->status == EMG_TRANSPORTING;
}

void emergency_printAll(const List *emergencies) {
    printf("%-5s %-6s %-9s %-8s %-4s %-5s %-6s %s\n",
           "ID", "NODE", "SEVERITY", "TYPE", "PAT", "AMB", "HOSP", "STATUS");
    for (ListNode *n = emergencies->head; n; n = n->next) {
        Emergency *e = (Emergency *)n->data;
        char amb[16] = "-";
        char hos[16] = "-";
        if (e->assignedAmbulance >= 0) snprintf(amb, sizeof amb, "A%d", e->assignedAmbulance);
        if (e->assignedHospital >= 0)  snprintf(hos, sizeof hos, "H%d", e->assignedHospital);
        printf("E%-4d %-6d %-9s %-8s %-4d %-5s %-6s %s\n",
               e->id, e->locationNode, severity_name(e->severity),
               emergencyType_name(e->type), e->patients, amb, hos,
               emergencyStatus_name(e->status));
    }
}

const char *severity_name(Severity s) {
    switch (s) {
        case SEV_MEDIUM:   return "MEDIUM";
        case SEV_HIGH:     return "HIGH";
        case SEV_CRITICAL: return "CRITICAL";
        default:           return "LOW";
    }
}

const char *emergencyType_name(EmergencyType t) {
    return t == EMG_NEED_ICU ? "ICU" : "GENERAL";
}

const char *emergencyStatus_name(EmergencyStatus s) {
    switch (s) {
        case EMG_QUEUED:              return "QUEUED";
        case EMG_ASSIGNED:            return "TO-SCENE";
        case EMG_TRANSPORTING:        return "TO-HOSPITAL";
        case EMG_COMPLETED:           return "COMPLETED";
        case EMG_FAILED_NO_AMBULANCE: return "FAIL:NO-AMB";
        case EMG_FAILED_NO_HOSPITAL:  return "FAIL:NO-HOSP";
        case EMG_FAILED_UNREACHABLE:  return "FAIL:UNREACH";
        default:                      return "?";
    }
}
