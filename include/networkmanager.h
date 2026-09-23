// Copyright 2026 Jannik Laugmand Bülow

#ifndef WIFI_NETWORKMANAGER_H
#define WIFI_NETWORKMANAGER_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char* ssid;
    int signal;
    bool secured;
    bool connected;
} Network;

bool nm_init(void);
void nm_shutdown(void);

bool nm_wifi_enabled(void);
bool nm_wifi_set_enabled(bool enabled);

bool nm_scan(Network** networks, size_t* count);
void nm_free_networks(Network* networks, size_t count);

bool nm_connect(const char* ssid, const char* password);
bool nm_disconnect(void);

bool nm_forget(const char* ssid);

#endif //WIFI_NETWORKMANAGER_H
