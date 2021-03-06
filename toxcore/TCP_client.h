/*
 * Implementation of the TCP relay client part of Tox.
 */

/*
 * Copyright © 2016-2017 The TokTok team.
 * Copyright © 2014 Tox project.
 *
 * This file is part of Tox, the free peer to peer instant messenger.
 *
 * Tox is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Tox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Tox.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include "TCP_server.h"
#include "crypto_core.h"

#ifdef HAVE_LIBEV
#include <ev.h>
#elif HAVE_LIBEVENT
#include <event2/event.h>
#endif

#define TCP_CONNECTION_TIMEOUT 10

typedef enum {
    TCP_PROXY_NONE,
    TCP_PROXY_HTTP,
    TCP_PROXY_SOCKS5
} TCP_PROXY_TYPE;

typedef struct {
    IP_Port ip_port;
    uint8_t proxy_type; // a value from TCP_PROXY_TYPE
} TCP_Proxy_Info;

enum {
    TCP_CLIENT_NO_STATUS,
    TCP_CLIENT_PROXY_HTTP_CONNECTING,
    TCP_CLIENT_PROXY_SOCKS5_CONNECTING,
    TCP_CLIENT_PROXY_SOCKS5_UNCONFIRMED,
    TCP_CLIENT_CONNECTING,
    TCP_CLIENT_UNCONFIRMED,
    TCP_CLIENT_CONFIRMED,
    TCP_CLIENT_DISCONNECTED,
};
typedef struct  {
    uint8_t status;
    Socket sock;
#ifdef HAVE_LIBEV
    struct {
        ev_io listener;
        struct ev_loop *dispatcher;
    } sock_listener;
#elif HAVE_LIBEVENT
    struct event *sock_listener;
#endif
    uint8_t self_public_key[CRYPTO_PUBLIC_KEY_SIZE]; /* our public key */
    uint8_t public_key[CRYPTO_PUBLIC_KEY_SIZE]; /* public key of the server */
    IP_Port ip_port; /* The ip and port of the server */
    TCP_Proxy_Info proxy_info;
    uint8_t recv_nonce[CRYPTO_NONCE_SIZE]; /* Nonce of received packets. */
    uint8_t sent_nonce[CRYPTO_NONCE_SIZE]; /* Nonce of sent packets. */
    uint8_t shared_key[CRYPTO_SHARED_KEY_SIZE];
    uint16_t next_packet_length;

    uint8_t temp_secret_key[CRYPTO_SECRET_KEY_SIZE];

    uint8_t last_packet[2 + MAX_PACKET_SIZE];
    uint16_t last_packet_length;
    uint16_t last_packet_sent;

    TCP_Priority_List *priority_queue_start, *priority_queue_end;

    uint64_t kill_at;

    uint64_t last_pinged;
    uint64_t ping_id;

    uint64_t ping_response_id;
    uint64_t ping_request_id;

    struct {
        uint8_t status; /* 0 if not used, 1 if other is offline, 2 if other is online. */
        uint8_t public_key[CRYPTO_PUBLIC_KEY_SIZE];
        uint32_t number;
    } connections[NUM_CLIENT_CONNECTIONS];
    int (*response_callback)(void *object, uint8_t connection_id, const uint8_t *public_key);
    void *response_callback_object;
    int (*status_callback)(void *object, uint32_t number, uint8_t connection_id, uint8_t status);
    void *status_callback_object;
    int (*data_callback)(void *object, uint32_t number, uint8_t connection_id, const uint8_t *data, uint16_t length,
                         void *userdata);
    void *data_callback_object;
    int (*oob_data_callback)(void *object, const uint8_t *public_key, const uint8_t *data, uint16_t length, void *userdata);
    void *oob_data_callback_object;

    int (*onion_callback)(void *object, const uint8_t *data, uint16_t length, void *userdata);
    void *onion_callback_object;

    /* Can be used by user. */
    void *custom_object;
    uint32_t custom_uint;
} TCP_Client_Connection;

/* Create new TCP connection to ip_port/public_key
 */
TCP_Client_Connection *new_TCP_connection(IP_Port ip_port, const uint8_t *public_key, const uint8_t *self_public_key,
        const uint8_t *self_secret_key, TCP_Proxy_Info *proxy_info);

/* Run the TCP connection
 */
void do_TCP_connection(TCP_Client_Connection *TCP_connection, void *userdata);

/* Kill the TCP connection
 */
void kill_TCP_connection(TCP_Client_Connection *TCP_connection);

/* return 1 on success.
 * return 0 if could not send packet.
 * return -1 on failure (connection must be killed).
 */
int send_onion_request(TCP_Client_Connection *con, const uint8_t *data, uint16_t length);
void onion_response_handler(TCP_Client_Connection *con, int (*onion_callback)(void *object, const uint8_t *data,
                            uint16_t length, void *userdata), void *object);

/* return 1 on success.
 * return 0 if could not send packet.
 * return -1 on failure (connection must be killed).
 */
int send_routing_request(TCP_Client_Connection *con, uint8_t *public_key);
void routing_response_handler(TCP_Client_Connection *con, int (*response_callback)(void *object, uint8_t connection_id,
                              const uint8_t *public_key), void *object);
void routing_status_handler(TCP_Client_Connection *con, int (*status_callback)(void *object, uint32_t number,
                            uint8_t connection_id, uint8_t status), void *object);

/* return 1 on success.
 * return 0 if could not send packet.
 * return -1 on failure (connection must be killed).
 */
int send_disconnect_request(TCP_Client_Connection *con, uint8_t con_id);

/* Set the number that will be used as an argument in the callbacks related to con_id.
 *
 * When not set by this function, the number is ~0.
 *
 * return 0 on success.
 * return -1 on failure.
 */
int set_tcp_connection_number(TCP_Client_Connection *con, uint8_t con_id, uint32_t number);

/* return 1 on success.
 * return 0 if could not send packet.
 * return -1 on failure.
 */
int send_data(TCP_Client_Connection *con, uint8_t con_id, const uint8_t *data, uint16_t length);
void routing_data_handler(TCP_Client_Connection *con, int (*data_callback)(void *object, uint32_t number,
                          uint8_t connection_id, const uint8_t *data, uint16_t length, void *userdata), void *object);

/* return 1 on success.
 * return 0 if could not send packet.
 * return -1 on failure.
 */
int send_oob_packet(TCP_Client_Connection *con, const uint8_t *public_key, const uint8_t *data, uint16_t length);
void oob_data_handler(TCP_Client_Connection *con, int (*oob_data_callback)(void *object, const uint8_t *public_key,
                      const uint8_t *data, uint16_t length, void *userdata), void *object);


#endif
