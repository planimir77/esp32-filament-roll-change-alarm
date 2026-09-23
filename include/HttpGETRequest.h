#include <Arduino.h>
//#include <WiFi.h>
#include <HttpClient.h>

String httpGETRequest(const String serverName)
{
    // WiFiClient client;
    HTTPClient http;

    // Your IP address with path or Domain name with URL path
    // http.begin(client, "https://planimir-iot-server.herokuapp.com/");

    // Server name example "https://planimir-iot-server.herokuapp.com/test"
    http.begin(serverName);

    // Send HTTP POST request

    int httpResponseCode = http.GET();

    String payload = "{}";

    if (httpResponseCode > 0)
    {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        payload = http.getString();
        Serial.print("Response payload: ");
        Serial.println(payload);
    }
    else
    {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
    }
    // Free resources
    http.end();

    return payload;
}