#pragma once
void log_error(const char *f, ...);
void fatal_error_terminate(void);
#define SEND_APP_DATA(buffer, buf_size, ind) VESC_IF->send_app_data(buffer, ind)
