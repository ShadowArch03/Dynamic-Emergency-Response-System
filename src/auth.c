#include <stdio.h>
#include <string.h>
#include "auth.h"

/* Demo credential - an academic simulation, not a production system. */
#define ADMIN_PASSWORD "admin"

static int readChoice(int lo, int hi) {
    int v, ok;
    for (;;) {
        printf("Select [%d-%d]: ", lo, hi);
        ok = scanf("%d", &v);
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {}
        if (ok == EOF) { printf("\n[input closed]\n"); return 0; }
        if (ok == 1 && v >= lo && v <= hi) return v;
        printf("  invalid input, try again\n");
    }
}

static int readPassword(char *buf, int size) {
    if (!fgets(buf, size, stdin)) return 0;
    buf[strcspn(buf, "\r\n")] = '\0';
    return 1;
}

Role auth_login(void) {
    printf("\n");
    printf("+---------------------- LOGIN ----------------------+\n");
    printf("|  1. ADMIN             control room (password)     |\n");
    printf("|  2. CITIZEN           report emergencies          |\n");
    printf("|  3. AMBULANCE DRIVER  routes & road conditions    |\n");
    printf("|  0. EXIT                                           |\n");
    printf("+----------------------------------------------------+\n");

    int c = readChoice(0, 3);
    if (c == 0) return ROLE_EXIT;

    if (c == 1) {
        char pw[32];
        for (int attempt = 0; attempt < 3; attempt++) {
            printf("Admin password (demo: admin): ");
            if (!readPassword(pw, sizeof pw)) return ROLE_EXIT;
            if (strcmp(pw, ADMIN_PASSWORD) == 0) return ROLE_ADMIN;
            printf("  wrong password (%d attempt%s left)\n",
                   2 - attempt, (2 - attempt) == 1 ? "" : "s");
        }
        printf("  login failed\n");
        return ROLE_NONE;
    }
    return (c == 2) ? ROLE_CITIZEN : ROLE_DRIVER;
}

const char *auth_roleName(Role r) {
    switch (r) {
        case ROLE_ADMIN:   return "ADMIN";
        case ROLE_CITIZEN: return "CITIZEN";
        case ROLE_DRIVER:  return "DRIVER";
        default:           return "?";
    }
}