/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@bessy.de>
 */

#include <stdio.h>

#include "epicsExport.h"
#include "freeList.h"
#include "db_field_log.h"
#include "chfPlugin.h"
#include "dbState.h"

#define STATE_NAME_LENGTH 20

static const
chfPluginEnumType modeEnum[] = { {"before", 0}, {"first", 1},
                                 {"last", 2}, {"after", 3},
                                 {"while", 4}, {"unless", 5},
                                 {NULL,0} };
typedef enum syncMode {
    syncModeBefore=0,
    syncModeFirst=1,
    syncModeLast=2,
    syncModeAfter=3,
    syncModeWhile=4,
    syncModeUnless=5
} syncMode;

typedef struct myStruct {
    syncMode mode;
    char state[STATE_NAME_LENGTH];
    dbStateId id;
    db_field_log *lastfl;
    int laststate:1;
} myStruct;

static void *myStructFreeList;

static const
chfPluginArgDef opts[] = {
    chfEnum   (myStruct, mode,  "m", 1, 1, modeEnum),
    chfString (myStruct, state, "s", 1, 0),
    chfPluginArgEnd
};

static void * allocPvt(void)
{
    myStruct *my = (myStruct*) freeListCalloc(myStructFreeList);
    return (void *) my;
}

static void freePvt(void *pvt)
{
    freeListFree(myStructFreeList, pvt);
}

static int parse_ok(void *pvt)
{
    myStruct *my = (myStruct*) pvt;

    if (!(my->id = dbStateFind(my->state)))
        return -1;

    return 0;
}

static db_field_log* filter(void* pvt, dbChannel *chan, db_field_log *pfl) {
    db_field_log *passfl = NULL;
    myStruct *my = (myStruct*) pvt;
    int actstate;

    if (pfl->ctx == dbfl_context_read)
        return pfl;

    actstate = dbStateGet(my->id);

    switch (my->mode) {
    case syncModeBefore:
        if (actstate && !my->laststate) {
            passfl = my->lastfl;
            my->lastfl = NULL;
        }
        break;
    case syncModeFirst:
        if (actstate && !my->laststate) {
            passfl = pfl;
            pfl = NULL;
        }
        break;
    case syncModeLast:
        if (!actstate && my->laststate) {
            passfl = my->lastfl;
            my->lastfl = NULL;
        }
        break;
    case syncModeAfter:
        if (!actstate && my->laststate) {
            passfl = pfl;
            pfl = NULL;
        }
        break;
    case syncModeWhile:
        if (actstate) {
            passfl = pfl;
        }
        goto no_shift;
    case syncModeUnless:
        if (!actstate) {
            passfl = pfl;
        }
        goto no_shift;
    }

    if (my->lastfl)
        db_delete_field_log(my->lastfl);
    my->lastfl = pfl;
    my->laststate = actstate;

    no_shift:
    return passfl;
}

static void channelRegisterPre(dbChannel *chan, void *pvt,
                               chPostEventFunc **cb_out, void **arg_out, db_field_log *probe)
{
    *cb_out = filter;
    *arg_out = pvt;
}

static void channel_report(dbChannel *chan, void *pvt, int level, const unsigned short indent)
{
    myStruct *my = (myStruct*) pvt;
    printf("%*s  plugin sync, mode=%s, state=%s\n", indent, "",
           chfPluginEnumString(modeEnum, my->mode, "n/a"), my->state);
}

static chfPluginIf pif = {
    allocPvt,
    freePvt,

    NULL, /* parse_error, */
    parse_ok,

    NULL, /* channel_open, */
    channelRegisterPre,
    NULL, /* channelRegisterPost, */
    channel_report,
    NULL /* channel_close */
};

static void syncInitialize(void)
{
    static int firstTime = 1;

    if (!firstTime) return;
    firstTime = 0;

    if (!myStructFreeList)
        freeListInitPvt(&myStructFreeList, sizeof(myStruct), 64);

    chfPluginRegister("sync", &pif, opts);
}

epicsExportRegistrar(syncInitialize);