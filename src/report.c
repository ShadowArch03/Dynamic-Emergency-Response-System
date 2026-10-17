#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "report.h"
#include "emergency.h"
#include "hospital.h"

/* Store the entry in the log list (linked list in action!); print when verbose. */
void report_log(World *w, const char *fmt, ...) {
    LogEntry *entry = malloc(sizeof(LogEntry));
    if (!entry) return;                          /* logging must never crash the sim */
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(entry->text, MAX_LOG_TEXT, fmt, ap);   /* bounded: no overflow possible */
    va_end(ap);
    entry->atTime = w->clock;
    if (list_push_back(&w->log, entry) != 0) { free(entry); return; }
    if (w->verbose) printf("[t=%3ld] %s\n", w->clock, entry->text);
}

void report_printLog(const World *w) {
    printf("\n---- EVENT LOG ----\n");
    for (ListNode *n = w->log.head; n; n = n->next) {
        LogEntry *e = (LogEntry *)n->data;
        printf("[t=%3ld] %s\n", e->atTime, e->text);
    }
}

static void writeSummary(FILE *out, const World *w) {
    fprintf(out, "\n================ RESPONSE SUMMARY ================\n");
    fprintf(out, "%-5s %-9s %-8s %-14s %-5s %-24s %-16s %s\n",
            "EMG", "SEVERITY", "TYPE", "STATUS", "AMB", "HOSPITAL", "LOCATION", "DETAILS");
    for (ListNode *n = w->emergencies.head; n; n = n->next) {
        Emergency *e = (Emergency *)n->data;
        char ambStr[16] = "-";
        char hosStr[64] = "-";
        char locStr[64] = "?";
        char detStr[96] = "";
        if (e->assignedAmbulance >= 0) snprintf(ambStr, sizeof ambStr, "A%d", e->assignedAmbulance);
        if (e->assignedHospital >= 0) {
            Hospital *h = hospital_findById(&w->hospitals, e->assignedHospital);
            snprintf(hosStr, sizeof hosStr, "%s", h ? h->name : "?");
        }
        Node *node = graph_getNode(&w->graph, e->locationNode);
        if (node) snprintf(locStr, sizeof locStr, "%s", node->name);
        if (e->status == EMG_COMPLETED)
            snprintf(detStr, sizeof detStr, "response=%ldmin reroutes=%d",
                     e->completedAt - e->createdAt, e->rerouteCount);
        else if (e->status == EMG_QUEUED)
            snprintf(detStr, sizeof detStr, "waiting since t=%ld", e->createdAt);
        else
            snprintf(detStr, sizeof detStr, "reroutes=%d", e->rerouteCount);
        fprintf(out, "E%-4d %-9s %-8s %-14s %-5s %-24s %-16s %s\n",
                e->id, severity_name(e->severity), emergencyType_name(e->type),
                emergencyStatus_name(e->status), ambStr, hosStr, locStr, detStr);
    }
    fprintf(out, "---------------------------------------------------\n");
    fprintf(out, "dispatched=%d  completed=%d  failed=%d  reroutes=%d  time=%ld min\n",
            w->dispatched, w->completed, w->failed, w->reroutes, w->clock);
}

void report_printSummary(const World *w) {
    writeSummary(stdout, w);
}

int report_save(const World *w, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) return -1;
    fprintf(f, "DYNAMIC EMERGENCY RESPONSE AND ROUTE OPTIMIZATION SYSTEM\n");
    fprintf(f, "report generated at simulation time %ld min\n", w->clock);
    writeSummary(f, w);
    fprintf(f, "\n---- FULL EVENT LOG ----\n");
    for (ListNode *n = w->log.head; n; n = n->next) {
        LogEntry *e = (LogEntry *)n->data;
        fprintf(f, "[t=%3ld] %s\n", e->atTime, e->text);
    }
    fclose(f);
    return 0;
}
