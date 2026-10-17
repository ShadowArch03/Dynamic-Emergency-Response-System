#ifndef HOSPITAL_H
#define HOSPITAL_H

#include "types.h"
#include "linked_list.h"

Hospital *hospital_create(List *hospitals, int id, int nodeId, const char *name,
                          int beds, int emergencyService, int icu);
Hospital *hospital_findById(const List *hospitals, int id);
Hospital *hospital_findByNode(const List *hospitals, int nodeId);
int  hospital_isCandidate(const Hospital *h, const Emergency *e);
void hospital_printAll(const List *hospitals);

#endif
