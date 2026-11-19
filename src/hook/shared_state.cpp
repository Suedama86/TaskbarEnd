#include "shared_state.h"

TeRuntime& TeState()
{
    static TeRuntime s;
    return s;
}
