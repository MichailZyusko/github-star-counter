// Copy this file to `secrets.h` (next to the sketch) and fill in your values.
// `secrets.h` is in .gitignore and will never be committed.
#pragma once

// Your 2.4 GHz Wi-Fi network (ESP8266 does not support 5 GHz).
#define WIFI_SSID    "your-wifi-name"
#define WIFI_PASS    "your-wifi-password"

// Optional. Without a token GitHub allows 60 requests/hour per IP, which is enough.
// A fine-grained personal access token with NO permissions (public repos only)
// raises the limit to 5000/hour. Create one at https://github.com/settings/personal-access-tokens
#define GITHUB_TOKEN ""
