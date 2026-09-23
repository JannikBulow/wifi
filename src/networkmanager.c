// Copyright 2026 Jannik Laugmand Bülow

#include "log.h"
#include "networkmanager.h"

#include <dbus/dbus.h>

static const char* const NETWORKMANAGER_PATH = "/org/freedesktop/NetworkManager";
static const char* const NETWORKMANAGER_INTERFACE = "org.freedesktop.NetworkManager";

static DBusConnection* connection = NULL;

static bool get_property(const char* path, const char* interface, const char* property, DBusMessageIter* value) {
    DBusMessage* message = dbus_message_new_method_call(interface, path, "org.freedesktop.DBus.Properties", "Get");
    dbus_message_append_args(message,
        DBUS_TYPE_STRING, &interface,
        DBUS_TYPE_STRING, &property,
        DBUS_TYPE_INVALID
    );

    DBusError error;
    dbus_error_init(&error);

    DBusMessage* reply = dbus_connection_send_with_reply_and_block(connection, message, -1, &error);

    dbus_message_unref(message);

    if (dbus_error_is_set(&error)) {
        log_error("D-Bus error: %s", error.message);
        dbus_error_free(&error);
        return false;
    }

    DBusMessageIter iter;
    dbus_message_iter_init(reply, &iter);

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
        dbus_message_unref(reply);
        return false;
    }

    dbus_message_iter_recurse(&iter, value);

    return true;
}

static bool get_boolean_property(const char* path, const char* interface, const char* property, bool* value) {
    DBusMessageIter variant;
    if (!get_property(path, interface, property, &variant)) {
        return false;
    }

    if (dbus_message_iter_get_arg_type(&variant) != DBUS_TYPE_BOOLEAN) {
        return false;
    }

    dbus_bool_t enabled;
    dbus_message_iter_get_basic(&variant, &enabled);

    *value = enabled;
    return true;
}

bool nm_init(void) {
    DBusError error;
    dbus_error_init(&error);

    connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);

    if (dbus_error_is_set(&error)) {
        log_error("D-Bus error: %s", error.message);
        dbus_error_free(&error);
        return false;
    }

    return true;
}

void nm_shutdown(void) {
    if (connection) {
        dbus_connection_unref(connection);
        connection = NULL;
    }
}

bool nm_wifi_enabled(void) {
    bool enabled;
    if (!get_boolean_property(NETWORKMANAGER_PATH, NETWORKMANAGER_INTERFACE, "WirelessEnabled", &enabled)) {
        return false;
    }

    return enabled;
}

bool nm_wifi_set_enabled(bool enabled) {}

bool nm_scan(Network** networks, size_t* count) {}

void nm_free_networks(Network* networks, size_t count) {}

bool nm_connect(const char* ssid, const char* password) {}

bool nm_disconnect(void) {}

bool nm_forget(const char* ssid) {}
