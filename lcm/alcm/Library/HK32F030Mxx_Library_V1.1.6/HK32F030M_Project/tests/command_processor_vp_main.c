/* ALCM - VP_LIGHT_CONTROL command processor tests (GPL-3.0-or-later) */
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <cmocka.h>

#include "test_command_processor_vp.h"

int main(void)
{
    return cmocka_run_group_tests_name("Command Processor VP Test", command_processor_vp_tests,
                                       NULL, NULL);
}
