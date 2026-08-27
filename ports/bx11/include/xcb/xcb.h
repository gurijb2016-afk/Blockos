#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct xcb_connection_t xcb_connection_t;
typedef struct { uint8_t response_type; uint8_t pad0; uint16_t sequence; uint32_t pad[7]; } xcb_generic_event_t;
typedef uint32_t xcb_window_t;
typedef uint32_t xcb_atom_t;
typedef struct { uint8_t reply; uint8_t sequence; uint16_t length; } xcb_void_cookie_t;
typedef xcb_void_cookie_t xcb_window_cookie_t;

xcb_connection_t *xcb_connect(const char *, int *);
int xcb_connection_has_error(xcb_connection_t *);
void xcb_disconnect(xcb_connection_t *);
xcb_window_t xcb_generate_id(xcb_connection_t *);
xcb_void_cookie_t xcb_create_window(xcb_connection_t *, uint8_t, xcb_window_t, xcb_window_t,
                                     int16_t, int16_t, uint16_t, uint16_t, uint16_t,
                                     uint16_t, uint32_t, uint32_t, const uint32_t *);
xcb_void_cookie_t xcb_map_window(xcb_connection_t *, xcb_window_t);
xcb_void_cookie_t xcb_unmap_window(xcb_connection_t *, xcb_window_t);
xcb_void_cookie_t xcb_destroy_window(xcb_connection_t *, xcb_window_t);
xcb_generic_event_t *xcb_wait_for_event(xcb_connection_t *);
int xcb_flush(xcb_connection_t *);
#ifdef __cplusplus
}
#endif
