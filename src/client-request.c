/*
    Copyright (C) 2014-2018 Flexible Software Solutions S.L.U.

    This file is part of flexVDI Client.

    flexVDI Client is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    flexVDI Client is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flexVDI Client. If not, see <https://www.gnu.org/licenses/>.
*/

#include <string.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

#include "client-request.h"


struct _ClientRequest {
    GObject parent;
    SoupSession * soup;
    GCancellable * cancel_mgr_request;
    JsonParser * parser;
    ClientRequestCallback cb;
    gpointer user_data;
    GError * error;
    GInputStream * stream;
};

G_DEFINE_TYPE(ClientRequest, client_request, G_TYPE_OBJECT);


static void client_request_init(ClientRequest * req) {
    req->cancel_mgr_request = g_cancellable_new();
}


static void client_request_dispose(GObject * obj) {
    ClientRequest * req = CLIENT_REQUEST(obj);

    if (req->cancel_mgr_request)
        g_cancellable_cancel(req->cancel_mgr_request);
    g_clear_object(&req->cancel_mgr_request);
    g_clear_object(&req->parser);
    g_clear_object(&req->stream);

    G_OBJECT_CLASS(client_request_parent_class)->dispose(obj);
}


static void client_request_finalize(GObject * obj) {
    G_OBJECT_CLASS(client_request_parent_class)->finalize(obj);
}


static void client_request_class_init(ClientRequestClass * class) {
    GObjectClass * object_class = G_OBJECT_CLASS(class);
    object_class->dispose = client_request_dispose;
    object_class->finalize = client_request_finalize;
}


void client_request_cancel(ClientRequest * req) {
    if (req->cancel_mgr_request) {
        g_cancellable_cancel(req->cancel_mgr_request);
    }
}


JsonNode * client_request_get_result(ClientRequest * req, GError ** error) {
    if (error)
        *error = req->error;
    if (req->parser) {
        return json_parser_get_root(req->parser);
    } else {
        return NULL;
    }
}


/*
 * Request parsed handler. Calls the callback on success.
 */
static void request_parsed_cb(GObject * object, GAsyncResult * res, gpointer user_data) {
    ClientRequest * req = CLIENT_REQUEST(user_data);
    GError * error = NULL;

    json_parser_load_from_stream_finish(JSON_PARSER(object), res, &error);
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        g_debug("Request cancelled");
        g_object_unref(req);
        return;
    }

    req->error = error;
    if (!req->error) {
        g_autoptr(JsonGenerator) gen = json_generator_new();
        json_generator_set_root(gen, json_parser_get_root(req->parser));
        g_autofree gchar * response = json_generator_to_data(gen, NULL);
        g_debug("request response:\n%s", response);
    }

    req->cb(req, req->user_data);
    g_input_stream_close(req->stream, NULL, NULL);
    g_object_unref(req);
}


/*
 * Request finished handler. It checks whether the request was cancelled, and starts
 * parsing the response body.
 */
static void request_finished_cb(GObject * object, GAsyncResult * result, gpointer user_data) {
    ClientRequest * req = CLIENT_REQUEST(user_data);
    GError * error = NULL;
    GInputStream * stream = soup_session_send_finish(SOUP_SESSION(object), result, &error);

    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        g_debug("Request cancelled");
        g_object_unref(req);
        return;
    }

    req->error = error;
    req->stream = g_object_ref(stream);
    if (req->error) {
        req->cb(req, req->user_data);
        g_object_unref(req);
    } else {
        req->parser = json_parser_new();
        json_parser_load_from_stream_async(req->parser, stream, req->cancel_mgr_request,
                                           request_parsed_cb, req);
    }
}


static ClientRequest * client_request_new_base(ClientConf * conf,
        ClientRequestCallback cb, gpointer user_data) {
    ClientRequest * req = CLIENT_REQUEST(g_object_new(CLIENT_REQUEST_TYPE, NULL));
    req->cb = cb;
    req->user_data = user_data;
    req->soup = client_conf_get_soup_session(conf);
    return req;
}

ClientRequest * client_request_new(ClientConf * conf, const gchar * path,
        ClientRequestCallback cb, gpointer user_data) {
    ClientRequest * req = client_request_new_base(conf, cb, user_data);

    g_autofree gchar * uri = client_conf_get_connection_uri(conf, path);
    SoupMessage * msg = soup_message_new("GET", uri);
    const gchar * token = client_conf_get_token(conf);
    if (token != NULL) {
        soup_message_headers_replace(msg->request_headers, "X-Auth-Token", token);
    }

    g_debug("GET request to %s", uri);
    soup_session_send_async(req->soup, msg, req->cancel_mgr_request,
                            request_finished_cb, g_object_ref(req));

    return req;
}


ClientRequest * client_request_new_with_data(ClientConf * conf, const gchar * path,
        const gchar * post_data, const gchar * loggable_post_data,
        ClientRequestCallback cb, gpointer user_data) {
    ClientRequest * req = client_request_new_base(conf, cb, user_data);

    g_autofree gchar * uri = client_conf_get_connection_uri(conf, path);
    SoupMessage * msg = soup_message_new("POST", uri);
    const gchar * token = client_conf_get_token(conf);
    if (token != NULL) {
        soup_message_headers_replace(msg->request_headers, "X-Auth-Token", token);
    }
    
    g_debug("POST request to %s, body:\n%s", uri, loggable_post_data);
    soup_message_set_request(msg, "text/json", SOUP_MEMORY_COPY, post_data, strlen(post_data));
    soup_session_send_async(req->soup, msg, req->cancel_mgr_request,
                            request_finished_cb, g_object_ref(req));

    return req;
}


// PUT request
ClientRequest * client_request_new_with_data2(ClientConf * conf, const gchar * path,
        const gchar * post_data, const gchar * loggable_post_data,
        ClientRequestCallback cb, gpointer user_data) {

    ClientRequest * req = client_request_new_base(conf, cb, user_data);

    g_autofree gchar * uri = client_conf_get_connection_uri(conf, path);
    SoupMessage * msg = soup_message_new("PUT", uri);
    const gchar * token = client_conf_get_token(conf);
    if (token != NULL) {
        soup_message_headers_replace(msg->request_headers, "X-Auth-Token", token);
    }
    
    g_debug("PUT request to %s, body:\n%s", uri, loggable_post_data);
    soup_message_set_request(msg, "text/json", SOUP_MEMORY_COPY, post_data, strlen(post_data));
    soup_session_send_async(req->soup, msg, req->cancel_mgr_request,
                            request_finished_cb, g_object_ref(req));

    return req;
}
