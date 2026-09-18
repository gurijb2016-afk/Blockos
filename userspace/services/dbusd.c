#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "sys/socket.h"
#include "sys/poll.h"
#include "dbus_protocol.h"

#define BUS_SYSTEM_PATH  "/run/dbus/system_bus_socket"
#define BUS_SESSION_PATH "/run/user/0/bus"
#define BUS_GUID         "424c4f434b4f532d4442555300000001"
#define BUS_UNIQUE_NAME  "org.freedesktop.DBus"
#define BUS_OBJECT_PATH  "/org/freedesktop/DBus"
#define BUS_INTERFACE    "org.freedesktop.DBus"

#define MATCH_TYPE_SIGNAL 1

typedef struct {
    char type[16];
    char sender[DBUS_NAME_MAX];
    char interface_name[DBUS_NAME_MAX];
    char member[DBUS_MEMBER_MAX];
    char path[DBUS_PATH_MAX];
    int has_type, has_sender, has_interface, has_member, has_path;
} MatchRule;

typedef struct {
    int fd;
    int used;
    int authenticated;
    int hello_done;
    uint32_t uid;
    uint32_t pid;
    uint32_t serial;
    char unique[DBUS_NAME_MAX];
    char names[DBUS_MAX_NAMES_PER_CONN][DBUS_NAME_MAX];
    uint32_t name_flags[DBUS_MAX_NAMES_PER_CONN];
    int name_count;
    MatchRule matches[DBUS_MAX_MATCHES];
    int match_count;
    uint8_t authbuf[DBUS_MAX_AUTH];
    size_t authlen;
    uint8_t inbuf[DBUS_MAX_MESSAGE];
    size_t inlen;
} Client;

typedef struct {
    int used;
    char name[DBUS_NAME_MAX];
    int owner;
    uint32_t flags;
} BusName;

static Client clients[DBUS_MAX_CLIENTS];
static BusName names[DBUS_MAX_CLIENTS * DBUS_MAX_NAMES_PER_CONN];
static uint32_t next_unique = 1;
static char bus_path[DBUS_PATH_MAX] = BUS_SYSTEM_PATH;
static int session_bus = 0;

static size_t align_to(size_t n, size_t a) { return (n + (a - 1)) & ~(a - 1); }
static size_t align8(size_t n) { return align_to(n, 8); }
static uint32_t rd_u32(const uint8_t *p) { uint32_t v; memcpy(&v, p, sizeof(v)); return v; }
static void wr_u32(uint8_t *p, uint32_t v) { memcpy(p, &v, sizeof(v)); }
static int in_bounds(size_t off, size_t need, size_t len) { return off <= len && need <= len - off; }

static int dbus_string_valid(const char *s) {
    if (!s || !s[0]) return 0;
    return 1;
}

static void client_close(int idx) {
    if (idx < 0 || idx >= DBUS_MAX_CLIENTS || !clients[idx].used) return;
    close(clients[idx].fd);
    memset(&clients[idx], 0, sizeof(clients[idx]));
}

static int owner_of_name(const char *name) {
    int i;
    for (i = 0; i < DBUS_MAX_CLIENTS * DBUS_MAX_NAMES_PER_CONN; ++i)
        if (names[i].used && strcmp(names[i].name, name) == 0)
            return names[i].owner;
    return -1;
}

static int name_slot_for(const char *name) {
    int i;
    for (i = 0; i < DBUS_MAX_CLIENTS * DBUS_MAX_NAMES_PER_CONN; ++i)
        if (names[i].used && strcmp(names[i].name, name) == 0)
            return i;
    return -1;
}

static int alloc_name_slot(void) {
    int i;
    for (i = 0; i < DBUS_MAX_CLIENTS * DBUS_MAX_NAMES_PER_CONN; ++i)
        if (!names[i].used) return i;
    return -1;
}

static int client_has_name(int ci, const char *name) {
    int i;
    for (i = 0; i < clients[ci].name_count; ++i)
        if (strcmp(clients[ci].names[i], name) == 0) return i;
    return -1;
}

static int alloc_client(void) {
    int i;
    for (i = 0; i < DBUS_MAX_CLIENTS; ++i) {
        if (!clients[i].used) {
            memset(&clients[i], 0, sizeof(clients[i]));
            clients[i].used = 1;
            clients[i].fd = -1;
            return i;
        }
    }
    return -1;
}

static void put_field_header(uint8_t *dst, size_t *pos, uint8_t code, char type) {
    *pos = align8(*pos);
    dst[(*pos)++] = code;
    dst[(*pos)++] = 2;
    dst[(*pos)++] = (uint8_t)type;
    dst[(*pos)++] = 0;
    if (type == 'u' || type == 's' || type == 'o') *pos = align_to(*pos, 4);
}

static size_t add_str_field(uint8_t *dst, size_t pos, uint8_t code, char type, const char *s) {
    uint32_t n = (uint32_t)strlen(s);
    put_field_header(dst, &pos, code, type);
    wr_u32(dst + pos, n); pos += 4;
    memcpy(dst + pos, s, n); pos += n;
    dst[pos++] = 0;
    return pos;
}

static size_t add_sig_field(uint8_t *dst, size_t pos, const char *sig) {
    return add_str_field(dst, pos, DBUS_HEADER_SIGNATURE, 'g', sig);
}

static size_t add_u32_field(uint8_t *dst, size_t pos, uint8_t code, uint32_t v) {
    put_field_header(dst, &pos, code, 'u');
    wr_u32(dst + pos, v);
    return pos + 4;
}

static size_t body_string(uint8_t *dst, size_t pos, const char *s) {
    uint32_t n = (uint32_t)strlen(s);
    pos = align_to(pos, 4);
    wr_u32(dst + pos, n); pos += 4;
    memcpy(dst + pos, s, n); pos += n;
    dst[pos++] = 0;
    return pos;
}

static size_t body_u32(uint8_t *dst, size_t pos, uint32_t v) {
    pos = align_to(pos, 4);
    wr_u32(dst + pos, v);
    return pos + 4;
}

static size_t body_bool(uint8_t *dst, size_t pos, int v) { return body_u32(dst, pos, v ? 1u : 0u); }

static size_t body_signature(uint8_t *dst, size_t pos, const char *s) {
    uint32_t n = (uint32_t)strlen(s);
    dst[pos++] = (uint8_t)n;
    memcpy(dst + pos, s, n); pos += n;
    dst[pos++] = 0;
    return pos;
}

static int parse_string(const uint8_t *body, size_t len, size_t *off, char *out, size_t cap) {
    uint32_t n;
    if (!in_bounds(*off, 4, len)) return 0;
    *off = align_to(*off, 4);
    if (!in_bounds(*off, 4, len)) return 0;
    n = rd_u32(body + *off); *off += 4;
    if (!in_bounds(*off, (size_t)n + 1, len)) return 0;
    if (cap) {
        size_t copy = n < cap - 1 ? n : cap - 1;
        memcpy(out, body + *off, copy); out[copy] = 0;
    }
    *off += (size_t)n + 1;
    return 1;
}

static int parse_u32(const uint8_t *body, size_t len, size_t *off, uint32_t *v) {
    *off = align_to(*off, 4);
    if (!in_bounds(*off, 4, len)) return 0;
    *v = rd_u32(body + *off); *off += 4; return 1;
}

static void field_reset(DBusHeaderField *f) { memset(f, 0, sizeof(*f)); }

static char *next_csv_token(char **cursor) {
    char *start;
    char *p;
    if (!cursor || !*cursor) return NULL;
    start = *cursor;
    p = strchr(start, ',');
    if (p) { *p = 0; *cursor = p + 1; }
    else { *cursor = NULL; }
    return start;
}

static int parse_variant_field(const uint8_t *p, size_t len, uint8_t code, DBusHeaderField *out) {
    size_t off = 0;
    uint32_t n;
    if (!in_bounds(0, 1, len)) return 0;
    n = p[off++];
    if (n == 0 || n >= DBUS_SIGNATURE_MAX || !in_bounds(off, n + 1, len)) return 0;
    memcpy(out->signature, p + off, n);
    out->signature[n] = 0;
    off += n + 1;
    if (out->signature[0] == 's' || out->signature[0] == 'o') off = align_to(off, 4);
    else if (out->signature[0] == 'u' || out->signature[0] == 'b') off = align_to(off, 4);
    if (out->signature[0] == 's' || out->signature[0] == 'o' || out->signature[0] == 'g') {
        if (!in_bounds(off, 4, len)) return 0;
        n = rd_u32(p + off); off += 4;
        if (!in_bounds(off, (size_t)n + 1, len)) return 0;
        if (n >= sizeof(out->string_value)) n = sizeof(out->string_value) - 1;
        memcpy(out->string_value, p + off, n); out->string_value[n] = 0;
        out->present = 1;
        return 1;
    }
    if (out->signature[0] == 'u' || out->signature[0] == 'b') {
        if (!in_bounds(off, 4, len)) return 0;
        out->u32_value = rd_u32(p + off);
        out->present = 1;
        return 1;
    }
    if (out->signature[0] == 'y') {
        if (!in_bounds(off, 1, len)) return 0;
        out->byte_value = p[off]; out->present = 1; return 1;
    }
    return 0;
}

static DBusHeaderField *find_field(DBusMessageView *m, uint32_t code) {
    size_t i;
    for (i = 0; i < m->field_count; ++i)
        if (m->fields[i].code == code) return &m->fields[i];
    return NULL;
}

static int parse_message(const uint8_t *buf, size_t len, DBusMessageView *m) {
    size_t pos, end;
    memset(m, 0, sizeof(*m));
    if (len < 16 || buf[0] != 'l') return 0;
    m->type = buf[1]; m->flags = buf[2]; m->version = buf[3];
    if (m->version != 1) return 0;
    m->body_len = rd_u32(buf + 4);
    m->serial = rd_u32(buf + 8);
    m->fields_len = rd_u32(buf + 12);
    if (m->fields_len > len - 16) return 0;
    end = 16 + (size_t)m->fields_len;
    end = align8(end);
    if (end > len || m->body_len > len - end) return 0;
    pos = 16;
    while (pos < 16 + m->fields_len) {
        uint8_t code;
        size_t start = pos;
        DBusHeaderField f;
        field_reset(&f);
        if (!in_bounds(pos, 1, 16 + m->fields_len)) return 0;
        code = buf[pos++];
        if (!in_bounds(pos, 1, 16 + m->fields_len)) return 0;
        /* Variant begins immediately after its type byte and must be aligned according to the contained value. */
        size_t vlen = (16 + m->fields_len) - pos;
        if (m->field_count >= sizeof(m->fields)/sizeof(m->fields[0])) return 0;
        if (!parse_variant_field(buf + pos, vlen, code, &f)) return 0;
        f.code = code;
        /* Advance by decoding the variant a second time using its signature. */
        {
            size_t q = pos;
            uint32_t sl = buf[q++];
            q += (size_t)sl + 1;
            if (f.signature[0] == 's' || f.signature[0] == 'o' || f.signature[0] == 'u' || f.signature[0] == 'b') q = align_to(q, 4);
            if (f.signature[0] == 's' || f.signature[0] == 'o' || f.signature[0] == 'g') {
                uint32_t n = rd_u32(buf + q); q += 4 + (size_t)n + 1;
            } else if (f.signature[0] == 'u' || f.signature[0] == 'b') {
                q += 4;
            } else if (f.signature[0] == 'y') {
                q += 1;
            } else return 0;
            pos = align8(q);
        }
        if (pos <= start) return 0;
        m->fields[m->field_count++] = f;
    }
    m->header_size = end;
    return 1;
}

static int send_raw(int fd, const uint8_t *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, buf + sent, len - sent);
        if (n <= 0) return 0;
        sent += (size_t)n;
    }
    return 1;
}

static size_t build_headers(uint8_t *msg, uint8_t type, uint8_t flags, uint32_t serial,
                            const char *path, const char *interface_name, const char *member,
                            const char *error_name, uint32_t reply_serial,
                            const char *destination, const char *sender, const char *signature) {
    size_t p = 16;
    msg[0] = 'l'; msg[1] = type; msg[2] = flags; msg[3] = 1;
    wr_u32(msg + 8, serial);
    if (path) p = add_str_field(msg, p, DBUS_HEADER_PATH, 'o', path);
    if (interface_name) p = add_str_field(msg, p, DBUS_HEADER_INTERFACE, 's', interface_name);
    if (member) p = add_str_field(msg, p, DBUS_HEADER_MEMBER, 's', member);
    if (error_name) p = add_str_field(msg, p, DBUS_HEADER_ERROR_NAME, 's', error_name);
    if (reply_serial) p = add_u32_field(msg, p, DBUS_HEADER_REPLY_SERIAL, reply_serial);
    if (destination) p = add_str_field(msg, p, DBUS_HEADER_DESTINATION, 's', destination);
    if (sender) p = add_str_field(msg, p, DBUS_HEADER_SENDER, 's', sender);
    if (signature && signature[0]) p = add_sig_field(msg, p, signature);
    p = align8(p);
    wr_u32(msg + 12, (uint32_t)(p - 16));
    return p;
}

static int send_method_return(Client *c, uint32_t reply_serial, const char *signature,
                              const uint8_t *body, size_t body_len) {
    uint8_t msg[DBUS_MAX_MESSAGE];
    size_t p = build_headers(msg, DBUS_MESSAGE_METHOD_RETURN, 0, ++c->serial,
                             NULL, NULL, NULL, NULL, reply_serial,
                             c->unique, BUS_UNIQUE_NAME, signature);
    if (p + body_len > sizeof(msg)) return 0;
    if (body_len) memcpy(msg + p, body, body_len);
    p += body_len;
    wr_u32(msg + 4, (uint32_t)body_len);
    return send_raw(c->fd, msg, p);
}

static int send_error(Client *c, uint32_t reply_serial, const char *name, const char *message) {
    uint8_t msg[4096], body[2048];
    size_t b = body_string(body, 0, message ? message : "");
    size_t p = build_headers(msg, DBUS_MESSAGE_ERROR, 0, ++c->serial,
                             NULL, NULL, NULL, name, reply_serial,
                             c->unique, BUS_UNIQUE_NAME, "s");
    if (p + b > sizeof(msg)) return 0;
    memcpy(msg + p, body, b); p += b;
    wr_u32(msg + 4, (uint32_t)b);
    return send_raw(c->fd, msg, p);
}

static int send_signal_to(Client *c, const char *path, const char *interface_name,
                          const char *member, const char *signature,
                          const uint8_t *body, size_t body_len, const char *destination) {
    uint8_t msg[DBUS_MAX_MESSAGE];
    size_t p = build_headers(msg, DBUS_MESSAGE_SIGNAL, 0, ++c->serial,
                             path, interface_name, member, NULL, 0,
                             destination, BUS_UNIQUE_NAME, signature);
    if (p + body_len > sizeof(msg)) return 0;
    memcpy(msg + p, body, body_len); p += body_len;
    wr_u32(msg + 4, (uint32_t)body_len);
    return send_raw(c->fd, msg, p);
}

static int match_rule_matches(const MatchRule *r, const DBusMessageView *m, const char *sender,
                              const char *path, const char *iface, const char *member) {
    if (r->has_type && strcmp(r->type, "signal") != 0) return 0;
    if (r->has_sender && strcmp(r->sender, sender ? sender : "") != 0) return 0;
    if (r->has_interface && strcmp(r->interface_name, iface ? iface : "") != 0) return 0;
    if (r->has_member && strcmp(r->member, member ? member : "") != 0) return 0;
    if (r->has_path && strcmp(r->path, path ? path : "") != 0) return 0;
    (void)m;
    return 1;
}

static int parse_match_rule(const char *rule, MatchRule *out) {
    char tmp[DBUS_MATCH_MAX];
    char *p, *item;
    memset(out, 0, sizeof(*out));
    if (!rule || strlen(rule) >= sizeof(tmp)) return 0;
    strcpy(tmp, rule);
    p = tmp;
    while ((item = next_csv_token(&p)) != NULL) {
        char *eq = strchr(item, '=');
        char *val;
        size_t n;
        if (!eq) continue;
        *eq = 0;
        val = eq + 1;
        while (*val == ' ' || *val == '\t') ++val;
        n = strlen(val);
        if (n >= 2 && val[0] == '\'' && val[n - 1] == '\'') { val[n - 1] = 0; ++val; }
        if (strcmp(item, "type") == 0) { strncpy(out->type, val, sizeof(out->type) - 1); out->has_type = 1; }
        else if (strcmp(item, "sender") == 0) { strncpy(out->sender, val, sizeof(out->sender) - 1); out->has_sender = 1; }
        else if (strcmp(item, "interface") == 0) { strncpy(out->interface_name, val, sizeof(out->interface_name) - 1); out->has_interface = 1; }
        else if (strcmp(item, "member") == 0) { strncpy(out->member, val, sizeof(out->member) - 1); out->has_member = 1; }
        else if (strcmp(item, "path") == 0) { strncpy(out->path, val, sizeof(out->path) - 1); out->has_path = 1; }
    }
    return out->has_type || out->has_sender || out->has_interface || out->has_member || out->has_path;
}

static void emit_name_owner_changed(const char *name, const char *old_owner, const char *new_owner) {
    uint8_t body[1024];
    size_t b = 0;
    char tmp_old[DBUS_NAME_MAX], tmp_new[DBUS_NAME_MAX];
    strncpy(tmp_old, old_owner ? old_owner : "", sizeof(tmp_old) - 1); tmp_old[sizeof(tmp_old)-1] = 0;
    strncpy(tmp_new, new_owner ? new_owner : "", sizeof(tmp_new) - 1); tmp_new[sizeof(tmp_new)-1] = 0;
    b = body_string(body, b, name);
    b = body_string(body, b, tmp_old);
    b = body_string(body, b, tmp_new);
    {
        DBusMessageView fake; memset(&fake, 0, sizeof(fake));
        fake.type = DBUS_MESSAGE_SIGNAL;
        int i;
        for (i = 0; i < DBUS_MAX_CLIENTS; ++i) {
            int m;
            if (!clients[i].used || !clients[i].hello_done || clients[i].fd < 0) continue;
            int matched = 0;
            if (clients[i].match_count == 0) continue;
            for (m = 0; m < clients[i].match_count; ++m) {
                if (match_rule_matches(&clients[i].matches[m], &fake, BUS_UNIQUE_NAME,
                                       BUS_OBJECT_PATH, BUS_INTERFACE, "NameOwnerChanged")) {
                    matched = 1; break;
                }
            }
            if (matched) send_signal_to(&clients[i], BUS_OBJECT_PATH, BUS_INTERFACE,
                                        "NameOwnerChanged", "sss", body, b, NULL);
        }
    }
}

static void add_name_to_client(int ci, const char *name, uint32_t flags) {
    int slot = alloc_name_slot();
    if (slot < 0 || clients[ci].name_count >= DBUS_MAX_NAMES_PER_CONN) return;
    names[slot].used = 1;
    names[slot].owner = ci;
    names[slot].flags = flags;
    strncpy(names[slot].name, name, sizeof(names[slot].name) - 1);
    strncpy(clients[ci].names[clients[ci].name_count], name, DBUS_NAME_MAX - 1);
    clients[ci].name_flags[clients[ci].name_count] = flags;
    clients[ci].name_count++;
}

static int drop_name_from_client(int ci, const char *name) {
    int idx = client_has_name(ci, name);
    int ns;
    if (idx < 0) return 0;
    ns = name_slot_for(name);
    if (ns >= 0) memset(&names[ns], 0, sizeof(names[ns]));
    if (idx + 1 < clients[ci].name_count) {
        memmove(&clients[ci].names[idx], &clients[ci].names[idx + 1],
                (size_t)(clients[ci].name_count - idx - 1) * DBUS_NAME_MAX);
        memmove(&clients[ci].name_flags[idx], &clients[ci].name_flags[idx + 1],
                (size_t)(clients[ci].name_count - idx - 1) * sizeof(uint32_t));
    }
    --clients[ci].name_count;
    return 1;
}

static void release_all_client_names(int ci) {
    while (clients[ci].name_count > 0) {
        char old_name[DBUS_NAME_MAX];
        strncpy(old_name, clients[ci].names[0], sizeof(old_name) - 1); old_name[sizeof(old_name)-1] = 0;
        drop_name_from_client(ci, old_name);
        emit_name_owner_changed(old_name, clients[ci].unique, "");
    }
}

static int extract_body(const DBusMessageView *m, const uint8_t *buf, size_t len,
                        const uint8_t **body, size_t *body_len) {
    if (m->header_size > len || m->body_len > len - m->header_size) return 0;
    *body = buf + m->header_size;
    *body_len = m->body_len;
    return 1;
}

static void handle_bus_call(int ci, const DBusMessageView *m, const uint8_t *buf, size_t len,
                            const char *path, const char *iface, const char *member,
                            const char *destination) {
    Client *c = &clients[ci];
    const uint8_t *body;
    size_t body_len, off = 0;
    char s1[DBUS_NAME_MAX], s2[DBUS_NAME_MAX];
    uint32_t u = 0;

    if (!extract_body(m, buf, len, &body, &body_len)) { send_error(c, m->serial, "org.freedesktop.DBus.Error.InvalidArgs", "Invalid body"); return; }
    if (!path || strcmp(path, BUS_OBJECT_PATH) != 0 || (iface && strcmp(iface, BUS_INTERFACE) != 0 && strcmp(iface, "org.freedesktop.DBus.Introspectable") != 0 && strcmp(iface, "org.freedesktop.DBus.Peer") != 0)) {
        send_error(c, m->serial, "org.freedesktop.DBus.Error.UnknownObject", "Unknown bus object");
        return;
    }

    if (!c->hello_done) {
        if (strcmp(member, "Hello") != 0) {
            send_error(c, m->serial, "org.freedesktop.DBus.Error.AccessDenied", "Hello must be called first");
            return;
        }
        if (c->unique[0]) {
            send_error(c, m->serial, "org.freedesktop.DBus.Error.Failed", "Hello already called"); return;
        }
        snprintf(c->unique, sizeof(c->unique), ":1.%u", next_unique++);
        c->hello_done = 1;
        add_name_to_client(ci, c->unique, 0);
        {
            uint8_t rb[DBUS_NAME_MAX + 8];
            size_t r = body_string(rb, 0, c->unique);
            send_method_return(c, m->serial, "s", rb, r);
        }
        emit_name_owner_changed(c->unique, "", c->unique);
        return;
    }

    if (strcmp(iface, "org.freedesktop.DBus.Peer") == 0) {
        if (strcmp(member, "Ping") == 0) { send_method_return(c, m->serial, "", NULL, 0); return; }
        if (strcmp(member, "GetMachineId") == 0) {
            uint8_t rb[64]; size_t r = body_string(rb, 0, "blockos-machine-id");
            send_method_return(c, m->serial, "s", rb, r); return;
        }
    }

    if (strcmp(iface, "org.freedesktop.DBus.Introspectable") == 0 && strcmp(member, "Introspect") == 0) {
        static const char xml[] =
            "<node>"
            "<interface name='org.freedesktop.DBus'>"
            "<method name='Hello'><arg direction='out' type='s'/></method>"
            "<method name='RequestName'><arg direction='in' type='s'/><arg direction='in' type='u'/><arg direction='out' type='u'/></method>"
            "<method name='ReleaseName'><arg direction='in' type='s'/><arg direction='out' type='u'/></method>"
            "<method name='ListNames'><arg direction='out' type='as'/></method>"
            "<method name='NameHasOwner'><arg direction='in' type='s'/><arg direction='out' type='b'/></method>"
            "<method name='GetNameOwner'><arg direction='in' type='s'/><arg direction='out' type='s'/></method>"
            "<method name='AddMatch'><arg direction='in' type='s'/></method>"
            "<method name='RemoveMatch'><arg direction='in' type='s'/></method>"
            "<method name='GetId'><arg direction='out' type='s'/></method>"
            "<method name='GetConnectionUnixUser'><arg direction='in' type='s'/><arg direction='out' type='u'/></method>"
            "<method name='GetConnectionUnixProcessID'><arg direction='in' type='s'/><arg direction='out' type='u'/></method>"
            "</interface>"
            "<interface name='org.freedesktop.DBus.Introspectable'>"
            "<method name='Introspect'><arg direction='out' type='s'/></method>"
            "</interface>"
            "<interface name='org.freedesktop.DBus.Peer'>"
            "<method name='Ping'/><method name='GetMachineId'><arg direction='out' type='s'/></method>"
            "</interface>"
            "</node>";
        uint8_t rb[8192]; size_t r = body_string(rb, 0, xml);
        send_method_return(c, m->serial, "s", rb, r); return;
    }

    if (strcmp(iface, BUS_INTERFACE) != 0) {
        send_error(c, m->serial, "org.freedesktop.DBus.Error.UnknownMethod", "Unknown interface"); return;
    }

    if (strcmp(member, "Hello") == 0) {
        send_error(c, m->serial, "org.freedesktop.DBus.Error.Failed", "Hello already called"); return;
    }
    if (strcmp(member, "GetId") == 0) {
        uint8_t rb[64]; size_t r = body_string(rb, 0, BUS_GUID);
        send_method_return(c, m->serial, "s", rb, r); return;
    }
    if (strcmp(member, "RequestName") == 0) {
        if (!parse_string(body, body_len, &off, s1, sizeof(s1)) || !parse_u32(body, body_len, &off, &u)) {
            send_error(c, m->serial, "org.freedesktop.DBus.Error.InvalidArgs", "Expected name and flags"); return;
        }
        if (!dbus_string_valid(s1)) { send_error(c, m->serial, "org.freedesktop.DBus.Error.InvalidArgs", "Invalid name"); return; }
        if (strcmp(s1, c->unique) == 0) { uint8_t rb[4]; size_t r = body_u32(rb, 0, DBUS_REQUEST_ALREADY_OWNER); send_method_return(c, m->serial, "u", rb, r); return; }
        {
            int slot = name_slot_for(s1);
            uint32_t result;
            if (slot >= 0) {
                int owner = names[slot].owner;
                if (owner == ci) result = DBUS_REQUEST_ALREADY_OWNER;
                else if ((u & DBUS_REQUEST_NAME_REPLACE_EXISTING) && (names[slot].flags & DBUS_REQUEST_NAME_ALLOW_REPLACEMENT)) {
                    char old_owner[DBUS_NAME_MAX];
                    strncpy(old_owner, clients[owner].unique, sizeof(old_owner)-1); old_owner[sizeof(old_owner)-1] = 0;
                    drop_name_from_client(owner, s1);
                    add_name_to_client(ci, s1, u);
                    result = DBUS_REQUEST_PRIMARY_OWNER;
                    emit_name_owner_changed(s1, old_owner, c->unique);
                } else result = DBUS_REQUEST_EXISTS;
            } else {
                add_name_to_client(ci, s1, u); result = DBUS_REQUEST_PRIMARY_OWNER;
                emit_name_owner_changed(s1, "", c->unique);
            }
            uint8_t rb[4]; size_t r = body_u32(rb, 0, result);
            send_method_return(c, m->serial, "u", rb, r);
        }
        return;
    }
    if (strcmp(member, "ReleaseName") == 0) {
        int slot, owner;
        if (!parse_string(body, body_len, &off, s1, sizeof(s1))) { send_error(c, m->serial, "org.freedesktop.DBus.Error.InvalidArgs", "Expected name"); return; }
        slot = name_slot_for(s1);
        if (slot < 0) u = DBUS_RELEASE_NON_EXISTENT;
        else {
            owner = names[slot].owner;
            if (owner != ci) u = DBUS_RELEASE_NOT_OWNER;
            else { char old_owner[DBUS_NAME_MAX]; strncpy(old_owner,c->unique,sizeof(old_owner)-1); drop_name_from_client(ci,s1); u=DBUS_RELEASE_RELEASED; emit_name_owner_changed(s1,old_owner,""); }
        }
        { uint8_t rb[4]; size_t r = body_u32(rb,0,u); send_method_return(c,m->serial,"u",rb,r); }
        return;
    }
    if (strcmp(member, "NameHasOwner") == 0) {
        if (!parse_string(body, body_len, &off, s1, sizeof(s1))) { send_error(c,m->serial,"org.freedesktop.DBus.Error.InvalidArgs","Expected name"); return; }
        { uint8_t rb[4]; size_t r = body_bool(rb,0,owner_of_name(s1) >= 0); send_method_return(c,m->serial,"b",rb,r); }
        return;
    }
    if (strcmp(member, "GetNameOwner") == 0) {
        int owner;
        if (!parse_string(body, body_len, &off, s1, sizeof(s1))) { send_error(c,m->serial,"org.freedesktop.DBus.Error.InvalidArgs","Expected name"); return; }
        owner = owner_of_name(s1);
        if (owner < 0) { send_error(c,m->serial,"org.freedesktop.DBus.Error.NameHasNoOwner","Name has no owner"); return; }
        { uint8_t rb[DBUS_NAME_MAX + 8]; size_t r = body_string(rb,0,clients[owner].unique); send_method_return(c,m->serial,"s",rb,r); }
        return;
    }
    if (strcmp(member, "ListNames") == 0) {
        uint8_t rb[16384]; size_t r = 0; int i, count = 0;
        /* as = uint32 length + array of aligned struct string values. */
        r = align_to(r,4); r += 4;
        size_t array_start = r;
        for (i = 0; i < DBUS_MAX_CLIENTS * DBUS_MAX_NAMES_PER_CONN; ++i) {
            if (!names[i].used) continue;
            r = body_string(rb,r,names[i].name); ++count;
        }
        wr_u32(rb + array_start - 4, (uint32_t)(r - array_start));
        (void)count;
        send_method_return(c,m->serial,"as",rb,r); return;
    }
    if (strcmp(member, "AddMatch") == 0 || strcmp(member, "RemoveMatch") == 0) {
        if (!parse_string(body, body_len, &off, s1, sizeof(s1))) { send_error(c,m->serial,"org.freedesktop.DBus.Error.InvalidArgs","Expected match rule"); return; }
        if (strcmp(member,"AddMatch") == 0) {
            MatchRule mr;
            if (!parse_match_rule(s1,&mr) || c->match_count >= DBUS_MAX_MATCHES) { send_error(c,m->serial,"org.freedesktop.DBus.Error.MatchRuleInvalid","Invalid match rule"); return; }
            c->matches[c->match_count++] = mr;
        } else {
            MatchRule target; int i;
            if (!parse_match_rule(s1,&target)) { send_error(c,m->serial,"org.freedesktop.DBus.Error.MatchRuleInvalid","Invalid match rule"); return; }
            for (i=0;i<c->match_count;++i) {
                if (memcmp(&target,&c->matches[i],sizeof(MatchRule))==0) { memmove(&c->matches[i],&c->matches[i+1],(size_t)(c->match_count-i-1)*sizeof(MatchRule)); --c->match_count; break; }
            }
        }
        send_method_return(c,m->serial,"",NULL,0); return;
    }
    if (strcmp(member, "GetConnectionUnixUser") == 0 || strcmp(member, "GetConnectionUnixProcessID") == 0) {
        int owner;
        if (!parse_string(body, body_len, &off, s1, sizeof(s1))) { send_error(c,m->serial,"org.freedesktop.DBus.Error.InvalidArgs","Expected bus name"); return; }
        owner = owner_of_name(s1);
        if (owner < 0) { send_error(c,m->serial,"org.freedesktop.DBus.Error.NameHasNoOwner","Name has no owner"); return; }
        { uint8_t rb[4]; uint32_t v = strcmp(member,"GetConnectionUnixUser")==0 ? clients[owner].uid : clients[owner].pid; size_t r=body_u32(rb,0,v); send_method_return(c,m->serial,"u",rb,r); }
        return;
    }
    send_error(c,m->serial,"org.freedesktop.DBus.Error.UnknownMethod","Unknown method");
    (void)destination;
}

static int rebuild_method_call_with_sender(const DBusMessageView *m, const uint8_t *buf, size_t len,
                                            const char *sender, uint8_t *out, size_t out_cap, size_t *out_len) {
    size_t old_fields = m->fields_len;
    size_t new_fields_pos = 16 + align8(old_fields);
    size_t sender_end;
    size_t new_header_size;
    if (16 + old_fields > len) return 0;
    if (new_fields_pos > out_cap) return 0;
    memcpy(out, buf, 16 + old_fields);
    {
        size_t p = 16 + old_fields;
        p = align8(p);
        p = add_str_field(out, p, DBUS_HEADER_SENDER, 's', sender);
        new_header_size = align8(p);
        sender_end = p;
        (void)sender_end;
        if (new_header_size + m->body_len > out_cap) return 0;
        if (m->body_len) memcpy(out + new_header_size, buf + m->header_size, m->body_len);
        out[0]='l';
        wr_u32(out + 12, (uint32_t)(new_header_size - 16));
        wr_u32(out + 4, m->body_len);
        *out_len = new_header_size + m->body_len;
        return 1;
    }
}

static void route_signal_and_method(int ci, const DBusMessageView *m, const uint8_t *buf, size_t len) {
    const char *path = NULL, *iface = NULL, *member = NULL, *destination = NULL;
    DBusHeaderField *f;
    char pathbuf[DBUS_PATH_MAX], ifacebuf[DBUS_NAME_MAX], memberbuf[DBUS_MEMBER_MAX], destbuf[DBUS_NAME_MAX];
    Client *c = &clients[ci];
    f = find_field((DBusMessageView *)m, DBUS_HEADER_PATH); if (f) { strncpy(pathbuf,f->string_value,sizeof(pathbuf)-1); pathbuf[sizeof(pathbuf)-1]=0; path=pathbuf; }
    f = find_field((DBusMessageView *)m, DBUS_HEADER_INTERFACE); if (f) { strncpy(ifacebuf,f->string_value,sizeof(ifacebuf)-1); ifacebuf[sizeof(ifacebuf)-1]=0; iface=ifacebuf; }
    f = find_field((DBusMessageView *)m, DBUS_HEADER_MEMBER); if (f) { strncpy(memberbuf,f->string_value,sizeof(memberbuf)-1); memberbuf[sizeof(memberbuf)-1]=0; member=memberbuf; }
    f = find_field((DBusMessageView *)m, DBUS_HEADER_DESTINATION); if (f) { strncpy(destbuf,f->string_value,sizeof(destbuf)-1); destbuf[sizeof(destbuf)-1]=0; destination=destbuf; }

    if (m->type == DBUS_MESSAGE_METHOD_CALL) {
        if (destination && strcmp(destination, BUS_UNIQUE_NAME) == 0) {
            handle_bus_call(ci,m,buf,len,path,iface,member,destination); return;
        }
        {
            int owner = destination ? owner_of_name(destination) : -1;
            if (owner < 0) { send_error(c,m->serial,"org.freedesktop.DBus.Error.ServiceUnknown","The requested service is unknown"); return; }
            {
                uint8_t relay[DBUS_MAX_MESSAGE]; size_t total = 0;
                if (!rebuild_method_call_with_sender(m, buf, len, c->unique, relay, sizeof(relay), &total)) {
                    send_error(c,m->serial,"org.freedesktop.DBus.Error.LimitsExceeded","Message too large");
                    return;
                }
                send_raw(clients[owner].fd,relay,total);
            }
        }
        return;
    }
}

static int auth_line(Client *c, const char *line) {
    if (strncmp(line,"AUTH EXTERNAL",13)==0) {
        const char *p=line+13;
        while (*p==' ') ++p;
        if (*p) {
            uint32_t uid=0; int any=0;
            while (*p) {
                int d;
                if (*p>='0'&&*p<='9') d=*p-'0'; else if (*p>='A'&&*p<='F') d=*p-'A'+10; else if (*p>='a'&&*p<='f') d=*p-'a'+10; else break;
                uid=(uid<<4)|(uint32_t)d; any=1; ++p;
            }
            if (any) c->uid=uid;
        }
        write(c->fd,"OK " BUS_GUID "\r\n",3+strlen(BUS_GUID)+2);
        return 1;
    }
    if (strcmp(line,"BEGIN")==0) { c->authenticated=1; return 1; }
    if (strcmp(line,"NEGOTIATE_UNIX_FD")==0) { write(c->fd,"REJECTED\r\n",10); return 1; }
    if (strcmp(line,"CANCEL")==0) { write(c->fd,"REJECTED\r\n",10); return 1; }
    write(c->fd,"REJECTED\r\n",10); return 1;
}

static int process_auth(Client *c, const uint8_t *buf, size_t n, size_t *used) {
    size_t p=0;
    *used=0;
    if (c->authenticated) return 1;
    if (c->authlen==0 && n && buf[0]==0) { p=1; }
    while (p<n && !c->authenticated) {
        size_t i;
        if (c->authlen >= sizeof(c->authbuf)-1) return 0;
        c->authbuf[c->authlen++] = buf[p++];
        for (i=0;i<c->authlen;i++) {
            if (c->authbuf[i]=='\r'||c->authbuf[i]=='\n') {
                size_t line_len=i; char line[2048]; size_t take=line_len<sizeof(line)-1?line_len:sizeof(line)-1;
                memcpy(line,c->authbuf,take); line[take]=0;
                size_t consume=i+1;
                while (consume<c->authlen && (c->authbuf[consume]=='\r'||c->authbuf[consume]=='\n')) ++consume;
                memmove(c->authbuf,c->authbuf+consume,c->authlen-consume); c->authlen-=consume;
                auth_line(c,line); break;
            }
        }
    }
    *used=p;
    return 1;
}

static int dispatch_buffer(Client *c, int ci) {
    while (c->inlen >= 16) {
        uint32_t body_len, fields_len;
        size_t total;
        DBusMessageView m;
        if (c->inbuf[0] != 'l') return 0;
        body_len = rd_u32(c->inbuf+4); fields_len=rd_u32(c->inbuf+12);
        total = align8(16 + (size_t)fields_len) + (size_t)body_len;
        if (total > DBUS_MAX_MESSAGE) return 0;
        if (c->inlen < total) return 1;
        if (!parse_message(c->inbuf,total,&m)) return 0;
        route_signal_and_method(ci,&m,c->inbuf,total);
        if (total < c->inlen) memmove(c->inbuf,c->inbuf+total,c->inlen-total);
        c->inlen -= total;
    }
    return 1;
}

static void make_dirs(void) {
    mkdir("/run",0755);
    mkdir("/run/dbus",0755);
    mkdir("/run/user",0755);
    mkdir("/run/user/0",0700);
}

int main(int argc, char **argv) {
    int s, i;
    if (argc > 1 && argv[1] && strcmp(argv[1],"--session")==0) { session_bus=1; strncpy(bus_path,BUS_SESSION_PATH,sizeof(bus_path)-1); }
    memset(clients,0,sizeof(clients)); memset(names,0,sizeof(names));
    make_dirs();
    s=socket(AF_UNIX,SOCK_STREAM,0);
    if (s<0) { printf("dbusd: socket failed errno=%d\n",errno); return 1; }
    unlink(bus_path);
    {
        struct sockaddr_un a; memset(&a,0,sizeof(a)); a.sun_family=AF_UNIX;
        strncpy(a.sun_path,bus_path,sizeof(a.sun_path)-1);
        if (bind(s,(struct sockaddr*)&a,(socklen_t)(2+strlen(a.sun_path)+1))<0) { printf("dbusd: bind %s failed errno=%d\n",bus_path,errno); close(s); return 1; }
    }
    if (listen(s,64)<0) { printf("dbusd: listen failed errno=%d\n",errno); close(s); return 1; }
    printf("dbusd: %s bus listening at %s\n",session_bus?"session":"system",bus_path);

    for (;;) {
        struct pollfd pfds[DBUS_MAX_CLIENTS+1];
        int map[DBUS_MAX_CLIENTS+1];
        int pn=1;
        pfds[0].fd=s; pfds[0].events=POLLIN; pfds[0].revents=0; map[0]=-1;
        for (i=0;i<DBUS_MAX_CLIENTS;++i) if (clients[i].used) {
            pfds[pn].fd=clients[i].fd; pfds[pn].events=POLLIN; pfds[pn].revents=0; map[pn]=i; ++pn;
        }
        if (poll(pfds,(size_t)pn,-1)<0) continue;
        if (pfds[0].revents&POLLIN) {
            int fd=accept(s,NULL,NULL);
            if (fd>=0) {
                int ci=alloc_client();
                if (ci<0) close(fd); else clients[ci].fd=fd;
            }
        }
        for (i=1;i<pn;++i) {
            int ci=map[i];
            if (ci<0 || !clients[ci].used) continue;
            if (pfds[i].revents&(POLLERR|POLLHUP)) {
                release_all_client_names(ci); client_close(ci); continue;
            }
            if (pfds[i].revents&POLLIN) {
                uint8_t temp[8192]; ssize_t n=read(clients[ci].fd,temp,sizeof(temp));
                if (n<=0) { release_all_client_names(ci); client_close(ci); continue; }
                size_t used=0;
                if (!process_auth(&clients[ci],temp,(size_t)n,&used)) { release_all_client_names(ci); client_close(ci); continue; }
                if (clients[ci].authenticated && used < (size_t)n) {
                    size_t add=(size_t)n-used;
                    if (add > sizeof(clients[ci].inbuf)-clients[ci].inlen) { release_all_client_names(ci); client_close(ci); continue; }
                    memcpy(clients[ci].inbuf+clients[ci].inlen,temp+used,add); clients[ci].inlen+=add;
                }
                if (clients[ci].authenticated && !dispatch_buffer(&clients[ci],ci)) { release_all_client_names(ci); client_close(ci); }
            }
        }
    }
    return 0;
}
