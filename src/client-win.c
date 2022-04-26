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

#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>

#include "client-win.h"


struct _ClientAppWindow {
    GtkApplicationWindow parent;
    GtkLabel * version;
    GtkLabel * info;
    GtkLabel * status;
    GtkRevealer * status_revealer;
    GtkStack * stack;
    GtkEntry * host;
    GtkEntry * port;
    GtkCheckButton * fullscreen;
    GtkEntry * proxy;
    GtkButton * config;
    GtkButton * about;
    GtkButton * save;
    GtkButton * discard;
    GtkButton * login;
    GtkButton * connect;
    GtkButton * back;
    GtkEntry * username;
    GtkEntry * password;
    GtkComboBox * authenticator;
    GtkListStore * authenticator_store;
    GtkTreeView * desktops;
    GtkListStore * desk_store;
    GtkImage * logo_toolbar;
    GtkImage * logo_client;
    guint error_timeout;
    guint status_animation;
};

enum {
    CLIENT_APP_BUTTON_PRESSED = 0,
    CLIENT_APP_DESKTOP_SELECTED,
    CLIENT_APP_LAST_SIGNAL
};

static guint signals[CLIENT_APP_LAST_SIGNAL];

G_DEFINE_TYPE(ClientAppWindow, client_app_window, GTK_TYPE_APPLICATION_WINDOW);


static void client_app_window_dispose(GObject * obj);

static void client_app_window_class_init(ClientAppWindowClass * class) {
    G_OBJECT_CLASS(class)->dispose = client_app_window_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                                "/com/flexvdi/client/window.ui");
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, version);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, info);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, status);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, status_revealer);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, stack);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, host);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, port);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, fullscreen);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, proxy);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, config);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, about);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, save);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, discard);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, login);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, connect);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, back);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, username);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, password);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, authenticator);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, desktops);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, logo_toolbar);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ClientAppWindow, logo_client);

    // Emited when the user presses the configuration button (with a small gear wheel)
    signals[CLIENT_APP_BUTTON_PRESSED] =
        g_signal_new("button-pressed",
                     CLIENT_APP_WINDOW_TYPE,
                     G_SIGNAL_RUN_FIRST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__INT,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_INT);

    // Emited when the user presses enter or space on the desktop list, the connect button
    // or double-clicks on a desktop name
    signals[CLIENT_APP_DESKTOP_SELECTED] =
        g_signal_new("desktop-selected",
                     CLIENT_APP_WINDOW_TYPE,
                     G_SIGNAL_RUN_FIRST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);
}


static void button_pressed_handler(GtkButton * button, gpointer user_data);
static void entry_activate_handler(GtkEntry * entry, gpointer user_data);
static void desktop_selected_handler(GtkTreeView * tree_view, GtkTreePath * path,
                                     GtkTreeViewColumn * column, gpointer user_data);
gchar* format_entry_text(GtkComboBox* self, gchar* path, gpointer user_data);

static void client_app_window_init(ClientAppWindow * win) {
    gtk_widget_init_template(GTK_WIDGET(win));

    gtk_label_set_text(win->version, "version " VERSION_STRING);
    win->authenticator_store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    gtk_combo_box_set_model(win->authenticator, GTK_TREE_MODEL(win->authenticator_store));
    gtk_combo_box_set_entry_text_column(win->authenticator, 2);
    gtk_combo_box_set_id_column(win->authenticator, 0);
    win->desk_store = gtk_list_store_new(1, G_TYPE_STRING);
    gtk_tree_view_set_model(win->desktops, GTK_TREE_MODEL(win->desk_store));

    g_signal_connect(win->config, "clicked", G_CALLBACK(button_pressed_handler), win);
    g_signal_connect(win->about, "clicked", G_CALLBACK(button_pressed_handler), win);
    g_signal_connect(win->save, "clicked", G_CALLBACK(button_pressed_handler), win);
    g_signal_connect(win->discard, "clicked", G_CALLBACK(button_pressed_handler), win);
    g_signal_connect(win->login, "clicked", G_CALLBACK(button_pressed_handler), win);
    g_signal_connect(win->connect, "clicked", G_CALLBACK(button_pressed_handler), win);
    g_signal_connect(win->back, "clicked", G_CALLBACK(button_pressed_handler), win);
    g_signal_connect(win->password, "activate", G_CALLBACK(entry_activate_handler), win);
    g_signal_connect(win->authenticator, "format-entry-text", G_CALLBACK(format_entry_text), win);
    g_signal_connect(win->desktops, "row-activated", G_CALLBACK(desktop_selected_handler), win);

    GtkEntry * au_entry = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(win->authenticator)));
    gtk_entry_set_icon_from_icon_name(au_entry, GTK_ENTRY_ICON_PRIMARY, "authenticator");
    gtk_entry_set_placeholder_text(au_entry, "- select authenticator -");
    gtk_editable_set_editable(GTK_EDITABLE(au_entry), FALSE);
}


static void client_app_window_dispose(GObject * obj) {
    ClientAppWindow * win = CLIENT_APP_WINDOW(obj);
    g_clear_object(&win->authenticator_store);
    g_clear_object(&win->desk_store);
    if (win->error_timeout > 0) {
        g_source_remove(win->error_timeout);
        win->error_timeout = 0;
    }
    if (win->status_animation > 0) {
        g_source_remove(win->status_animation);
        win->status_animation = 0;
    }
    G_OBJECT_CLASS(client_app_window_parent_class)->dispose(obj);
}


ClientAppWindow * client_app_window_new(ClientApp * app, ClientConf * conf) {
    ClientAppWindow * win = g_object_new(CLIENT_APP_WINDOW_TYPE, "application", app, NULL);

    /* Set the CSS rules for all the widgets in the current screen. This includes other
       windows too, but it is done here because this is the first window and there is
       only one */
    GtkCssProvider * css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(css_provider, "/com/flexvdi/client/style.css");
    GdkScreen * screen = gtk_widget_get_screen(GTK_WIDGET(win));
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER + 1);

    // Add custom style
    g_autofree gchar * custom_css_file = client_conf_get_custom_style_file(conf);
    if (custom_css_file != NULL) {
        css_provider = gtk_css_provider_new();
        g_debug("Loading custom CSS from %s", custom_css_file);
        gtk_css_provider_load_from_path(css_provider, custom_css_file, NULL);
        gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(css_provider),
                                                  GTK_STYLE_PROVIDER_PRIORITY_USER + 2);
    }

    g_autofree gchar * custom_toolbar_logo = client_conf_get_custom_toolbar_logo(conf);
    if (custom_toolbar_logo != NULL) {
        g_debug("Loading custom toolbar logo from %s", custom_toolbar_logo);
        GdkPixbuf * toolbar_logo_img = gdk_pixbuf_new_from_file(custom_toolbar_logo, NULL);
        if (toolbar_logo_img != NULL)
            gtk_image_set_from_pixbuf(win->logo_toolbar, toolbar_logo_img);
    }

    g_autofree gchar * custom_client_logo = client_conf_get_custom_client_logo(conf);
    if (custom_client_logo != NULL) {
        g_debug("Loading custom client logo from %s", custom_client_logo);
        GdkPixbuf * client_logo_img = gdk_pixbuf_new_from_file(custom_client_logo, NULL);
        if (client_logo_img != NULL)
            gtk_image_set_from_pixbuf(win->logo_client, client_logo_img);
    }

    return win;
}


static gboolean validate_settings(ClientAppWindow * win);

/*
 * Button pressed handler, for all the buttons.
 */
static void button_pressed_handler(GtkButton * button, gpointer user_data) {
    ClientAppWindow * win = CLIENT_APP_WINDOW(user_data);

    if (button == win->config)
        g_signal_emit(win, signals[CLIENT_APP_BUTTON_PRESSED], 0, SETTINGS_BUTTON);
    else if (button == win->about)
        g_signal_emit(win, signals[CLIENT_APP_BUTTON_PRESSED], 0, ABOUT_BUTTON);
    else if (button == win->save) {
        if (validate_settings(win)) {
            g_signal_emit(win, signals[CLIENT_APP_BUTTON_PRESSED], 0, SAVE_BUTTON);
        }
    } else if (button == win->login)
        g_signal_emit(win, signals[CLIENT_APP_BUTTON_PRESSED], 0, LOGIN_BUTTON);
    else if (button == win->connect)
        g_signal_emit(win, signals[CLIENT_APP_DESKTOP_SELECTED], 0);
    else if (button == win->back)
        g_signal_emit(win, signals[CLIENT_APP_BUTTON_PRESSED], 0, BACK_BUTTON);
    else if (button == win->discard)
        g_signal_emit(win, signals[CLIENT_APP_BUTTON_PRESSED], 0, DISCARD_BUTTON);
}


/*
 * Entry box activated, for login when the user presses enter on the password box.
 */
static void entry_activate_handler(GtkEntry * entry, gpointer user_data) {
    ClientAppWindow * win = CLIENT_APP_WINDOW(user_data);
    if (entry == win->password)
        g_signal_emit(win, signals[CLIENT_APP_BUTTON_PRESSED], 0, LOGIN_BUTTON);
}


/*
 * Authenticator selection changed, for login.
 */
gchar* format_entry_text(GtkComboBox* self, gchar* path, gpointer user_data) {
    ClientAppWindow * win = CLIENT_APP_WINDOW(user_data);

    GtkTreeIter iter;
    gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(win->authenticator_store), &iter, path);
    gchar * authenticator_name = NULL;
    gtk_tree_model_get(GTK_TREE_MODEL(win->authenticator_store), &iter, 0, &authenticator_name, -1);

    return g_strdup(authenticator_name);
}


/*
 * Desktop selected handler, for double-clicks on the desktop list.
 */
static void desktop_selected_handler(GtkTreeView * tree_view, GtkTreePath * path,
                                     GtkTreeViewColumn * column, gpointer user_data) {
    ClientAppWindow * win = CLIENT_APP_WINDOW(user_data);
    g_signal_emit(win, signals[CLIENT_APP_DESKTOP_SELECTED], 0);
}


void client_app_window_load_config(ClientAppWindow * win, ClientConf * conf) {
    const gchar * host = client_conf_get_host(conf);
    const gchar * port = client_conf_get_port(conf);
    const gchar * username = client_conf_get_username(conf);
    const gchar * password = client_conf_get_password(conf);
    const gchar * proxy_uri = client_conf_get_proxy_uri(conf);

    if (host)
        gtk_entry_set_text(win->host, host);

    gtk_entry_set_text(win->port, port ? port : "443");

    if (username)
        gtk_entry_set_text(win->username, username);

    if (password)
        gtk_entry_set_text(win->password, password);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(win->fullscreen),
        client_conf_get_fullscreen(conf));

    if (proxy_uri)
        gtk_entry_set_text(win->proxy, proxy_uri);
}


static gboolean validate_settings(ClientAppWindow * win) {
    const gchar * host = gtk_entry_get_text(win->host);

    if (!g_strcmp0(host, "")) {
        client_app_window_error(win, "You must enter a value for the connection address.");
        return FALSE;
    }

    return TRUE;
}


void client_app_window_save_config(ClientAppWindow * win, ClientConf * conf) {
    const gchar * host = gtk_entry_get_text(win->host);
    const gchar * port = gtk_entry_get_text(win->port);
    const gchar * proxy_uri = gtk_entry_get_text(win->proxy);

    client_conf_set_host(conf, host);
    client_conf_set_port(conf, g_strcmp0(port, "443") ? port : NULL);
    client_conf_set_fullscreen(conf,
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(win->fullscreen)));
    client_conf_set_proxy_uri(conf, g_strcmp0(proxy_uri, "") ? proxy_uri : NULL);
}


void client_app_window_set_info(ClientAppWindow * win, const gchar * text) {
    gtk_label_set_text(win->info, text);
}


static gboolean error_status_timeout(gpointer user_data);

static void client_app_window_set_status(ClientAppWindow * win, gboolean error,
                                         const gchar * text) {
    gtk_label_set_text(win->status, text);
    GtkStyleContext * style = gtk_widget_get_style_context(GTK_WIDGET(win->status));
    if (error) {
        gtk_style_context_add_class(style, "error");
        // Hide error messages after 10 seconds
        if (win->error_timeout > 0)
            g_source_remove(win->error_timeout);
        win->error_timeout = g_timeout_add_seconds(10, error_status_timeout, win);
    } else {
        gtk_style_context_remove_class(style, "error");
    }
    gtk_revealer_set_reveal_child(win->status_revealer, TRUE);
}


/*
 * Status animation, stop it when the status message is hidden
 */
static gboolean status_animation(gpointer user_data) {
    ClientAppWindow * win = CLIENT_APP_WINDOW(user_data);
    GtkStyleContext * style = gtk_widget_get_style_context(GTK_WIDGET(win->status));
    if (!gtk_revealer_get_reveal_child(win->status_revealer) ||
        gtk_style_context_has_class(style, "error")) {
        win->status_animation = 0;
        return G_SOURCE_REMOVE;
    }

    const gchar * text = gtk_label_get_text(win->status);
    g_autofree gchar * new_text;
    if (g_str_has_suffix(text, "..."))
        new_text = g_strndup(text, strlen(text) - 2);
    else new_text = g_strdup_printf("%s%s", text, ".");
    gtk_label_set_text(win->status, new_text);
    return G_SOURCE_CONTINUE;
}


void client_app_window_status(ClientAppWindow * win, const gchar * text) {
    client_app_window_set_status(win, FALSE, text);
    if (win->status_animation > 0)
        g_source_remove(win->status_animation);
    win->status_animation = g_timeout_add(300, status_animation, win);
}


void client_app_window_error(ClientAppWindow * win, const gchar * text) {
    client_app_window_set_status(win, TRUE, text);
}


void client_app_window_hide_status(ClientAppWindow * win) {
    // Hide the status message only if it is not an error message
    GtkStyleContext * style = gtk_widget_get_style_context(GTK_WIDGET(win->status));
    if (!gtk_style_context_has_class(style, "error"))
        gtk_revealer_set_reveal_child(win->status_revealer, FALSE);
}


/*
 * Error message timeout. Hide it after 10 seconds.
 */
static gboolean error_status_timeout(gpointer user_data) {
    ClientAppWindow * win = CLIENT_APP_WINDOW(user_data);
    gtk_revealer_set_reveal_child(win->status_revealer, FALSE);
    win->error_timeout = 0;
    return G_SOURCE_REMOVE;
}


void client_app_window_set_central_widget(ClientAppWindow * win, const gchar * name) {
    gtk_stack_set_visible_child_name(win->stack, name);
    // Hide the status message when we switch pages.
    gtk_revealer_set_reveal_child(win->status_revealer, FALSE);
}


void client_app_window_set_central_widget_sensitive(ClientAppWindow * win, gboolean sensitive) {
    gtk_widget_set_sensitive(GTK_WIDGET(win->stack), sensitive);

    // Focus the correct widget when we switch pages
    if (sensitive) {
        const gchar * child_name = gtk_stack_get_visible_child_name(win->stack);

        if (!g_strcmp0(child_name, "settings")) {
            gtk_widget_grab_focus(GTK_WIDGET(win->host));
        } else if (!g_strcmp0(child_name, "login")) {
            if (!g_strcmp0(gtk_entry_get_text(win->username), ""))
                gtk_widget_grab_focus(GTK_WIDGET(win->username));
            else
                gtk_widget_grab_focus(GTK_WIDGET(win->password));
        } else if (!g_strcmp0(child_name, "desktops")) {
            gtk_widget_grab_focus(GTK_WIDGET(win->desktops));
        }
    }
}


const gchar * client_app_window_get_username(ClientAppWindow * win) {
    return gtk_entry_get_text(win->username);
}


const gchar * client_app_window_get_password(ClientAppWindow * win) {
    return gtk_entry_get_text(win->password);
}


void client_app_window_set_authenticator(ClientAppWindow * win, GList * authenticator, const gchar * active) {
    GtkTreeIter it;
    gtk_list_store_clear(win->authenticator_store);

    for (GList* au_item = authenticator; au_item != NULL; au_item = au_item->next->next->next) {
        gtk_list_store_append(win->authenticator_store, &it);
        gtk_list_store_set(win->authenticator_store, &it, 0, au_item->data, 1, au_item->next->data, 2, au_item->next->next->data, -1);

        if (active != NULL && !g_strcmp0(au_item->data, active))
            gtk_combo_box_set_active_iter(win->authenticator, &it);
    }
    // const gchar* authenticator = client_conf_get_authenticator(conf);

    // if (authenticator) {
    //     GtkTreeIter iter;
    //     gtk_tree_model_get_iter_first(GTK_TREE_MODEL(win->authenticator_store), &iter);
    //     do {
    //         gchar* authenticator_name = NULL;
    //         gtk_tree_model_get(GTK_TREE_MODEL(win->authenticator_store), &iter, 0, &authenticator_name, -1);
    //         if (g_strcmp0(authenticator_name, authenticator) == 0) {
    //             gtk_combo_box_set_active_iter(win->authenticator, &iter);
    //             break;
    //         }
    //     } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(win->authenticator_store), &iter));
    // }
}

const gchar * client_app_window_get_authenticator(ClientAppWindow * win) {
    const gchar * name = gtk_combo_box_get_active_id(win->authenticator);
    if (name == NULL)
        name = "";
    return name;
}


void client_app_window_set_desktops(ClientAppWindow * win, GList * desktop_names,
                                    const gchar * desktop) {
    GList * name;
    GtkTreeIter it;

    gtk_list_store_clear(win->desk_store);
    for (name = desktop_names; name != NULL; name = name->next) {
        gtk_list_store_append(win->desk_store, &it);
        gtk_list_store_set(win->desk_store, &it, 0, name->data, -1);
    }

    if (desktop != NULL) {
        gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(win->desk_store), &it);

        while (valid) {
            g_autofree gchar * d;
            gtk_tree_model_get(GTK_TREE_MODEL(win->desk_store), &it, 0, &d, -1);

            if (!g_strcmp0(desktop, d)) {
                GtkTreePath * path = gtk_tree_model_get_path(GTK_TREE_MODEL(win->desk_store), &it);
                GtkTreeSelection * sel = gtk_tree_view_get_selection(win->desktops);
                gtk_tree_selection_select_path(sel, path);
                g_signal_emit(win, signals[CLIENT_APP_DESKTOP_SELECTED], 0);
                gtk_tree_path_free(path);
                break;
            } else {
                valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(win->desk_store), &it);
            }
        }
    }
}

gchar * client_app_window_get_desktop(ClientAppWindow * win) {
    GtkTreeSelection * sel = gtk_tree_view_get_selection(win->desktops);
    GtkTreeModel * model;
    GtkTreeIter it;
    gchar * desktop;

    if (gtk_tree_selection_get_selected(sel, &model, &it)) {
        gtk_tree_model_get(model, &it, 0, &desktop, -1);
        return desktop;
    } else return g_strdup("");
}

void client_app_window_hide_config_button(ClientAppWindow * win) {
    gtk_widget_hide(GTK_WIDGET(win->config));
}
