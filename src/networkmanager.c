// Copyright 2026 Jannik Laugmand Bülow

#include "log.h"
#include "networkmanager.h"

#include <dbus/dbus.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NETWORKMANAGER_PATH "/org/freedesktop/NetworkManager"
#define NETWORKMANAGER_INTERFACE "org.freedesktop.NetworkManager"

enum NMDeviceType {
    NM_DEVICE_TYPE_WIFI = 2
};

enum NM80211ApFlags {
    NM_802_11_AP_FLAGS_PRIVACY = 0x00000001
};

static DBusConnection* connection = NULL;

static bool get_property(const char* path, const char* interface, const char* property, DBusMessage** out_reply, DBusMessageIter* value) {
    DBusMessage* message = dbus_message_new_method_call(NETWORKMANAGER_INTERFACE, path, "org.freedesktop.DBus.Properties", "Get");
    if (!message) {
        log_error("Out of memory");
        return false;
    }

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
    *out_reply = reply;

    return true;
}

static bool get_boolean_property(const char* path, const char* interface, const char* property, bool* value) {
    DBusMessage* reply;
    DBusMessageIter variant;
    if (!get_property(path, interface, property, &reply, &variant)) {
        return false;
    }

    if (dbus_message_iter_get_arg_type(&variant) != DBUS_TYPE_BOOLEAN) {
        dbus_message_unref(reply);
        return false;
    }

    dbus_bool_t enabled;
    dbus_message_iter_get_basic(&variant, &enabled);

    dbus_message_unref(reply);

    *value = enabled;
    return true;
}

static bool get_byte_property(const char* path, const char* interface, const char* property, uint8_t* value) {
    DBusMessage* reply;
    DBusMessageIter variant;
    if (!get_property(path, interface, property, &reply, &variant)) {
        return false;
    }

    if (dbus_message_iter_get_arg_type(&variant) != DBUS_TYPE_BYTE) {
        dbus_message_unref(reply);
        return false;
    }

    unsigned char byte;
    dbus_message_iter_get_basic(&variant, &byte);

    dbus_message_unref(reply);

    *value = byte;
    return true;
}

static bool get_uint32_property(const char* path, const char* interface, const char* property, uint32_t* out_value) {
    DBusMessage* reply;
    DBusMessageIter variant;
    if (!get_property(path, interface, property, &reply, &variant)) {
        return false;
    }

    if (dbus_message_iter_get_arg_type(&variant) != DBUS_TYPE_UINT32) {
        dbus_message_unref(reply);
        return false;
    }

    dbus_uint32_t value;
    dbus_message_iter_get_basic(&variant, &value);

    dbus_message_unref(reply);

    *out_value = value;
    return true;
}

static bool get_object_path_property(const char* path, const char* interface, const char* property, char** value) {
    DBusMessage* reply;
    DBusMessageIter variant;
    if (!get_property(path, interface, property, &reply, &variant)) {
        return false;
    }

    if (dbus_message_iter_get_arg_type(&variant) != DBUS_TYPE_OBJECT_PATH) {
        dbus_message_unref(reply);
        return false;
    }

    const char* object_path;
    dbus_message_iter_get_basic(&variant, &object_path);

    *value = strdup(object_path);

    dbus_message_unref(reply);

    return true;
}

static bool get_byte_array_property(const char* path, const char* interface, const char* property, uint8_t** value_arr, size_t* value_size) {
   DBusMessage* reply;
    DBusMessageIter variant;
    if (!get_property(path, interface, property, &reply, &variant)) {
        return false;
    }

    DBusMessageIter array;
    dbus_message_iter_recurse(&variant, &array);

    if (dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_BYTE) {
        dbus_message_unref(reply);
        return false;
    }

    unsigned char* data;
    int length;

    dbus_message_iter_get_fixed_array(&array, &data, &length);

    size_t size = length;
    uint8_t* arr = malloc(sizeof(uint8_t) * size);

    if (!arr) {
        dbus_message_unref(reply);
        log_error("Out of memory");
        return false;
    }

    memcpy(arr, data, size);

    *value_arr = arr;
    *value_size = size;

    dbus_message_unref(reply);

    return true;
}

static bool get_wifi_device_path(char** out_path) {
    DBusMessage* reply;
    DBusMessageIter variant;
    if (!get_property(NETWORKMANAGER_PATH, NETWORKMANAGER_INTERFACE, "Devices", &reply, &variant)) {
        return false;
    }

    DBusMessageIter devices;
    dbus_message_iter_recurse(&variant, &devices);

    while (dbus_message_iter_get_arg_type(&devices) != DBUS_TYPE_INVALID) {
        const char* path;
        dbus_message_iter_get_basic(&devices, &path);

        uint32_t device_type;
        if (!get_uint32_property(path, NETWORKMANAGER_INTERFACE ".Device", "DeviceType", &device_type)) {
            dbus_message_iter_next(&devices);
            continue;
        }

        if (device_type == NM_DEVICE_TYPE_WIFI) {
            *out_path = strdup(path);
            dbus_message_unref(reply);
            return true;
        }

        dbus_message_iter_next(&devices);
    }

    dbus_message_unref(reply);

    return false;
}

static bool append_empty_sv_dict(DBusMessage* message) {
    DBusMessageIter iter;
    DBusMessageIter dict;

    dbus_message_iter_init_append(message, &iter);

    if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict)) {
        log_error("Out of memory");
        return false;
    }

    if (!dbus_message_iter_close_container(&iter, &dict)) {
        log_error("Out of memory");
        return false;
    }

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

bool nm_wifi_set_enabled(bool _enabled) {
    DBusMessage* message = dbus_message_new_method_call(NETWORKMANAGER_INTERFACE, NETWORKMANAGER_PATH, NETWORKMANAGER_INTERFACE, "Enable");
    if (!message) {
        log_error("Out of memory");
        return false;
    }

    dbus_bool_t enabled = _enabled;
    dbus_message_append_args(message,
        DBUS_TYPE_BOOLEAN, &enabled,
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

    dbus_message_unref(reply);
    return true;
}

bool nm_scan(Network** out_networks, size_t* count) {
    char* wifi_device_path;
    if (!get_wifi_device_path(&wifi_device_path)) {
        log_error("Failed to get wifi device");
        return false;
    }

    DBusError error;
    dbus_error_init(&error);

    char match[1024];
    snprintf(match, sizeof(match),
    "type='signal',"
        "interface='org.freedesktop.DBus.Properties',"
        "member='PropertiesChanged',"
        "path='%s',"
        "arg0='" NETWORKMANAGER_INTERFACE ".Device.Wireless'",
        wifi_device_path
    );

    dbus_bus_add_match(connection, match, &error);

    if (dbus_error_is_set(&error)) {
        log_error("Failed to add match: %s", error.message);
        dbus_error_free(&error);
        free(wifi_device_path);
        return false;
    }

    DBusMessage* message = dbus_message_new_method_call(NETWORKMANAGER_INTERFACE, wifi_device_path, NETWORKMANAGER_INTERFACE ".Device.Wireless", "RequestScan");
    if (!message) {
        log_error("Out of memory");
        free(wifi_device_path);
        return false;
    }

    if (!append_empty_sv_dict(message)) {
        free(wifi_device_path);
        return false;
    }

    dbus_error_init(&error);

    DBusMessage* reply = dbus_connection_send_with_reply_and_block(connection, message, -1, &error);

    dbus_message_unref(message);

    if (dbus_error_is_set(&error)) {
        log_error("D-Bus error: %s", error.message);
        dbus_error_free(&error);
        free(wifi_device_path);
        return false;
    }

    dbus_message_unref(reply);

    bool property_changed = false;
    while (!property_changed) {
        if (!dbus_connection_read_write(connection, -1)) break;

        DBusMessage* message;
        while ((message = dbus_connection_pop_message(connection)) != NULL) {
            if (dbus_message_is_signal(message, "org.freedesktop.DBus.Properties", "PropertiesChanged")) {
                DBusMessageIter iter;
                if (dbus_message_iter_init(message, &iter)) {
                    const char* interface_name;

                    dbus_message_iter_get_basic(&iter, &interface_name);

                    if (strcmp(interface_name, NETWORKMANAGER_INTERFACE ".Device.Wireless") == 0) {
                        dbus_message_iter_next(&iter);

                        DBusMessageIter properties;
                        dbus_message_iter_recurse(&iter, &properties);

                        while (dbus_message_iter_get_arg_type(&properties) != DBUS_TYPE_INVALID) {
                            DBusMessageIter entry;
                            dbus_message_iter_recurse(&properties, &entry);

                            const char* property_name;
                            dbus_message_iter_get_basic(&entry, &property_name);

                            if (strcmp(property_name, "LastScan") == 0) {
                                property_changed = true;
                                break;
                            }

                            dbus_message_iter_next(&properties);
                        }
                    }
                }
            }

            dbus_message_unref(message);

            if (property_changed) break;
        }
    }

    dbus_error_init(&error);

    dbus_bus_remove_match(connection, match, &error);

    if (dbus_error_is_set(&error)) {
        log_error("Failed to remove match: %s", error.message);
        dbus_error_free(&error);
        return false;
    }

    char* active_access_point;
    if (!get_object_path_property(wifi_device_path, NETWORKMANAGER_INTERFACE ".Device.Wireless", "ActiveAccessPoint", &active_access_point)) {
        log_error("Failed to get active access point");
        return false;
    }

    DBusMessageIter variant;
    if (!get_property(wifi_device_path, NETWORKMANAGER_INTERFACE ".Device.Wireless", "AccessPoints", &reply, &variant)) {
        log_error("Failed to get wifi access points");
        free(wifi_device_path);
        return false;
    }

    DBusMessageIter access_points;
    dbus_message_iter_recurse(&variant, &access_points);

    int access_point_count = 0;
    while (dbus_message_iter_get_arg_type(&access_points) != DBUS_TYPE_INVALID) {
        if (dbus_message_iter_get_arg_type(&access_points) != DBUS_TYPE_OBJECT_PATH) {
            // malformed/unexpected response
            break;
        }

        const char* path;
        dbus_message_iter_get_basic(&access_points, &path);

        access_point_count++;
        dbus_message_iter_next(&access_points);
    }

    dbus_message_iter_recurse(&variant, &access_points);

    Network* networks = calloc(access_point_count, sizeof(Network));
    if (!networks) {
        log_error("Out of memory");
        dbus_message_unref(reply);
        free(wifi_device_path);
        return false;
    }

    int i = 0;
    while (dbus_message_iter_get_arg_type(&access_points) != DBUS_TYPE_INVALID) {
        const char* path;
        dbus_message_iter_get_basic(&access_points, &path);

        uint8_t* array = NULL;
        size_t size;
        if (!get_byte_array_property(path, NETWORKMANAGER_INTERFACE ".AccessPoint", "Ssid", &array, &size)) {
            goto fail;
        }

        Network* network = &networks[i];
        network->ssid = malloc(size + 1);
        if (!network->ssid) {
            log_error("Out of memory");
            goto fail;
        }

        memcpy(network->ssid, array, size);
        network->ssid[size] = '\0';

        free(array);

        uint8_t strength;
        if (!get_byte_property(path, NETWORKMANAGER_INTERFACE ".AccessPoint", "Strength", &strength)) {
            goto fail;
        }

        network->signal = strength;

        uint32_t flags;
        if (!get_uint32_property(path, NETWORKMANAGER_INTERFACE ".AccessPoint", "Flags", &flags)) {
            goto fail;
        }

        network->secured = flags & NM_802_11_AP_FLAGS_PRIVACY;

        network->connected = strcmp(path, active_access_point) == 0;

        dbus_message_iter_next(&access_points);
        i++;
        continue;

        fail:
        if (array) free(array);
        for (int j = 0; j < i; j++) {
            free(networks[j].ssid);
        }
        free(networks);
        free(active_access_point);
        dbus_message_unref(reply);
        free(wifi_device_path);
        return false;
    }

    dbus_message_unref(reply);
    free(active_access_point);
    free(wifi_device_path);

    *out_networks = networks;
    *count = access_point_count;

    return true;
}

void nm_free_networks(Network* networks, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(networks[i].ssid);
    }
    free(networks);
}

bool nm_connect(const char* ssid, const char* password) {
    char* wifi_device_path;
    if (!get_wifi_device_path(&wifi_device_path)) {
        log_error("Failed to get wifi device");
        return false;
    }

    DBusMessage* message = dbus_message_new_method_call(NETWORKMANAGER_INTERFACE, NETWORKMANAGER_PATH, NETWORKMANAGER_INTERFACE, "AddAndActivateConnection");
    if (!message) {
        log_error("Out of memory");
        free(wifi_device_path);
        return false;
    }

    DBusMessageIter iter;
    dbus_message_iter_init_append(message, &iter);

    DBusMessageIter settings;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sa{sv}}", &settings);

    DBusMessageIter entry;
    dbus_message_iter_open_container(&settings, DBUS_TYPE_DICT_ENTRY, NULL, &entry);

    const char* key = "connection";
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

    DBusMessageIter dict;
    dbus_message_iter_open_container(&entry, DBUS_TYPE_ARRAY, "{sv}", &dict);

    DBusMessageIter property;
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &property);

    const char* property_name = "id";
    dbus_message_iter_append_basic(&property, DBUS_TYPE_STRING, &property_name);

    DBusMessageIter variant;
    dbus_message_iter_open_container(&property, DBUS_TYPE_VARIANT, "s", &variant);

    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &ssid);

    dbus_message_iter_close_container(&property, &variant);
    dbus_message_iter_close_container(&dict, &property);


    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &property);

    property_name = "type";
    dbus_message_iter_append_basic(&property, DBUS_TYPE_STRING, &property_name);

    dbus_message_iter_open_container(&property, DBUS_TYPE_VARIANT, "s", &variant);

    const char* type = "802-11-wireless";
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &type);

    dbus_message_iter_close_container(&property, &variant);
    dbus_message_iter_close_container(&dict, &property);

    dbus_message_iter_close_container(&entry, &dict);
    dbus_message_iter_close_container(&settings, &entry);

    dbus_message_iter_open_container(&settings, DBUS_TYPE_DICT_ENTRY, NULL, &entry);


    key = "802-11-wireless";
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

    dbus_message_iter_open_container(&entry, DBUS_TYPE_ARRAY, "{sv}", &dict);

    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &property);

    property_name = "ssid";
    dbus_message_iter_append_basic(&property, DBUS_TYPE_STRING, &property_name);

    dbus_message_iter_open_container(&property, DBUS_TYPE_VARIANT, "ay", &variant);

    DBusMessageIter ssid_array;
    dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "y", &ssid_array);

    const unsigned char* ssid_bytes = (const unsigned char*) ssid;
    dbus_message_iter_append_fixed_array(&ssid_array, DBUS_TYPE_BYTE, &ssid_bytes, strlen(ssid));

    dbus_message_iter_close_container(&variant, &ssid_array);
    dbus_message_iter_close_container(&property, &variant);
    dbus_message_iter_close_container(&dict, &property);

    dbus_message_iter_close_container(&entry, &dict);
    dbus_message_iter_close_container(&settings, &entry);

    dbus_message_iter_open_container(&settings, DBUS_TYPE_DICT_ENTRY, NULL, &entry);


    key = "802-11-wireless-security";
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);

    dbus_message_iter_open_container(&entry, DBUS_TYPE_ARRAY, "{sv}", &dict);

    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &property);

    property_name = "key-mgmt";
    dbus_message_iter_append_basic(&property, DBUS_TYPE_STRING, &property_name);

    dbus_message_iter_open_container(&property, DBUS_TYPE_VARIANT, "s", &variant);

    const char* property_value = "wpa-psk";
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &property_value);

    dbus_message_iter_close_container(&property, &variant);
    dbus_message_iter_close_container(&dict, &property);

    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &property);

    property_name = "psk";
    dbus_message_iter_append_basic(&property, DBUS_TYPE_STRING, &property_name);

    dbus_message_iter_open_container(&property, DBUS_TYPE_VARIANT, "s", &variant);

    property_value = password;
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &property_value);

    dbus_message_iter_close_container(&property, &variant);
    dbus_message_iter_close_container(&dict, &property);

    dbus_message_iter_close_container(&entry, &dict);
    dbus_message_iter_close_container(&settings, &entry);

    dbus_message_iter_close_container(&iter, &settings);

    dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &wifi_device_path);
    const char* slash = "/";
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &slash);

    DBusError error;
    dbus_error_init(&error);

    DBusMessage* reply = dbus_connection_send_with_reply_and_block(connection, message, -1, &error);

    dbus_message_unref(message);

    if (dbus_error_is_set(&error)) {
        log_error("D-Bus error: %s", error.message);
        dbus_error_free(&error);
        free(wifi_device_path);
        return false;
    }

    dbus_message_unref(reply);
    free(wifi_device_path);
    return true;
}

bool nm_disconnect(void) {
    char* wifi_device_path;
    if (!get_wifi_device_path(&wifi_device_path)) {
        log_error("Failed to get wifi device");
        return false;
    }

    DBusMessage* message = dbus_message_new_method_call(NETWORKMANAGER_INTERFACE, NETWORKMANAGER_PATH, NETWORKMANAGER_INTERFACE ".Device", "Disconnect");
    if (!message) {
        log_error("Out of memory");
        free(wifi_device_path);
        return false;
    }

    DBusError error;
    dbus_error_init(&error);

    DBusMessage* reply = dbus_connection_send_with_reply_and_block(connection, message, -1, &error);

    dbus_message_unref(message);

    if (dbus_error_is_set(&error)) {
        log_error("D-Bus error: %s", error.message);
        dbus_error_free(&error);
        free(wifi_device_path);
        return false;
    }

    dbus_message_unref(reply);
    free(wifi_device_path);
    return true;
}

bool nm_forget(const char* ssid) {
    DBusMessage* message = dbus_message_new_method_call(NETWORKMANAGER_INTERFACE, NETWORKMANAGER_PATH "/Settings", NETWORKMANAGER_INTERFACE ".Settings", "ListConnections");
    if (!message) {
        log_error("Out of memory");
        return false;
    }

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

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
        dbus_message_unref(reply);
        return false;
    }

    DBusMessageIter connections;
    dbus_message_iter_recurse(&iter, &connections);

    while (dbus_message_iter_get_arg_type(&connections) != DBUS_TYPE_INVALID) {
        const char* path;
        dbus_message_iter_get_basic(&connections, &path);

        DBusMessage* message = dbus_message_new_method_call(NETWORKMANAGER_INTERFACE, path, NETWORKMANAGER_INTERFACE ".Settings.Connection", "GetSettings");
        if (!message) {
            log_error("Out of memory");
            dbus_message_unref(reply);
            return false;
        }

        dbus_error_init(&error);

        DBusMessage* settings_reply = dbus_connection_send_with_reply_and_block(connection, message, -1, &error);

        dbus_message_unref(message);

        if (dbus_error_is_set(&error)) {
            log_error("D-Bus error: %s", error.message);
            dbus_error_free(&error);
            dbus_message_unref(reply);
            return false;
        }

        DBusMessageIter iter;
        dbus_message_iter_init(settings_reply, &iter);

        if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
            dbus_message_unref(settings_reply);
            dbus_message_unref(reply);
            return false;
        }

        DBusMessageIter settings;
        dbus_message_iter_recurse(&iter, &settings);

        while (dbus_message_iter_get_arg_type(&settings) != DBUS_TYPE_INVALID) {
            DBusMessageIter entry;
            dbus_message_iter_recurse(&settings, &entry);

            const char* section;
            dbus_message_iter_get_basic(&entry, &section);

            dbus_message_iter_next(&entry);

            DBusMessageIter properties;
            dbus_message_iter_recurse(&entry, &properties);

            if (strcmp(section, "802-11-wireless") == 0) {
                while (dbus_message_iter_get_arg_type(&properties) != DBUS_TYPE_INVALID) {
                    DBusMessageIter property;
                    dbus_message_iter_recurse(&properties, &property);

                    const char* property_name;
                    dbus_message_iter_get_basic(&property, &property_name);

                    dbus_message_iter_next(&property);

                    if (strcmp(property_name, "ssid") == 0) {
                        DBusMessageIter variant;
                        dbus_message_iter_recurse(&property, &variant);

                        if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_ARRAY && dbus_message_iter_get_element_type(&variant) == DBUS_TYPE_BYTE) {
                            DBusMessageIter bytes;
                            dbus_message_iter_recurse(&variant, &bytes);

                            unsigned char* data;
                            int length;
                            dbus_message_iter_get_fixed_array(&bytes, &data, &length);

                            size_t ssid_length = strlen(ssid);

                            if (length == ssid_length && memcmp(ssid, data, ssid_length) == 0) {
                                DBusMessage* message = dbus_message_new_method_call(NETWORKMANAGER_INTERFACE, path, NETWORKMANAGER_INTERFACE ".Settings.Connection", "Delete");
                                if (!message) {
                                    log_error("Out of memory");
                                    dbus_message_unref(settings_reply);
                                    dbus_message_unref(reply);
                                    return false;
                                }

                                dbus_error_init(&error);

                                DBusMessage* delete_reply = dbus_connection_send_with_reply_and_block(connection, message, -1, &error);

                                dbus_message_unref(message);

                                if (dbus_error_is_set(&error)) {
                                    log_error("D-Bus error: %s", error.message);
                                    dbus_error_free(&error);
                                    dbus_message_unref(settings_reply);
                                    dbus_message_unref(reply);
                                    return false;
                                }

                                dbus_message_unref(delete_reply);
                            }
                        }
                    }

                    dbus_message_iter_next(&properties);
                }
            }

            dbus_message_iter_next(&settings);
        }

        dbus_message_unref(settings_reply);

        dbus_message_iter_next(&connections);
    }

    dbus_message_unref(reply);
    return true;
}
