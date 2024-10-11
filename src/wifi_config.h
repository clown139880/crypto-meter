#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <EEPROM.h>

class WiFiConfig
{
public:
    static bool connect(const char *ssid, const char *password, int timeout = 3000)
    {
        Serial.println("Initializing EEPROM..." + String(ssid) + " " + String(password));

        WiFi.mode(WIFI_AP_STA); // Explicitly set the WiFi mode to station
        WiFi.begin(ssid, password);
        unsigned long startTime = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startTime < timeout)
        {
            delay(500);
        }
        return WiFi.status() == WL_CONNECTED;
    }

    static void startConfigPortal()
    {
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESP32-Config");

        DNSServer dnsServer;
        WebServer server(80);

        dnsServer.start(53, "*", WiFi.softAPIP());

        server.on("/", HTTP_GET, [&server]()
                  { server.send(200, "text/html", getConfigPage()); });

        server.on("/save", HTTP_POST, [&server, &dnsServer]()
                  {
            String newSSID = server.arg("ssid");
            String newPass = server.arg("password");
            
            // Save the new credentials to EEPROM
            saveWiFiCredentials(newSSID, newPass);
            
            server.send(200, "text/plain", "Attempting to connect with new credentials...");
            
            if (connect(newSSID.c_str(), newPass.c_str())) {
                server.stop();
                dnsServer.stop();
                WiFi.softAPdisconnect(true);
            }
            else {
                server.send(400, "text/plain", "Failed to connect with new credentials");
            } });

        server.begin();

        while (true)
        {
            dnsServer.processNextRequest();
            server.handleClient();
            delay(10);
        }
    }

    static bool loadWiFiCredentials(String &ssid, String &password)
    {
        EEPROM.begin(512);
        int addr = 0;

        // Read SSID
        char c;
        while ((c = EEPROM.read(addr++)) != 0)
        {
            ssid += c;
        }

        // Read password
        while ((c = EEPROM.read(addr++)) != 0)
        {
            password += c;
        }

        EEPROM.end();

        return ssid.length() > 0 && password.length() > 0;
    }

private:
    static String getConfigPage()
    {
        String html = R"(
        <!DOCTYPE html>
        <html>
        <head>
            <meta name="viewport" content="width=device-width, initial-scale=1">
            <style>
                body {
                    font-family: Arial, sans-serif;
                    margin: 0;
                    padding: 20px;
                    background-color: #f0f0f0;
                }
                h1 {
                    color: #333;
                }
                form {
                    background-color: white;
                    padding: 20px;
                    border-radius: 8px;
                    box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
                }
                input[type="text"], input[type="password"] {
                    width: 100%;
                    padding: 10px;
                    margin: 10px 0;
                    border: 1px solid #ddd;
                    border-radius: 4px;
                    box-sizing: border-box;
                }
                input[type="submit"] {
                    background-color: #4CAF50;
                    color: white;
                    padding: 10px 20px;
                    border: none;
                    border-radius: 4px;
                    cursor: pointer;
                }
                input[type="submit"]:hover {
                    background-color: #45a049;
                }
            </style>
        </head>
        <body>
            <h1>ESP32 WiFi Configuration</h1>
            <form method="post" action="/save">
                <input type="text" name="ssid" placeholder="SSID" required><br>
                <input type="password" name="password" placeholder="Password" required><br>
                <input type="submit" value="Save">
            </form>
        </body>
        </html>
        )";
        return html;
    }

    static void saveWiFiCredentials(const String &ssid, const String &password)
    {
        EEPROM.begin(512);
        int addr = 0;

        // Write SSID
        for (int i = 0; i < ssid.length(); ++i)
        {
            EEPROM.write(addr++, ssid[i]);
        }
        EEPROM.write(addr++, 0); // Null terminator

        // Write password
        for (int i = 0; i < password.length(); ++i)
        {
            EEPROM.write(addr++, password[i]);
        }
        EEPROM.write(addr++, 0); // Null terminator

        EEPROM.commit();
        EEPROM.end();
    }
};

#endif // WIFI_CONFIG_H
