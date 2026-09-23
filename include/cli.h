// Copyright 2026 Jannik Laugmand Bülow

// Subcommand entry points

#ifndef WIFI_CLI_H
#define WIFI_CLI_H

void wifi_on(void);
void wifi_off(void);

void wifi_status(void);

void wifi_scan(void);

void wifi_connect(const char* ssid);
void wifi_disconnect(void);

void wifi_forget(const char* ssid);

#endif //WIFI_CLI_H
