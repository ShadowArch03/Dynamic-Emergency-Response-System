#ifndef CONFIG_H
#define CONFIG_H

#define MAX_NAME        64     /* node/hospital name buffer          */
#define MAX_LOG_TEXT    256    /* one report-log line                */
#define INF_COST        1000000000  /* "infinity" for Dijkstra      */

/* Run executables from the PROJECT ROOT so these relative paths work */
#define NODES_CSV       "data/nodes.csv"
#define ROADS_CSV       "data/roads.csv"
#define AMBULANCES_CSV  "data/ambulances.csv"
#define HOSPITALS_CSV   "data/hospitals.csv"
#define EMERGENCIES_CSV "data/emergencies.csv"
#define REPORT_FILE     "report.txt"

/* Terminal map dashboard dimensions (characters) */
#define MAP_WIDTH       92
#define MAP_HEIGHT      26

#endif
