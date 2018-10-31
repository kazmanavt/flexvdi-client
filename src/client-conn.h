/**
 * Copyright Flexible Software Solutions S.L. 2018
 **/

#ifndef _CLIENT_CONN_H
#define _CLIENT_CONN_H

#include <glib-object.h>
#include <json-glib/json-glib.h>
#include <spice-client-gtk.h>

#include "configuration.h"


typedef enum {
    CLIENT_CONN_DISCONNECT_NO_ERROR = 0,
    CLIENT_CONN_DISCONNECT_USER,
    CLIENT_CONN_DISCONNECT_INACTIVITY,
    CLIENT_CONN_DISCONNECT_CONN_ERROR,
    CLIENT_CONN_DISCONNECT_IO_ERROR,
    CLIENT_CONN_DISCONNECT_AUTH_ERROR,
} ClientConnDisconnectReason;

/*
 * ClientConn
 *
 * Client connection with the Spice protocol. It controls the
 * life-cycle of a Spice connection, the Spice session and
 * its channels.
 */
#define CLIENT_CONN_TYPE (client_conn_get_type())
G_DECLARE_FINAL_TYPE(ClientConn, client_conn, CLIENT, CONN, GObject)

/*
 * client_conn_new
 *
 * Create a new connection with the current configuration parameters and
 * the parameters received from the flexVDI Manager.
 */
ClientConn * client_conn_new(ClientConf * conf, JsonObject * params);

/*
 * client_conn_new_with_uri
 *
 * Create a new connection with the current configuration and the uri
 * passed with the command line.
 */
ClientConn * client_conn_new_with_uri(ClientConf * conf, const char * uri);

/*
 * client_conn_connect
 *
 * Start the connection to the Spice server.
 */
void client_conn_connect(ClientConn * conn);

/*
 * client_conn_disconnect
 *
 * Start the disconnection process from the server. This is an asynchronous
 * operation, that involves finishing the coroutines associated with each
 * channel and their sockets. Do not stop the Glib main loop, wait until it
 * runs out of events and sources.
 */
void client_conn_disconnect(ClientConn * conn, ClientConnDisconnectReason reason);

/*
 * client_conn_get_session
 *
 * Get the SpiceSession object associated with this connection.
 */
SpiceSession * client_conn_get_session(ClientConn * conn);

/*
 * client_conn_get_gtk_session
 *
 * Get the SpiceGtkSession object associated with this connection.
 */
SpiceGtkSession * client_conn_get_gtk_session(ClientConn * conn);

/*
 * client_conn_get_main_channel
 *
 * Get the SpiceMainChannel object associated with this connection,
 * once the main channel has been opened.
 */
SpiceMainChannel * client_conn_get_main_channel(ClientConn * conn);


#endif /* _CLIENT_CONN_H */
