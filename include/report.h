#ifndef REPORT_H
#define REPORT_H

#include "simulation.h"

typedef struct LogEntry {
    char text[MAX_LOG_TEXT];
    long atTime;
} LogEntry;

void report_log(World *w, const char *fmt, ...);   /* store + print if verbose */
void report_printLog(const World *w);
void report_printSummary(const World *w);
int  report_save(const World *w, const char *filename);

#endif
