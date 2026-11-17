#pragma once
#include <stdint.h>
typedef enum { STATE_DISABLED = 0, STATE_STARTUP, STATE_READY, STATE_RUNNING } RunState;
typedef struct { RunState state; } State;
