#ifndef BLOCKOS_DBUS_PROTOCOL_H
#define BLOCKOS_DBUS_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define DBUS_MAX_CLIENTS       64
#define DBUS_MAX_NAMES_PER_CONN 32
#define DBUS_MAX_MATCHES       32
#define DBUS_MAX_MESSAGE       (256u * 1024u)
#define DBUS_MAX_AUTH          2048u
#define DBUS_NAME_MAX          256
#define DBUS_PATH_MAX          256
#define DBUS_MEMBER_MAX        256
#define DBUS_SIGNATURE_MAX     128
#define DBUS_MATCH_MAX         512

#define DBUS_MESSAGE_METHOD_CALL   1
#define DBUS_MESSAGE_METHOD_RETURN 2
#define DBUS_MESSAGE_ERROR         3
#define DBUS_MESSAGE_SIGNAL        4

#define DBUS_FLAG_NO_REPLY_EXPECTED 0x01
#define DBUS_FLAG_NO_AUTO_START     0x02
#define DBUS_FLAG_ALLOW_INTERACTIVE_AUTHORIZATION 0x04

#define DBUS_HEADER_PATH        1
#define DBUS_HEADER_INTERFACE   2
#define DBUS_HEADER_MEMBER      3
#define DBUS_HEADER_ERROR_NAME  4
#define DBUS_HEADER_REPLY_SERIAL 5
#define DBUS_HEADER_DESTINATION 6
#define DBUS_HEADER_SENDER      7
#define DBUS_HEADER_SIGNATURE   8
#define DBUS_HEADER_UNIX_FDS    9

#define DBUS_REQUEST_NAME_ALLOW_REPLACEMENT 0x01u
#define DBUS_REQUEST_NAME_REPLACE_EXISTING  0x02u
#define DBUS_REQUEST_NAME_DO_NOT_QUEUE      0x04u

#define DBUS_REQUEST_PRIMARY_OWNER 1u
#define DBUS_REQUEST_IN_QUEUE      2u
#define DBUS_REQUEST_EXISTS        3u
#define DBUS_REQUEST_ALREADY_OWNER 4u

#define DBUS_RELEASE_RELEASED      1u
#define DBUS_RELEASE_NON_EXISTENT  2u
#define DBUS_RELEASE_NOT_OWNER     3u

typedef struct {
    uint32_t code;
    char signature[DBUS_SIGNATURE_MAX];
    char string_value[DBUS_NAME_MAX];
    uint32_t u32_value;
    uint8_t byte_value;
    int present;
} DBusHeaderField;

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint8_t version;
    uint32_t body_len;
    uint32_t serial;
    uint32_t fields_len;
    size_t header_size;
    DBusHeaderField fields[16];
    size_t field_count;
} DBusMessageView;

#endif
