#ifndef EMERGENCY_H
#define EMERGENCY_H

#include "types.h"
#include "linked_list.h"

Emergency *emergency_create(List *emergencies, int id, int locationNode,
                            Severity severity, EmergencyType type, int patients, long now);
Emergency *emergency_findById(const List *emergencies, int id);
int  emergency_isActive(const Emergency *e);
void emergency_printAll(const List *emergencies);
const char *severity_name(Severity s);
const char *emergencyType_name(EmergencyType t);
const char *emergencyStatus_name(EmergencyStatus s);

#endif
