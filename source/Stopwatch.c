/*******************************************************/
/*              Includes                               */
/*******************************************************/

#include "rb/Stopwatch.h"
#include "rb/Utils.h"
#include "rb/priv/ErrorPriv.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*******************************************************/
/*              Defines                                */
/*******************************************************/

#define STOPWATCH_MAGIC ( 0x1111AB34 )
#define MS_IN_S ( 1000 )
#define NS_IN_S ( 1000000 )

/*******************************************************/
/*              Typedefs                               */
/*******************************************************/

typedef struct {
    int32_t magic;
    struct timespec time;
    bool timeSet;
} StopwatchContext;

/*******************************************************/
/*              Functions Declarations                 */
/*******************************************************/

static StopwatchContext* StopwatchPriv_getContext(Rb_StopwatchHandle handle);

/*******************************************************/
/*              Functions Definitions                  */
/*******************************************************/


Rb_StopwatchHandle Rb_Stopwatch_new(){
    int32_t rc;

    StopwatchContext* sw = (StopwatchContext*)RB_CALLOC(sizeof(StopwatchContext));

    sw->magic = STOPWATCH_MAGIC;

    rc = Rb_Stopwatch_reset(sw);
    if(rc != RB_OK){
        RB_ERR("Rb_Stopwatch_resetf failed");
        RB_FREE(&sw);
        return NULL;
    }

    return (Rb_StopwatchHandle)sw;
}

int32_t Rb_Stopwatch_free(Rb_StopwatchHandle* handle){
    StopwatchContext* sw = StopwatchPriv_getContext(*handle);
    if(sw == NULL) {
        RB_ERRC(RB_INVALID_ARG, "Invalid handle");
    }

    RB_FREE(&sw);
    *handle = NULL;

    return RB_OK;
}

int64_t Rb_Stopwatch_reset(Rb_StopwatchHandle handle){
    StopwatchContext* sw = StopwatchPriv_getContext(handle);
    if (sw == NULL) {
        RB_ERRC(RB_INVALID_ARG, "Invalid handle");
    }

    int64_t elapsedMs = sw->timeSet ? Rb_Stopwatch_elapsedMs(handle) : 0;
    if(elapsedMs < 0){
        RB_ERRC(elapsedMs, "Rb_Stopwatch_elapsedMs failed");
    }

    int32_t rc = clock_gettime(CLOCK_REALTIME, &sw->time);
    if(rc != 0){
        RB_ERRC(RB_ERROR, "clock_gettime failed: %d", rc);
    }

    if(!sw->timeSet){
        sw->timeSet = true;
    }

    return elapsedMs;
}

int64_t Rb_Stopwatch_elapsedMs(Rb_StopwatchHandle handle){
    int32_t rc;

    StopwatchContext* sw = StopwatchPriv_getContext(handle);
    if(sw == NULL) {
        RB_ERRC(-1, "Invalid handle");
    }

    struct timespec currTime;
    rc = clock_gettime(CLOCK_REALTIME, &currTime);
    if(rc != 0){
        RB_ERRC(RB_ERROR, "clock_gettime failed: %d", rc);
    }

    int64_t elapsedMs = 0;

    elapsedMs += ( currTime.tv_sec - sw->time.tv_sec ) * MS_IN_S;
    elapsedMs += ( currTime.tv_nsec - sw->time.tv_nsec ) / NS_IN_S;

    return elapsedMs;
}

StopwatchContext* StopwatchPriv_getContext(Rb_StopwatchHandle handle) {
    if(handle == NULL) {
        return NULL;
    }

    StopwatchContext* sw = (StopwatchContext*)handle;
    if(sw->magic != STOPWATCH_MAGIC) {
        return NULL;
    }

    return sw;
}
