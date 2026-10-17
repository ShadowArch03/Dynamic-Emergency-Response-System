#ifndef AUTH_H
#define AUTH_H

typedef enum {
    ROLE_NONE = -1,          /* failed login -> back to login screen */
    ROLE_EXIT = 0,
    ROLE_ADMIN,
    ROLE_CITIZEN,
    ROLE_DRIVER
} Role;

Role auth_login(void);
const char *auth_roleName(Role r);

#endif