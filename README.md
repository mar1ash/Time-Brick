# 🧱 Time Brick

A minimal, aesthetic bedside clock built with an ESP-01 and OLED display.  
Time Brick shows the current time, date, weather, and a rotating set of Stanley Parable-inspired quotes — all in a tiny translucent micro bricks enclosure.

![Time Brick](https://mar1ash.pp.ua/assets/images/time-brick01.jpg)

---

## ✨ Features

- **Current time** in 24-hour format
- **Date display** (DD.MM.YYYY)
- **Weather info** (today only)
- **Rotating quotes** inspired by *The Stanley Parable*  
- **Animated icons** for weather (e.g. sun, rain, clouds with lightning)
- **Night mode** (23:00–07:00): display locks to clock only
- **Hourly hydration reminder**
- **Compact build** in a LEGO-style translucent brick case

---

## 📷 Gallery

Check out the full build and photos on the [project page](https://mar1ash.pp.ua/diy-projects/time-brick/).

---

## 🛠️ Tech Stack

- **ESP-01 (ESP8266)**
- **0.96" I2C OLED Display** (128x64, used as 128x32)
- **Weather API** via WiFi (OpenWeatherMap)
- **NTP Time Sync** via WiFi
- **Custom Arduino sketch**
- **5V USB or 3.3V regulated power**

---

## 📦 Files

- `Time-Brick.ino`: main Arduino sketch
- `README.md`: this file

---

## ⚙️ Setup

1. Edit `Time-Brick.ino` with your WiFi + OpenWeatherMap API key
2. Flash the ESP-01 using FT232RL or similar adapter
3. Connect OLED display to ESP-01 (via I2C)
4. Power the device (USB or 3.3V regulated)
5. Enjoy your existential bedside clock ✨

---

## 📄 License

MIT License — free to use, remix, and share.  
Just don’t tell the narrator.

---

## 🔗 More

Website: [mar1ash.pp.ua](https://mar1ash.pp.ua)  
Project page: [Time Brick](https://mar1ash.pp.ua/diy-projects/time-brick/)

