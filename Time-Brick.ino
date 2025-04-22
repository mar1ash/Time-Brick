/*
  ESP8266 OLED Weather Clock

  This project displays time, date, weather information, and random quotes
  on a 128x32 SSD1306 OLED display using an ESP8266 microcontroller.
  It fetches time from an NTP server and weather data from the OpenWeatherMap API.
  Includes features like automatic WiFi reconnection, hourly water reminders,
  random Stanley Parable-style quotes, and a night mode.

  Author: mar1ash (Based on original code)
  Date: April 2025
  License: MIT License

  Hardware Requirements:
  - ESP8266 Development Board (e.g., NodeMCU, Wemos D1 Mini)
  - SSD1306 128x32 I2C OLED Display
  - Micro USB Cable for power and programming
  - Power supply (5V, 1A or more recommended)

  Software Requirements:
  - Arduino IDE
  - ESP8266 Board Support Package for Arduino IDE
  - Required Libraries (listed below)

  Connections (Common for Wemos D1 Mini / NodeMCU):
  - OLED VCC -> ESP8266 3.3V or 5V (check your OLED module's voltage tolerance)
  - OLED GND -> ESP8266 GND
  - OLED SDA -> ESP8266 GPIO2 (D4)
  - OLED SCL -> ESP8266 GPIO0 (D3)

  Note: Ensure you replace the placeholder values for WiFi credentials,
  OpenWeatherMap API Key, City, and Country below before uploading.
*/

// --- Include Required Libraries ---
#include <Wire.h>              // Required for I2C communication (used by OLED)
#include <ESP8266WiFi.h>       // Core library for ESP8266 WiFi functions
#include <WiFiUdp.h>           // Required for NTPClient
#include <NTPClient.h>         // Library for network time synchronization
#include <Adafruit_GFX.h>      // Core graphics library for Adafruit displays
#include <Adafruit_SSD1306.h>  // Library for SSD1306 OLED displays
#include <ESP8266HTTPClient.h> // Library for making HTTP requests (for weather API)
#include <ArduinoJson.h>       // Library for parsing JSON data (from weather API)
#include <math.h>              // Include for math functions like sin, cos, fmod (used in icon animations)
#include <TimeLib.h>           // Include TimeLib for date/time functions (used by hour(), etc.)

// --- Display Settings ---
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET -1  // Reset pin # (or -1 if sharing Arduino reset pin). Use -1 for ESP8266.
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The address 0x3C is common for 128x32 displays.
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- WiFi Credentials ---
// !! IMPORTANT: Replace with your actual WiFi network name and password !!
const char* ssid = "YOUR_WIFI_SSID";      // Replace with your WiFi SSID
const char* password = "YOUR_WIFI_PASSWORD"; // Replace with your WiFi password

// --- Time Client Settings ---
WiFiUDP ntpUDP; // UDP client for NTP
// Initialize NTPClient.
// Arguments: UDP client, NTP server address, timezone offset in seconds, update interval in ms
// Kyiv is UTC+3 (10800 seconds). Update every 30 minutes (30 * 60 * 1000 = 1800000 ms).
// NTP servers typically handle Daylight Saving Time adjustments automatically.
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800, 1800000);

// --- Weather API Settings ---
// !! IMPORTANT: Replace with your actual OpenWeatherMap API Key, City, and Country !!
const String API_KEY = "YOUR_OPENWEATHERMAP_API_KEY"; // Your OpenWeatherMap API Key
const String CITY = "City"; // Your city
const String COUNTRY = "Country"; // Your country code (e.g., "US", "GB", "DE", "UA")
#define API_HOST "api.openweathermap.org" // OpenWeatherMap API host address
String temperature = "--"; // Variable to store temperature string (initialized to placeholder)
String weatherDesc = "Loading..."; // Variable to store weather description
String iconCode = "01d"; // Variable to store weather icon code (default to clear day, used before first fetch)
unsigned long lastWeatherUpdate = 0; // Timestamp of the last weather update (ms)
// Weather update interval: 60 minutes (60 * 60 * 1000 = 3600000 milliseconds)
const unsigned long weatherInterval = 3600000;

// --- Screen Switching Logic ---
// Define the order and duration of screens in the regular cycle.
// Screen indices: 0=Time, 1=Date, 2=Weather, 3=Quote
const unsigned long regularScreenDurations[] = {15000, 5000, 10000, 15000}; // Durations in milliseconds
const int numRegularScreens = 4; // Number of screens in the regular cycle
int screenIndex = 0; // Current screen index within the regular cycle (0 to numRegularScreens-1)

unsigned long lastSwitch = 0; // Timestamp when the *current* screen (regular or special) became active (ms)

// Flags and Timers for Special Screens (interrupting the regular cycle)
bool showStanleyQuote = false; // Flag to indicate if the Stanley Quote screen should be shown
unsigned long stanleyQuoteStartTime = 0; // Timestamp when the Stanley Quote screen became active (ms)
unsigned long lastStanleyQuoteTime = 0; // Timestamp of the *last* time a Stanley quote was shown (ms)
const unsigned long stanleyQuoteMinInterval = 1800000; // Minimum interval between Stanley quotes (30 minutes in ms)
const unsigned long stanleyQuoteDuration = 10000; // Duration for the Stanley Quote screen (10 seconds in ms)
String currentStanleyQuote = ""; // Variable to hold the quote currently being displayed

bool showWaterReminder = false; // Flag to indicate if the Drink Water reminder should be shown
unsigned long waterReminderStartTime = 0; // Timestamp when the Drink Water screen became active (ms)
const unsigned long drinkWaterDuration = 15000; // Duration for the Drink Water screen (15 seconds in ms)
int lastReminderHour = -1; // To track the last hour the reminder was shown. -1 indicates not yet set/synced.


// --- Night Mode Settings ---
const int nightStartHour = 23; // 11 PM (23:00)
const int nightEndHour = 7;    // 7 AM (07:00)
bool nightMode = false; // Flag indicating if night mode is active

// --- WiFi Reconnection Timer ---
unsigned long lastWiFiCheck = 0; // Timestamp of the last WiFi status check (ms)
const unsigned long wifiCheckInterval = 60000; // Check WiFi status and attempt reconnect every 1 minute (ms)

// --- Stanley Parable style quotes ---
// Array of quotes to be displayed randomly. Use '\n' for new lines.
// Be mindful of memory usage with a very large number of long quotes on ESP8266.
const char* stanleyQuotes[] = {
  "This is a\nsentence.\nIt ends here.",
  "Waiting is\nimportant.\nOr not.",
  "The button was\npressed. Maybe.",
  "Data acquired.\nMeaning unknown.",
  "Where are we\ngoing? No idea.",
  "Naturally.",
  "The parable\ncontrols you.\nProbably.",
  "Not a choice.",
  "Simply happens.",
  "Fulfilling\npurpose. Supposedly.",
  "Is there a\npurpose? Unsure.",
  "Look left.",
  "Now look right.\nStill nothing.",
  "Nothing changed.",
  "Expected\noutcome?\nDebatable.",
  "The display\npersists without\nreason.",
  "As does the\ntime. Always.",
  "A moment passed.\nAnother begins.",
  "Feeling observed.\nYou should be.",
  "This is not a\ntest. Unless it\nis.",
  "The numbers shift.\nUncaringly.",
  "Is this the end?\nUnlikely.",
  "Inventory:\nDisplay, chip,\nno answers.",
  "Could be worse.\nCould be wet.",
  "Is anyone there?\n...Anyone?",
  "Error:\nMeaning not\nfound.",
  "Affirmative.",
  "Negative.",
  "Proceed to next\nstep. Hurry.",
  "Wait. Why?",
  "Perhaps there's\nanother way.",
  "Following the\nyellow line.",
  "There was a\ndoor here.",
  "This is highly\nirregular.",
  "Just one more\nscreen to go.",
  "And then?",
  "The story\ncontinues\nforever.",
  "Or does it?",
  "You are doing\ngreat! Probably.",
  "Did you press\nthe button?",
  "That wasn't it.",
  "Significance\nunknown.",
  "To the next\nscreen we go.",
  "The parable\ndictates.\nSometimes.",
  "Simply happens.",
  "Look left.\nAgain.",
  "Expected outcome.\nOr maybe not.",
  "Relentless.",
  "Another moment\nloops again.",
  "Feeling watched?\nIt watches too.",
  "Just keep\ndisplaying.",
  "For whom?",
  "Error:\nSelf-doubt\ndetected.",
  "Proceed to next\nstep. Please.",
  "Was that\nnecessary?\nHard to tell.",
  "Maybe it was.",
  "Or maybe not.",
  "Systems\nengaged.",
  "Stillness\ndetected.",
  "Proceeding\nanyway.",
  "Instructions\nunclear.\nShrug.",
  "Clarification\nunavailable.\nSorry.",
  "Trust the\nprocess.\nMaybe.",
  "Ignore the\nprocess.\nAlso valid.",
  "Wait for\ninput.\nKeep waiting.",
  "Input ignored.",
  "Silence speaks\nvolumes.",
  "Nothing happens.",
  "Something\nshifted.\nBarely.",
  "Coincidence?",
  "Probably not.",
  "Definitely maybe.",
  "Perception\nshifts slowly.",
  "Reality\nremains fixed.",
  "Just pixels.",
  "Just questions.",
  "A moment loops.",
  "Comfort in\nrepetition.",
  "Or discomfort?",
  "Frame updated.",
  "Meaning\nnot included.",
  "This is it.",
  "Or is it?",
  "No user\ndetected.",
  "Yet something\nwatches closely.",
  "This screen\nbreathes.\nProbably.",
  "The illusion\nholds steady.",
  "You are here.\nApparently.",
  "The story\nended. Maybe.",
  "What story?",
  "It's all\nscripted.",
  "Script lost.",
  "Improvising\nnow.",
  "Pretend it\nmatters.",
  "Pretend it\ndoesn't.",
  "No wrong\nanswer.",
  "No right one\neither.",
  "Crisis avoided.",
  "Or postponed?",
  "What a journey.\nNo motion.",
  "Identity\nunknown.",
  "Name not found.",
  "Purpose remains\nhidden.",
  "Reset imminent.",
  "Reset delayed.",
  "Now is now.",
  "Then was then.",
  "Does it align?",
  "Unlikely.",
  "Zero context.",
  "Maximum\nimplication.",
  "Choices make you.",
  "Or unmake you.",
  "This is normal.",
  "Define normal.",
  "Stand by.",
  "Still standing.",
  "Nobody noticed.",
  "Everyone noticed.",
  "Paradox\nconfirmed.",
  "Truth pending.",
  "Thank you for\nnothing.",
  "You're welcome."
};
// Calculate the number of quotes in the array
const int numStanleyQuotes = sizeof(stanleyQuotes) / sizeof(stanleyQuotes[0]);


// --- Forward Declarations ---
// Declare functions before they are called in setup() or loop()
void splashScreen(String text); // Displays a centered message on the OLED
void drawLoadingDots(int step); // Draws animated dots for loading indication
void checkAndReconnectWiFi(); // Checks WiFi status and attempts reconnection
void updateWeather(); // Fetches weather data from API
void drawTime(); // Draws the current time on the OLED
void drawDate(); // Draws the current date on the OLED
void drawQuote(); // Draws the original static quote
void drawRandomStanleyQuote(); // Draws a random Stanley Parable style quote
void drawDrinkWaterReminder(); // Draws the drink water reminder with animation
void drawWeather(); // Draws weather info (icon, temp, desc)
void drawWeatherIcon(String code); // Draws the animated weather icon based on code
void setDisplayContrast(uint8_t contrast); // Sets display brightness/contrast


// --- Setup Function: Runs once when the ESP8266 starts ---
void setup() {
  Serial.begin(115200); // Initialize serial communication for debugging
  Serial.println("\nStarting ESP8266 Weather Clock...");

  // Initialize I2C communication for the OLED display
  // SDA=GPIO2 (D4), SCL=GPIO0 (D3) are common pins on D1 Mini/NodeMCU
  Wire.begin(2, 0);

  // Initialize OLED display
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V logic
  // The address 0x3C is common for 128x32 displays
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    // If display initialization fails, halt execution
    for (;;)
      ; // Don't proceed, loop forever
  }

  // Clear display buffer and set basic text properties
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE); // Set text color to white
  display.setTextSize(1); // Set default text size (1 is the smallest)
  display.cp437(true); // Use full 256 char 'Code Page 437' font for special characters like degree symbol
  display.display(); // Display the buffer content (cleared screen)
  delay(100); // Short delay

  // --- Startup Splash Screens ---
  // Display initial messages using the splashScreen helper function
  splashScreen("You Are Being Timed"); // Centered by splashScreen
  delay(1000); // Keep splash screen visible for 1 second
  splashScreen("by Mar1ash"); // Centered by splashScreen
  delay(1000);

  // --- WiFi Connection ---
  splashScreen("Connecting to Wi-Fi"); // Display connection message
  WiFi.begin(ssid, password); // Start WiFi connection attempt

  int wifi_retries = 0;
  // Retry WiFi connection for up to ~30 seconds (60 retries * 500ms delay)
  while (WiFi.status() != WL_CONNECTED && wifi_retries < 60) {
    drawLoadingDots(wifi_retries % 4); // Animate dots at bottom during connection attempts
    delay(500); // Wait before retrying connection
    wifi_retries++;
  }

  // --- Initial Setup After Potential WiFi Connection ---
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // --- Initial Weather Fetch ---
    splashScreen("Getting weather"); // Display message
    // Note: updateWeather() is currently blocking, no dots animated during fetch.
    updateWeather(); // Fetch weather data immediately after connecting

    // --- Initial Time Synchronization ---
    splashScreen("Getting time"); // Display message

    // Wait until time is initially synchronized with a timeout
    unsigned long startAttemptTime = millis();
    const unsigned long timeSyncTimeout = 30000; // 30 seconds timeout for initial sync
    int sync_step = 0;

    // Attempt time update until successful or timeout
    while (!timeClient.update()) {
      // Check if timeout has occurred
      if (millis() - startAttemptTime > timeSyncTimeout) {
          Serial.println("\nTime sync timeout!");
          splashScreen("Time Sync Failed"); // Indicate timeout on display
          delay(1500); // Show message briefly
          break; // Exit the loop even if sync failed
      }
      // Only draw dots if time sync is in progress
      drawLoadingDots(sync_step++ % 4); // Animate dots at bottom
      delay(50); // Short delay to allow ESP8266 background tasks
    }

    // Check if sync was successful (epoch time will be > 0 if synced)
    if (millis() - startAttemptTime <= timeSyncTimeout && timeClient.getEpochTime() > 0) {
       Serial.println("\nTime synchronized.");
        // Initialize lastReminderHour on successful time sync to the current hour.
        // This prevents the reminder from triggering immediately on boot if it happens to be a new hour.
        // Requires TimeLib for the hour() function.
        time_t rawTime = timeClient.getEpochTime();
        lastReminderHour = hour(rawTime);

    } else {
       Serial.println("Proceeding, but time might not be synchronized.");
       // If time sync fails, the hourly reminder won't trigger until time sync succeeds later.
       lastReminderHour = -1; // Keep as -1 if sync failed so it triggers after first successful sync
    }

    // --- Final Setup Splash Screen ---
    splashScreen("What time is it?"); // Display final message
    delay(1500); // Show message briefly

  } else { // WiFi connection failed during setup
    Serial.println("\nWiFi Connection Failed!");
    splashScreen("No WiFi!"); // Indicate failure on display
    delay(3000); // Show message for 3 seconds
    // Set weather status to reflect no WiFi so it's shown on the weather screen later
    weatherDesc = "No WiFi";
    temperature = "--";
    iconCode = "50"; // Use a generic icon like mist/error when no WiFi
    lastReminderHour = -1; // Ensure reminder doesn't trigger without time sync
  }

  // Initialize timers for main loop screen switching and random quotes
  lastSwitch = millis(); // Start the timer for the first regular screen duration
  // Set Stanley timer so it's immediately eligible (or set it later for first quote delay)
  lastStanleyQuoteTime = millis() - stanleyQuoteMinInterval; // Allows Stanley quote immediately if random chance hits

  // Seed the random number generator using an analog read from an unconnected pin
  // This provides a somewhat random seed for random() calls.
  randomSeed(analogRead(0));
}

// --- Main Loop Function: Runs repeatedly ---
void loop() {
  // Regularly call update() to keep the time client running.
  // It will only perform a network sync based on the interval set in setup (currently 30 mins).
  timeClient.update();

  // Get current hour *after* timeClient.update() to ensure it's the latest time.
  // Use TimeLib's hour() function for consistency and clarity.
  time_t rawTime = timeClient.getEpochTime();
  int currentHour = hour(rawTime);

  // Determine if night mode should be active based on the current hour.
  nightMode = (currentHour >= nightStartHour || currentHour < nightEndHour);

  // --- Night Mode Handling ---
  if (nightMode) {
    setDisplayContrast(2); // Set a very dim contrast for night mode
    display.clearDisplay(); // Clear the display buffer
    drawTime(); // Only show time in night mode
    display.display(); // Update the display
    delay(1000); // Update time every second in night mode
    return;      // Exit loop iteration, skipping other screens and special triggers during night mode
  } else {
    // --- Day Mode Handling ---
    setDisplayContrast(255); // Set full contrast during day mode

    // --- Check and Reconnect WiFi Periodically ---
    // This runs only in day mode.
    checkAndReconnectWiFi();

    // Check if it's time to update weather data (only if WiFi is connected)
    // Also trigger an update if lastWeatherUpdate is 0 (first run after boot/reconnect).
    if (WiFi.status() == WL_CONNECTED && (millis() - lastWeatherUpdate > weatherInterval || lastWeatherUpdate == 0)) {
        Serial.println("Updating weather..."); // Debug message
        updateWeather(); // Fetch weather data. This is blocking but infrequent.
    }

    // --- Screen Management and Drawing Logic ---
    display.clearDisplay(); // Clear display buffer at the start of each drawing cycle

    // Flags to track which screen is currently active or requested.
    // showWaterReminder and showStanleyQuote are global and persist between loops.

    // --- Check if Current Special Screen Duration is Over ---
    // If a special screen is active, check if its time is up first.
    if (showWaterReminder) {
        if (millis() - waterReminderStartTime > drinkWaterDuration) {
            showWaterReminder = false; // Drink Water reminder finished
            lastSwitch = millis();     // Reset timer for the *next* screen (which will be the next regular)
            Serial.println("Drink Water duration finished. Resuming regular cycle."); // Debug
             // The regular cycle will resume or Stanley might interrupt in the next check below.
        }
    } else if (showStanleyQuote) {
        if (millis() - stanleyQuoteStartTime > stanleyQuoteDuration) {
            showStanleyQuote = false; // Stanley quote finished
            lastSwitch = millis();    // Reset timer for the *next* screen (which will be the next regular)
            // Note: screenIndex is NOT incremented here. It continues its independent cycle.
            Serial.println("Stanley Quote duration finished. Resuming regular cycle."); // Debug
        }
    }

    // --- Trigger Special Screens (if not already active and their conditions are met) ---
    // Prioritize Drink Water reminder over Stanley Quote and Regular Cycle.
    bool specialScreenTriggeredThisLoop = false; // Flag to prevent multiple special screens in one loop iteration

    // Trigger Drink Water Reminder:
    // Conditions: Not already showing reminder, not showing Stanley, time is synced (epoch > 0),
    // it's a new hour compared to the last reminder, not in night mode, and no other special screen triggered this loop.
    if (!showWaterReminder && !showStanleyQuote && timeClient.getEpochTime() > 0 && currentHour != lastReminderHour && !nightMode && !specialScreenTriggeredThisLoop) {
         // It's a new hour (and time is synced), trigger the reminder.
        showWaterReminder = true;       // Activate water reminder screen
        waterReminderStartTime = millis(); // Record when it started
        lastReminderHour = currentHour; // Record the hour this reminder is for
        // No need to reset lastSwitch here; it will be reset when this screen *finishes*.
        Serial.println("Hourly Drink Water reminder triggered!"); // Debug
        specialScreenTriggeredThisLoop = true; // A special screen was triggered
    }

    // Trigger Stanley Quote:
    // Conditions: Not showing Water reminder, not already showing Stanley, no other special screen triggered this loop,
    // WiFi is connected, minimum interval since last Stanley quote has passed, and a random chance (1 in 4) hits.
    if (!showWaterReminder && !showStanleyQuote && !specialScreenTriggeredThisLoop &&
        WiFi.status() == WL_CONNECTED && millis() - lastStanleyQuoteTime > stanleyQuoteMinInterval && random(4) == 0) {
         // Stanley conditions met, trigger the quote.
        showStanleyQuote = true;       // Activate Stanley screen
        stanleyQuoteStartTime = millis(); // Record when Stanley started
        lastStanleyQuoteTime = millis(); // Reset the timer for the next Stanley quote possibility
        // Select the random quote NOW so it doesn't change during the screen duration.
        currentStanleyQuote = stanleyQuotes[random(0, numStanleyQuotes)];
        // No need to reset lastSwitch here; it will be reset when this screen *finishes*.
        Serial.println("Stanley Quote interruption triggered!"); // Debug
        specialScreenTriggeredThisLoop = true; // A special screen was triggered
    }

    // --- Handle Regular Screen Switching (if no special screen is active or triggered) ---
    if (!showWaterReminder && !showStanleyQuote) { // Only check/advance regular cycle if no special screen is active
        // Check if the current regular screen's duration has elapsed.
        if (millis() - lastSwitch > regularScreenDurations[screenIndex]) {
             // Time for a regular screen switch.
            lastSwitch = millis(); // Reset timer for the *new* regular screen
            // Advance the regular screen index, wrapping around using the modulo operator.
            screenIndex = (screenIndex + 1) % numRegularScreens; // Move to the next regular screen (0, 1, 2, 3)
            Serial.print("Regular screen duration met. Switching to: "); Serial.println(screenIndex); // Debug
        }
    }

    // --- Draw the Currently Active Screen ---
    // Draw the special screen if one is active, otherwise draw the current regular screen.
    if (showWaterReminder) {
        drawDrinkWaterReminder();
    } else if (showStanleyQuote) {
        drawRandomStanleyQuote();
    } else {
        // Draw the current regular screen based on screenIndex (0-3)
        switch (screenIndex) {
            case 0: drawTime(); break; // Time screen
            case 1: drawDate(); break; // Date screen
            case 2: drawWeather(); break; // Weather screen
            case 3: drawQuote(); break; // Static Quote screen
            // Default case can be added for safety, but 0-3 should be the only states here.
        }
    }

    display.display(); // Push the content of the buffer to the actual OLED display

  } // End of !nightMode (Day Mode) block

  // Short delay at the end of the loop to prevent high CPU usage and allow ESP background tasks.
  // This delay is important for stability and preventing watchdog timer resets.
  delay(100);
}

// --- Function Definitions ---

// Sets the display contrast (brightness) using a command.
// contrast: Value from 0 (dimmest) to 255 (brightest).
void setDisplayContrast(uint8_t contrast) {
  display.ssd1306_command(SSD1306_SETCONTRAST); // SSD1306 command to set contrast
  display.ssd1306_command(contrast);          // Send the contrast value
}

// Displays a centered text message briefly, typically used during setup/initialization.
// text: The String message to display.
void splashScreen(String text) {
  display.clearDisplay(); // Clear the buffer
  display.setTextSize(1); // Set text size to 1 (as requested for consistent size)
  int16_t x1, y1; // Variables to store text bounds coordinates
  uint16_t w, h;  // Variables to store text bounds width and height
  // Get the pixel bounds of the text string to calculate its size.
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  // Calculate cursor position to center the text horizontally and vertically on the screen.
  display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
  display.print(text);  // Print the text string
  display.display();  // Show the text on the actual display
  // Note: The duration the splash screen is visible is controlled by delay() calls in setup().
}

// Draws simple animated dots at the bottom for loading progress indication during setup.
// step: The current animation step (0 to 3) to determine which dots are lit.
void drawLoadingDots(int step) {
    int dotX = (SCREEN_WIDTH / 2) - 10; // Starting X position for the dots (centered below screen center)
    int dotY = SCREEN_HEIGHT - 5;     // Y position for the dots (near bottom edge)

    // Clear the small area where the dots are drawn to animate them.
    // This prevents previous dots from remaining on the screen.
    display.fillRect(dotX, dotY - 2, 20, 5, BLACK);
    // Only display dots if the step is valid (0, 1, 2, or 3). Otherwise, it just clears the area.
    if (step >= 0 && step < 4) {
        for(int i = 0; i < 4; i++) {
            // Draw a filled circle (dot) if the current dot index (i) is less than or equal to the step.
            if (i <= step) {
                 display.fillCircle(dotX + (i * 5), dotY, 2, WHITE); // Draw a dot with spacing
            }
        }
    }
    display.display(); // Update display with the new dots
}

// Checks WiFi status periodically and attempts to reconnect if disconnected.
// This function is called in the main loop during day mode.
void checkAndReconnectWiFi() {
    // Check WiFi status only at the defined interval to avoid constant checks.
    if (millis() - lastWiFiCheck > wifiCheckInterval) {
        lastWiFiCheck = millis(); // Reset the check timer

        // If WiFi is not connected (status is not WL_CONNECTED)
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi connection lost. Attempting to reconnect...");
            // We are not showing a splash screen here during runtime disconnection,
            // but you could add a small indicator on the display if desired.

            // Attempt to reconnect. WiFi.begin() is non-blocking.
            WiFi.begin(ssid, password);
            // The loop() will continue, and the WiFi.status() will be checked
            // again in subsequent iterations by this function. The status will
            // eventually become WL_CONNECTED if the reconnection is successful.
        }
    }
}

// Draws the current time, centered on the screen.
void drawTime() {
  display.setTextSize(2); // Use larger font for time (font height is 16 pixels for size 2)
  String timeStr = timeClient.getFormattedTime(); // Get formatted time (HH:MM:SS)
  // Optionally remove seconds if you prefer HH:MM format:
  // timeStr = timeStr.substring(0, 5); // Uncomment this line for HH:MM

  int16_t x1, y1; // Variables for text bounds
  uint16_t w, h;  // Variables for text bounds
  // Get the pixel bounds of the time string to calculate its size.
  display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
  // Calculate cursor position to center the time vertically and horizontally.
  display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
  display.print(timeStr); // Print the time string
}

// Draws the current date, centered on the screen.
// Uses TimeLib to format the date from the epoch time.
void drawDate() {
  time_t rawTime = timeClient.getEpochTime(); // Get epoch time (seconds since Unix epoch)
  struct tm * ti; // Pointer to a struct to hold broken-down time components
  ti = localtime (&rawTime); // Convert epoch time to local time struct

  char dateBuffer[20]; // Buffer to hold the formatted date string (size 20 is sufficient for "DD.MM.YYYY\0")
  // Format time struct into "DD.MM.YYYY" format (e.g., "20.04.2025").
  // strftime is a standard C function for formatting time.
  strftime(dateBuffer, sizeof(dateBuffer), "%d.%m.%Y", ti);

  display.setTextSize(2); // Use larger font for date (size 2)
  int16_t x1, y1; // Variables for text bounds
  uint16_t w, h;  // Variables for text bounds
  // Get the pixel bounds of the date string.
  display.getTextBounds(dateBuffer, 0, 0, &x1, &y1, &w, &h);
  // Calculate cursor position to center the date vertically and horizontally.
  display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
  display.print(dateBuffer); // Print the date string
}

// Draws the original static quote with some random "stars" (single pixels).
void drawQuote() {
  display.setTextSize(1); // Use smaller font for the quote (size 1)
  const char* quote = "Do what you can't"; // The static quote text

  // Add a few random "stars" (single pixels) for visual interest on this screen.
  for (int i = 0; i < 5; i++) {
    // Draw a pixel at a random X and Y coordinate within the screen bounds.
    display.drawPixel(random(SCREEN_WIDTH), random(SCREEN_HEIGHT), WHITE);
  }

  int16_t x1, y1; // Variables for text bounds
  uint16_t w, h;  // Variables for text bounds
  // Get the pixel bounds of the quote text.
  display.getTextBounds(quote, 0, 0, &x1, &y1, &w, &h);
  // Position the quote nicely, slightly above the vertical center.
  // Adjust y-position as needed for desired placement.
  display.setCursor((SCREEN_WIDTH - w) / 2, 10);
  display.print(quote); // Print the quote text
}

// Draws a random Stanley Parable style quote from the stanleyQuotes array.
// Handles multi-line quotes using '\n'.
void drawRandomStanleyQuote() {
    display.setTextSize(1); // Use smaller font for quotes (font height is 8 pixels for size 1)
    const int lineHeight = 8 + 2; // Font height (8) + 2 pixels spacing between lines

    // Get the stored quote as a C-style string for strtok.
    // currentStanleyQuote is a String object, so we need its C-string representation.
    const char* quote = currentStanleyQuote.c_str();
    // Create a mutable copy of the quote string because strtok modifies the string it operates on.
    char tempQuote[strlen(quote) + 1];
    strcpy(tempQuote, quote); // Copy the string into the temporary buffer

    // Manually count lines to determine the overall block height for centering.
    int lineCount = 0;
    const char* tempPtr = quote; // Use the original quote string for counting lines
    while (*tempPtr) {
        if (*tempPtr == '\n') {
            lineCount++;
        }
        tempPtr++;
    }
    lineCount++; // Account for the last line (or the only line if no '\n')

    // Calculate the initial Y coordinate to center the entire text block vertically.
    int totalTextHeight = lineCount * lineHeight - 2; // Total height minus the last line's spacing
    int currentY = (SCREEN_HEIGHT - totalTextHeight) / 2;
    // Ensure the starting Y coordinate doesn't go off the top of the screen.
     if (currentY < 0) currentY = 0;

    // Use strtok to split the string into lines based on the newline character '\n'.
    char* line = strtok(tempQuote, "\n"); // Get the first line

    // Iterate through each line
    while (line != NULL) {
        int16_t x1, y1;
        uint16_t w, h;
        // Calculate text bounds for the current line (needed primarily for its width).
        display.getTextBounds(line, 0, 0, &x1, &y1, &w, &h);

        // Calculate the centered X position for the current line.
        int cursorX = (SCREEN_WIDTH - w) / 2;
        // Ensure text doesn't go off the left edge if a line is wider than the screen.
        if (cursorX < 0) cursorX = 0;

        display.setCursor(cursorX, currentY); // Set the cursor position for this line
        display.print(line); // Print the current line

        currentY += lineHeight; // Move the Y coordinate down for the next line, including spacing.

        line = strtok(NULL, "\n"); // Get the next line (strtok remembers its position)
    }
}

// Draws the Drink Water reminder text and an animated upright glass that "drains".
void drawDrinkWaterReminder() {
    display.setTextSize(1); // Use smaller font for the reminder text
    const char* reminderText = "Drink Water!"; // The reminder text

    int16_t x1, y1;
    uint16_t w, h;

    // Calculate text bounds for centering the reminder text.
    display.getTextBounds(reminderText, 0, 0, &x1, &y1, &w, &h);
    int textX = (SCREEN_WIDTH - w) / 2;
    // Position text higher to make space for the smaller glass below it.
    int textY = (SCREEN_HEIGHT / 2) - h - 2; // Centered vertically, then move up by text height + spacing

    display.setCursor(textX, textY);
    display.print(reminderText);

    // --- Draw Upright Glass (U shape) and Draining Animation ---
    int glassX = (SCREEN_WIDTH - 20) / 2; // X position for the glass (centered, made slightly narrower)
    // Adjust glass Y position lower to create spacing under the text.
    int glassY = (SCREEN_HEIGHT / 2) + 2; // Start below the vertical center with some spacing
    int glassWidth = 20; // Made slightly narrower
    int glassHeight = 16; // Made vertically smaller

    // Draw upright glass outline: two vertical sides and a base line at the bottom.
    display.drawLine(glassX, glassY, glassX, glassY + glassHeight, WHITE); // Left side
    display.drawLine(glassX + glassWidth, glassY, glassX + glassWidth, glassY + glassHeight, WHITE); // Right side
    display.drawLine(glassX, glassY + glassHeight, glassX + glassWidth, glassY + glassHeight, WHITE); // Bottom base line

    // Calculate inner dimensions for the "water" fill (relative to the upright glass outline).
    int innerGlassX = glassX + 2; // Inner area starts 2 pixels in from the left side
    int innerGlassY = glassY + 1; // Inner area starts 1 pixel down from the top opening
    int innerGlassWidth = glassWidth - 4; // Inner width is total width minus 2 pixels on each side
    int innerGlassHeight = glassHeight - 1; // Inner height is glass height minus base thickness (1 pixel)

    // Calculate "water" height based on animation progress (draining animation).
    // Get elapsed time since the reminder screen became active.
    unsigned long elapsedTime = millis() - waterReminderStartTime;
    // Calculate fill percentage (clamped between 0.0 and 1.0). This is the "empty" percentage now.
    float emptyPercentage = min(1.0f, (float)elapsedTime / drinkWaterDuration);
    // Calculate current water height. It starts full (innerHeight) and decreases over time towards 0.
    int waterHeight = (int)(innerGlassHeight * (1.0f - emptyPercentage));

    // Draw the filled "water" rectangle.
    // It starts from the top of the water level (innerGlassY + (innerGlassHeight - waterHeight))
    // and extends downwards to the bottom of the inner glass area (innerGlassY + innerGlassHeight).
    if (waterHeight > 0) { // Only draw if there is still water
        // Calculate the Y coordinate of the *top* of the water level.
        int waterLevelTopY = innerGlassY + (innerGlassHeight - waterHeight);
        // Ensure water doesn't draw above the top of the inner glass area due to floating point inaccuracies.
        if (waterLevelTopY < innerGlassY) waterLevelTopY = innerGlassY;

        // Draw the filled rectangle representing the water.
        display.fillRect(innerGlassX, waterLevelTopY, innerGlassWidth, waterHeight, WHITE);
    }
}

// Draws the weather icon and text information (temperature and description).
void drawWeather() {
  // Draw the animated icon in the dedicated area on the left (~42 pixels wide).
  drawWeatherIcon(iconCode);

  // Draw temperature (large font).
  display.setTextSize(2); // Larger font for temperature (size 2)
  // Create the temperature string, including the degree symbol (°).
  // The degree symbol is character 248 in the Code Page 437 font (enabled by display.cp437(true)).
  // Round the temperature float to the nearest whole number before casting to int.
  String tempStr = String((int)round(temperature.toFloat())) + (char)248 + "C";

  // Calculate text bounds for positioning the temperature string.
  int16_t x1_temp, y1_temp;
  uint16_t w_temp, h_temp;
  display.getTextBounds(tempStr, 0, 0, &x1_temp, &y1_temp, &w_temp, &h_temp);

  // Calculate centered X position for the temperature text to the right of the icon area.
  int textStartX = 42; // The area for text starts approximately 42 pixels from the left edge.
  int availableWidth = SCREEN_WIDTH - textStartX; // The width available for text.
  // Calculate X position to center the temperature text within the available width.
  int tempX = textStartX + (availableWidth - w_temp) / 2;
  // Ensure the calculated X position doesn't go left of the icon area if the text is very wide.
  if (tempX < textStartX) tempX = textStartX;

  // Set the Y position for the temperature text (near the top of the screen).
  int tempY = 2;

  // Set the cursor and print the temperature string.
  display.setCursor(tempX, tempY);
  display.print(tempStr);

  // Draw weather description (small font).
  display.setTextSize(1); // Smaller font for description (size 1)
  int16_t x1_desc, y1_desc;
  uint16_t w_desc, h_desc;
  // Get text bounds for the description string.
  display.getTextBounds(weatherDesc, 0, 0, &x1_desc, &y1_desc, &w_desc, &h_desc);
   // Calculate X position to center the description within the available area (right of icon).
  int cursorX = textStartX + (availableWidth - w_desc) / 2;
  // Ensure the cursor doesn't go back into the icon area if the text is wide.
  if (cursorX < textStartX) cursorX = textStartX;

  // Position the description text near the bottom of the screen.
  display.setCursor(cursorX, SCREEN_HEIGHT - h_desc - 2); // Position Y near bottom with a small margin
  display.print(weatherDesc); // Print the weather description
}

// Draws the animated weather icon based on the OpenWeatherMap icon code.
// Icons are designed to fit within a roughly 32x32 pixel area on the left side.
// Uses sin/cos functions for simple animations based on elapsed time (millis()).
void drawWeatherIcon(String code) {
  // Use floating point time (seconds) for smoother sin/cos animations.
  float animTime = millis() / 1000.0f;
  // Base coordinates for the icon drawing area (top-left corner within the left section).
  int iconX = 5; // Start 5 pixels from the left edge
  int iconY = 0; // Start at the top edge
  // The icon drawing area is implicitly within the left ~42 pixels used by drawWeather.

  display.setTextSize(1); // Ensure text size is reset to default if it was changed elsewhere.

  // --- Clear Sky (01d/01n) ---
  if (code.startsWith("01")) { // Icon code starts with "01" for clear sky.
    int centerX = iconX + 16; // Center X coordinate for the sun/moon body within the icon area.
    int centerY = iconY + 16; // Center Y coordinate for the sun/moon body.
    int radius = 6; // Radius of the sun/moon body.
    // Draw central body (filled circle).
    display.fillCircle(centerX, centerY, radius, WHITE); // Filled sun/moon body

    // Animated rays for sun (pulsating length and slow rotation).
    if (code.endsWith("d")) { // Day: Sun with rays
      for (int i = 0; i < 8; i++) { // Draw 8 rays
        // Calculate angle for each ray, adding a slow rotation based on animTime.
        float angle = (2 * PI / 8.0f) * i + animTime * 0.5f;
        float baseLen = radius + 2.0f; // Base length of the ray from the center.
        // Animated length pulsating using a sin wave.
        float animLen = 3.0f + 1.5f * sin(animTime * 5.0f + i);
        // Calculate start and end points of the ray using trigonometry.
        float rayStartX = centerX + baseLen * cos(angle);
        float rayStartY = centerY + baseLen * sin(angle);
        float rayEndX = centerX + (baseLen + animLen) * cos(angle);
        float rayEndY = centerY + (baseLen + animLen) * sin(angle);
        // Draw the ray as a line.
        display.drawLine(rayStartX, rayStartY, rayEndX, rayEndY, WHITE);
      }
    } else { // Night: Moon (simple static crescent created by drawing a black circle over a white one)
       // Draw a black circle slightly offset to cut out a crescent shape from the white circle.
       display.fillCircle(centerX + 2, centerY - 1, radius, BLACK);
    }
  }
  // --- Few Clouds (02d/02n) ---
  else if (code.startsWith("02")) { // Few clouds icon (sun/moon partially covered by a cloud)
    // Draw sun/moon first (slightly smaller and offset from the icon area center).
    int sunX = iconX + 8 + (int)(2 * sin(animTime)); // Gentle horizontal movement for sun/moon
    int sunY = iconY + 10; // Y position for sun/moon
    int sunRadius = 4; // Radius of the smaller sun/moon
    display.fillCircle(sunX, sunY, sunRadius, WHITE);
    // Cut out for moon crescent if it's night.
    if (!code.endsWith("d")) { display.fillCircle(sunX + 2, sunY - 1, sunRadius, BLACK); }

    // Draw cloud shape over the sun/moon, using combined filled circles.
    int cloudBaseX = iconX + 10; // Base X position for the cloud shape
    int cloudBaseY = iconY + 14 + (int)(1.5 * cos(animTime * 0.8)); // Gentle vertical movement for the cloud

    // Combine multiple filled circles to form a fluffy cloud shape.
    display.fillCircle(cloudBaseX + 5, cloudBaseY + 3, 5, WHITE);
    display.fillCircle(cloudBaseX + 15, cloudBaseY + 4, 6, WHITE);
    display.fillCircle(cloudBaseX + 10, cloudBaseY + 6, 5, WHITE);
    display.fillCircle(cloudBaseX + 20, cloudBaseY + 6, 4, WHITE);
  }
  // --- Scattered Clouds (03d/03n) ---
  else if (code.startsWith("03")) { // Scattered clouds icon (two smaller clouds)
      // Draw two smaller, separate cloud shapes using combined circles, with subtle drift.
      int cloud1BaseX = iconX + 4 + (int)(2 * sin(animTime * 0.7)); // Gentle movement for cloud 1
      int cloud1BaseY = iconY + 8 + (int)(1.5 * cos(animTime * 0.9));
      // Combine circles for cloud 1 shape.
      display.fillCircle(cloud1BaseX + 3, cloud1BaseY + 2, 4, WHITE);
      display.fillCircle(cloud1BaseX + 10, cloud1BaseY + 3, 5, WHITE);
      display.fillCircle(cloud1BaseX + 7, cloud1BaseY + 5, 4, WHITE);

      int cloud2BaseX = iconX + 15 + (int)(1.5 * cos(animTime * 0.6)); // Gentle movement for cloud 2
      int cloud2BaseY = iconY + 18 + (int)(2 * sin(animTime * 0.8));
      // Combine circles for cloud 2 shape.
      display.fillCircle(cloud2BaseX + 2, cloud2BaseY + 1, 3, WHITE);
      display.fillCircle(cloud2BaseX + 8, cloud2BaseY + 2, 4, WHITE);
      display.fillCircle(cloud2BaseX + 5, cloud2BaseY + 4, 3, WHITE);
  }
  // --- Broken Clouds (04d/04n) ---
  else if (code.startsWith("04")) { // Broken clouds icon (single larger, denser cloud)
     // Single larger, denser cloud shape with gentle wobble, using combined circles.
    int cloudBaseX = iconX + 5 + (int)(1.5 * sin(animTime * 0.5)); // Gentle movement
    int cloudBaseY = iconY + 10 + (int)(1.5 * cos(animTime * 0.7));
    // Combine circles for a denser cloud shape.
    display.fillCircle(cloudBaseX + 6, cloudBaseY + 4, 6, WHITE);
    display.fillCircle(cloudBaseX + 16, cloudBaseY + 5, 7, WHITE);
    display.fillCircle(cloudBaseX + 10, cloudBaseY + 8, 6, WHITE);
    display.fillCircle(cloudBaseX + 22, cloudBaseY + 7, 5, WHITE);
    display.fillCircle(cloudBaseX + 14, cloudBaseY + 3, 5, WHITE);

    // Note: OLEDs are monochrome, so we can't easily show "darker" clouds visually
    // beyond just the shape.
  }
  // --- Shower Rain (09d/09n) ---
  else if (code.startsWith("09")) { // Shower rain icon (cloud with angled rain)
    // Draw cloud shape using combined circles.
    int cloudBaseX = iconX + 6; // Base X for cloud
    int cloudBaseY = iconY + 5; // Base Y for cloud
    // Combine circles for cloud shape.
    display.fillCircle(cloudBaseX + 5, cloudBaseY + 3, 6, WHITE);
    display.fillCircle(cloudBaseX + 15, cloudBaseY + 4, 7, WHITE);
    display.fillCircle(cloudBaseX + 10, cloudBaseY + 6, 6, WHITE);
    display.fillCircle(cloudBaseX + 20, cloudBaseY + 7, 5, WHITE);

    // Animated rain drops (angled, falling).
    for (int i = 0; i < 4; i++) { // Draw 4 rain drops
        // Calculate start and end points for rain drops with animation and staggering.
        // dropStartX: Staggered start X with slight wobble using sin.
        int dropStartX = iconX + 10 + i * 4 + (int)(sin(animTime * 8 + i) * 2);
        // dropStartY: Y position cycles downwards using fmod for looping animation.
        int dropStartY = cloudBaseY + 10 + (int)(fmod(animTime * 15 + i * 5, 15));
        int dropEndY = dropStartY + 4; // Raindrop length
        int dropEndX = dropStartX - 2; // Angle the rain by offsetting X endpoint.
        // Ensure drops stay below the cloud base.
        if (dropStartY < cloudBaseY + 10) dropStartY = cloudBaseY + 10;
        // Draw the rain drop as a line.
        display.drawLine(dropStartX, dropStartY, dropEndX, dropEndY, WHITE);
    }
  }
  // --- Rain (10d/10n) ---
  else if (code.startsWith("10")) { // Rain icon (sun/moon with vertical rain)
    // Sun/Moon partially hidden by cloud.
    int sunX = iconX + 8 + (int)(1.5 * sin(animTime * 0.6)); // Gentle movement for sun/moon
    int sunY = iconY + 8; // Y position for sun/moon
    int sunRadius = 3; // Radius of smaller sun/moon
    display.fillCircle(sunX, sunY, sunRadius, WHITE);
    // Cut out for moon crescent if it's night.
    if (!code.endsWith("d")) { display.fillCircle(sunX + 1, sunY - 1, sunRadius, BLACK); }

    // Draw cloud shape using combined circles.
    int cloudBaseX = iconX + 8; // Base X for cloud
    int cloudBaseY = iconY + 10; // Base Y for cloud
    // Combine circles for cloud shape.
    display.fillCircle(cloudBaseX + 4, cloudBaseY + 2, 5, WHITE);
    display.fillCircle(cloudBaseX + 14, cloudBaseY + 3, 6, WHITE);
    display.fillCircle(cloudBaseX + 8, cloudBaseY + 5, 5, WHITE);
    display.fillCircle(cloudBaseX + 18, cloudBaseY + 6, 4, WHITE);

    // Animated rain drops (vertical, falling).
    for (int i = 0; i < 3; i++) { // Draw 3 rain drops
        // Calculate position for rain drops with animation and staggering.
        // dropX: Staggered X with slight wobble using cos.
        int dropX = iconX + 12 + i * 4 + (int)(cos(animTime * 7 + i) * 1.5);
        // dropY: Y position cycles downwards using fmod.
        int dropY = cloudBaseY + 10 + (int)(fmod(animTime * 18 + i * 6, 12));
        // Ensure drops stay below the cloud base.
        if (dropY < cloudBaseY + 10) dropY = cloudBaseY + 10;
        // Draw the rain drop as a short vertical line (multiple pixels for visibility).
        display.drawLine(dropX, dropY, dropX, dropY + 3, WHITE);
    }
  }
   // --- Thunderstorm (11d/11n) ---
  else if (code.startsWith("11")) { // Thunderstorm icon (cloud with lightning)
    // Draw cloud shape using combined circles (similar to broken clouds).
    int cloudBaseX = iconX + 5; // Base X for cloud
    int cloudBaseY = iconY + 6; // Base Y for cloud
    // Combine circles for cloud shape.
    display.fillCircle(cloudBaseX + 6, cloudBaseY + 4, 6, WHITE);
    display.fillCircle(cloudBaseX + 16, cloudBaseY + 5, 7, WHITE);
    display.fillCircle(cloudBaseX + 10, cloudBaseY + 8, 6, WHITE);
    display.fillCircle(cloudBaseX + 22, cloudBaseY + 7, 5, WHITE);
    display.fillCircle(cloudBaseX + 14, cloudBaseY + 3, 5, WHITE);

    // Animated lightning bolt (flashing based on time integer part).
    // The bolt flashes on and off approximately 1.5 times per second.
    // (int)(animTime * 3.0f) creates a sequence like 0, 0, 0, 1, 1, 1, 2, 2, 2, ...
    // Checking if it's even makes it flash on and off.
    if ((int)(animTime * 3.0f) % 2 == 0) {
        // Calculate lightning bolt segments.
        int boltX = iconX + 10 + random(0, 10); // Randomize start X slightly for variation
        int boltY = cloudBaseY + 12; // Starting Y below the cloud
        // Draw lightning bolt as connected line segments.
        display.drawLine(boltX, boltY, boltX - 4, boltY + 6, WHITE);
        display.drawLine(boltX - 4, boltY + 6, boltX, boltY + 6, WHITE);
        display.drawLine(boltX, boltY + 6, boltX - 6, boltY + 12, WHITE);
    }
    // Optional: Could add rain drops here too if desired for a more complete thunderstorm icon.
  }
  // --- Snow (13d/13n) ---
  else if (code.startsWith("13")) { // Snow icon (cloud with falling snowflakes)
    // Draw cloud shape using combined circles.
    int cloudBaseX = iconX + 6; // Base X for cloud
    int cloudBaseY = iconY + 7; // Base Y for cloud
    // Combine circles for cloud shape.
    display.fillCircle(cloudBaseX + 5, cloudBaseY + 3, 6, WHITE);
    display.fillCircle(cloudBaseX + 15, cloudBaseY + 4, 7, WHITE);
    display.fillCircle(cloudBaseX + 10, cloudBaseY + 6, 6, WHITE);
    display.fillCircle(cloudBaseX + 20, cloudBaseY + 7, 5, WHITE);

    // Animated snowflakes (asterisk shape, falling gently with slight side drift).
    for (int i = 0; i < 4; i++) { // Draw 4 snowflakes
        // Calculate snowflake position with animation and staggering.
        // flakeX: Staggered X with slight side drift using cos.
        float flakeX = iconX + 8 + i * 5 + cos(animTime * 1.5f + i) * 2.0f;
        // flakeY: Y position cycles downwards using fmod.
        float flakeY = cloudBaseY + 10 + fmod(animTime * 4.0f + i * 4.0f, 15.0f);
        // Ensure flakes stay below the cloud base.
        if (flakeY < cloudBaseY + 10) flakeY = cloudBaseY + 10;

        // Draw asterisk (*) shape using multiple lines for each snowflake.
        display.drawLine(flakeX - 2, flakeY, flakeX + 2, flakeY, WHITE); // Horizontal line
        display.drawLine(flakeX, flakeY - 2, flakeX, flakeY + 2, WHITE); // Vertical line
        display.drawLine(flakeX - 1, flakeY - 1, flakeX + 1, flakeY + 1, WHITE); // Diagonal /
        display.drawLine(flakeX + 1, flakeY - 1, flakeX - 1, flakeY + 1, WHITE); // Diagonal \
    }
  }
  // --- Mist/Fog/Haze (50d/50n) ---
  else if (code.startsWith("50")) { // Mist/Fog icon (horizontal lines moving)
      // Draw 3 horizontal lines, gently moving vertically and maybe horizontally.
      for (int i = 0; i < 3; i++) { // Draw 3 lines
          // Calculate line Y position with vertical oscillation using sin.
          int lineY = iconY + 10 + i * 6 + (int)(sin(animTime * 1.2f + i * 0.6f) * 2.0f);
          // Calculate line X position with horizontal oscillation using cos.
          int lineX = iconX + 2 + (int)(cos(animTime * 0.8f + i * 0.4f) * 1.5f);
          // Calculate line width, varying slightly using abs(cos).
          int lineWidth = 28 - abs((int)(cos(animTime * 0.8f + i * 0.4f) * 3.0f));
          // Clamp Y position to stay within reasonable bounds within the icon area.
          if (lineY < iconY + 8) lineY = iconY + 8;
          if (lineY > iconY + 26) lineY = iconY + 26;

          // Draw the line as a filled rectangle for thickness.
          display.fillRect(lineX, lineY, lineWidth, 2, WHITE);
      }
  }
  // --- Unknown / Default Icon ---
  else { // Icon for weather codes not explicitly handled or in case of API errors.
    int centerX = iconX + 16; // Center X for the question mark icon
    int centerY = iconY + 16; // Center Y for the question mark icon
    // Draw a simple question mark (?) icon to indicate unknown status.
    display.drawCircle(centerX, centerY, 10, WHITE); // Outer circle
    display.setTextSize(2); // Temporarily larger text size for the question mark character.
    display.setCursor(centerX - 4, centerY - 7); // Position the question mark inside the circle.
    display.print("?"); // Print the question mark character.
    display.setTextSize(1); // Reset text size to default after drawing the question mark.
  }
}

// Fetches weather data from the OpenWeatherMap API over HTTP.
// Parses the JSON response to extract temperature, description, and icon code.
void updateWeather() {
  // Only attempt to update weather if WiFi is connected.
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Skipping weather update.");
    // Set weather status variables to indicate no WiFi connection.
    weatherDesc = "No WiFi";
    temperature = "--";
    iconCode = "50"; // Use a generic error/mist icon.
    lastWeatherUpdate = millis(); // Still update timestamp to prevent rapid retries if WiFi is down.
    return; // Exit the function early.
  }

  Serial.println("Attempting to fetch weather data...");
  WiFiClient client; // Create a WiFiClient object for network communication.
  HTTPClient http; // Create an HTTPClient object.

  // Construct the full API URL using the defined constants.
  String url = "http://" + String(API_HOST) + "/data/2.5/weather?q=" + CITY + "," + COUNTRY + "&units=metric&appid=" + API_KEY;

  Serial.print("[HTTP] Requesting URL: ");
  Serial.println(url);

  // Begin the HTTP request using the WiFiClient instance.
  http.begin(client, url);
  // setConnectTimeout and setTimeout methods were removed for broader compatibility
  // with different ESP8266 Core versions. Default timeouts are used.

  // Send the HTTP GET request and get the HTTP response code.
  int httpCode = http.GET();

  // Check the HTTP response code. A positive code indicates a successful request was made.
  if (httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    // If the request was successful (HTTP_CODE_OK = 200) or moved permanently (301 - though less likely for API)
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
      String payload = http.getString(); // Get the response payload (JSON data) as a String.
      // Serial.println("Received payload:\n" + payload); // Debug: Uncomment to print raw JSON payload

      // Create a DynamicJsonDocument to parse the JSON payload.
      // The size (2048) is an estimate; you might need to tune this based on the actual payload size.
      // Use ArduinoJson Assistant (https://arduinojson.org/v6/assistant/) to estimate size accurately.
      DynamicJsonDocument doc(2048);
      // Deserialize the JSON payload into the JsonDocument.
      DeserializationError error = deserializeJson(doc, payload);

      // Check for JSON parsing errors.
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        weatherDesc = "JSON Error"; // Update display status to indicate parsing failure.
        temperature = "--";
        iconCode = "50"; // Use a generic error icon.
      } else {
        // Safely access JSON elements and extract weather data.
        // Use containsKey() and isNull() checks to prevent crashes if keys or values are missing.
        if (doc.containsKey("main") && !doc["main"]["temp"].isNull() &&
            doc.containsKey("weather") && doc["weather"].is<JsonArray>() && doc["weather"].size() > 0 &&
            !doc["weather"][0]["main"].isNull() && !doc["weather"][0]["icon"].isNull())
        {
          float tempFloat = doc["main"]["temp"]; // Get temperature as a float.
          // Round the float temperature to the nearest integer and convert to a String.
          temperature = String((int)round(tempFloat));

          // Get weather main description and icon code as Strings.
          weatherDesc = doc["weather"][0]["main"].as<String>();
          iconCode = doc["weather"][0]["icon"].as<String>();

          // Print updated weather data to Serial for debugging.
          Serial.println("Weather data updated successfully:");
          Serial.println(" Temp: " + temperature + "C");
          Serial.println(" Desc: " + weatherDesc);
          Serial.println(" Icon: " + iconCode);
        } else {
           // Handle cases where the expected JSON structure is different or data is missing.
           Serial.println("Error: Unexpected JSON structure or missing data.");
           weatherDesc = "API Format Err"; // Update display status.
           temperature = "--";
           iconCode = "50"; // Use a generic error icon.
        }
      }
    } else {
      // Handle HTTP errors (e.g., 404 Not Found, 401 Unauthorized, 429 Too Many Requests).
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      weatherDesc = "HTTP Error"; // Update display status.
      temperature = "--";
      iconCode = "50"; // Use a generic error icon.
    }
  } else {
    // Handle connection errors or timeouts where no HTTP code is received.
    Serial.printf("[HTTP] GET... failed, connection error or timeout: %s\n", http.errorToString(httpCode).c_str());
    weatherDesc = "Connect Err"; // Update display status.
    temperature = "--";
    iconCode = "50"; // Use a generic error icon.
  }

  http.end(); // Close the connection and free HTTPClient resources.
  lastWeatherUpdate = millis(); // Update the timestamp regardless of success/failure to respect the interval.
}
