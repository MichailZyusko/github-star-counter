# GitHub Star Counter

A tiny desk gadget that shows the live GitHub star count of your repositories on an 8x40 LED matrix. It cycles through a list of projects, draws a pixel-art logo for each one and animates the digits when the number changes.

- **Board:** NodeMCU v3 (ESP8266, ESP-12E)
- **Display:** 5 x MAX7219 8x8 matrices (FC16 modules chained into 8x40)
- **Data:** GitHub REST API (`stargazers_count`), polled every 3 minutes
- **Case:** 3D-printable, files in [`3d/`](3d)

## Hardware

| MAX7219 | NodeMCU |
|---------|---------|
| VCC     | 3V3 / VIN (5 V recommended for brightness) |
| GND     | GND     |
| DIN     | D7 (MOSI) |
| CLK     | D5 (SCK)  |
| CS      | D8      |

## Software setup

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) and add ESP8266 board support (`http://arduino.esp8266.com/stable/package_esp8266com_index.json` in *Preferences → Additional boards manager URLs*).
2. Install the **MD_MAX72XX** library (Library Manager).
3. Create your secrets file:
   ```bash
   cp secrets.example.h secrets.h
   ```
   then edit `secrets.h`:

   | Define         | Required | What to put there |
   |----------------|----------|-------------------|
   | `WIFI_SSID`    | yes | Name of your 2.4 GHz Wi-Fi network |
   | `WIFI_PASS`    | yes | Wi-Fi password |
   | `GITHUB_TOKEN` | no  | Fine-grained GitHub token with no permissions (public repos). Leave `""` to use the anonymous 60 req/hour limit. |

4. Choose your repositories in `github-star-counter.ino`:
   ```cpp
   Project projects[] = {
     {"owner", "repo", LOGO_KEYBOARD, 2489},   // last number = fallback if GitHub is unreachable
   };
   ```
5. Select board **NodeMCU 1.0 (ESP-12E Module)**, upload.

### Settings

In the sketch: `DEMO_MODE` (1 = fast switching with fake stars, handy for testing), `BRIGHTNESS` (0..15), `PROJECT_DURATION_MS`, `FETCH_INTERVAL_MS`. If the image is upside down or mirrored, flip `FLIP_VERTICAL` / `MIRROR`.

## 3D printing

Files in [`3d/`](3d):

| File | Purpose |
|------|---------|
| `GitHub Star Counter Case.stl` / `.3mf` | Main case |
| `GitHub Star Counter Case with supports.stl` | Case with supports baked in |
| `GitHub Star Counter Cap.stl` / `.3mf` | Front cap |

Print in PLA/PETG, 0.2 mm layers. Use a translucent or smoked cap for a nicer LED diffusion.

## License

MIT
