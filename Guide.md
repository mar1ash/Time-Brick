# ESP8266 OLED Weather Clock Setup Guide

This guide will walk you through the steps to set up and run the **ESP8266 OLED Weather Clock** project.

---

## 🕹️ Project Overview

The ESP8266 OLED Weather Clock displays:

- Current **time**, **date**
- Live **weather** data (via OpenWeatherMap API)
- A static **quote**
- Occasional random *Stanley Parable*-style quotes  
- Hourly **"Drink Water"** reminders  
- Automatic **night mode** to dim the display

It uses a 128x32 SSD1306 OLED display and connects to your WiFi.

---

## 🧰 Hardware Requirements

- **ESP8266 Development Board**  
  *(e.g., NodeMCU, Wemos D1 Mini, ESP-01 + adapter)*

- **SSD1306 128x32 I2C OLED Display**  
  *(Make sure it's I2C and 128x32)*

- **Micro USB Cable** *(for power & programming)*

- **5V Power Supply** *(1A+ recommended)*

- **Jumper Wires**

---

## 💻 Software Requirements

- **Arduino IDE**  
  [Download here](https://www.arduino.cc/en/software)

- **ESP8266 Board Package**

- **Libraries:**
  - `Wire` *(built-in)*
  - `ESP8266WiFi` *(built-in)*
  - `WiFiUdp` *(built-in)*
  - `ESP8266HTTPClient` *(built-in)*
  - [`NTPClient`](https://github.com/arduino-libraries/NTPClient)
  - [`Adafruit_GFX`](https://github.com/adafruit/Adafruit-GFX-Library)
  - [`Adafruit_SSD1306`](https://github.com/adafruit/Adafruit_SSD1306)
  - [`ArduinoJson`](https://github.com/bblanchon/ArduinoJson) *(v6+ recommended)*
  - [`TimeLib`](https://github.com/PaulStoffregen/Time)

---

## ⚙️ Setup Instructions

### 1. Install Arduino IDE

Download and install from [arduino.cc](https://www.arduino.cc/en/software)

---

### 2. Add ESP8266 Board Support

- Go to `File > Preferences`
- Add this URL to **"Additional Boards Manager URLs"**:  
  ```
  http://arduino.esp8266.com/stable/package_esp8266com_index.json
  ```
- Go to `Tools > Board > Boards Manager`
- Search for `esp8266` → Click **Install**

---

### 3. Install Required Libraries

In Arduino IDE:

- Go to `Sketch > Include Library > Manage Libraries`
- Install the following:
  - **NTPClient**
  - **Adafruit GFX Library**
  - **Adafruit SSD1306**
  - **ArduinoJson** *(v6+)*
  - **TimeLib**

---

### 4. Get OpenWeatherMap API Key

- Sign up at [openweathermap.org](https://openweathermap.org/)
- Go to your account → **API Keys**
- Copy your key (it may take time to activate)

---

### 5. Wire the Components

**Wiring (Wemos D1 Mini / NodeMCU):**

| OLED Pin | ESP8266 Pin |
|----------|-------------|
| VCC      | 3.3V or 5V  |
| GND      | GND         |
| SDA      | D4 (GPIO2)  |
| SCL      | D3 (GPIO0)  |

> Code uses `Wire.begin(2, 0);` → SDA = GPIO2, SCL = GPIO0

---

### 6. Configure the Code

Replace in the code:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const String API_KEY = "YOUR_OPENWEATHERMAP_API_KEY";
const String CITY = "Kyiv";
const String COUNTRY = "UA";
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800, 1800000); // UTC+3
```

- Replace `ssid`, `password`, and `API_KEY`
- Adjust `CITY`, `COUNTRY` and timezone offset (`10800` = UTC+3)

---

### 7. Upload the Code

- Select your board: `Tools > Board > NodeMCU 1.0` (or similar)
- Select port: `Tools > Port`
- Click `Upload`

---

### 8. Monitor Serial Output (Optional)

- Open `Tools > Serial Monitor`
- Set **baud rate** to `115200`
- Useful for debugging WiFi and weather errors

---

### 9. Observe the Display

- Splash screen → WiFi connection → Time & weather data
- Cycles through:
  - ⏰ Time  
  - 📅 Date  
  - 🌤️ Weather  
  - 💬 Quote  
- Hourly 💧 **"Drink Water"** reminder

---

## 🛠️ Troubleshooting

### ❌ `SSD1306 allocation failed`
- Check wiring, especially **SDA/SCL**
- Try changing address from `0x3C` to `0x3D`

### ❌ WiFi not connecting
- Double-check SSID/password
- Use **2.4GHz WiFi** only
- Avoid captive portals

### ❌ Time Sync Failed
- Check WiFi & NTP offset
- Make sure UDP port 123 is not blocked

### ❌ Weather shows `--` or HTTP errors
- Check API key & city/country
- Ensure active internet
- Wait if key is newly created

### ❌ Blank screen
- Re-check OLED wiring
- Ensure correct voltage
- Try power cycle

### ❌ No random quotes or drink reminder
- Ensure synced time
- Quotes show randomly (1/4 chance, every 30 min)

---

## 🧪 Customization & Improvements

- **Change screen durations**: edit `regularScreenDurations[]`
- **Add/remove screens**: add new `draw...()` functions
- **Edit quotes**: modify `stanleyQuotes[]` array
- **Change night mode**: update `nightStartHour` / `nightEndHour`
- **Add buttons**: to cycle views or refresh weather
- **Battery operation**: use a LiPo + charging circuit
- **Web config portal**: use [WiFiManager](https://github.com/tzapu/WiFiManager) for easier setup

---

## 📸 Example Photos (Optional Section)

*(You can include images like: assembled device, breadboard setup, close-up of ESP-01, glowing night view, etc.)*

---

## 🧭 Final Notes

This guide should provide everything you need to bring your **ESP8266 OLED Weather Clock** to life!  
Feel free to fork, improve, and customize it to match your style and needs.  
Happy tinkering! 🔧✨
