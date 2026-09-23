// Sensor.cpp
#include "USSensor.h"
#include <Arduino.h>

USSensor::USSensor(const int trigPinInput, const int echoPinInput)
{
    trigPin = trigPinInput;
    echoPin = echoPinInput;
    count = 0;
}

// takes arguments for trigPin and echoPin and set pins
void USSensor::setPins(int trigPin, int echoPin)
{
    pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
    pinMode(echoPin, INPUT);  // Sets the echoPin as an Input
}
float USSensor::setDiameter(float diameter)
{
    diameterMm = diameter;
    Serial.println("diameterMm: " + String(diameterMm));
    return diameterMm;
};
long USSensor::getSoundWaveDuration()
{
     // Clears the trigPin
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    // Sets the trigPin on HIGH state for 10 micro seconds
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(12);
    digitalWrite(trigPin, LOW);

    return pulseIn(echoPin, HIGH, 35000);
};
void USSensor::calculateFoilWidth()
{
    long averageValue = 0;
    for (size_t i = 0; i < 10; i++)
    {
       averageValue += durationArr[i];
    }
    averageValue = averageValue / 10;
    Serial.println("calculateDistance --> averageValue = " + String(averageValue));

    distanceToObjectIn_mm = ((soundSpeed/100) * averageValue) / 20000;
    Serial.println("calculateDistance --> distanceToObjectIn_mm = " + String(distanceToObjectIn_mm));

    //foilWidthValue = ((diameterMm / 2) - distance) * M_PI;             / 10 => convert to centimeters
    foilWidthValue = (((diameterMm / 2) - distanceToObjectIn_mm) * M_PI) / 10;
    Serial.println("calculateDistance --> foilWidthValue = " + String(foilWidthValue));
};
void USSensor::registerDuration()
{
    duration = getSoundWaveDuration();
    //Serial.println("registerDuration --> duration: " + String(duration));

    for (size_t i = 0; i < 9; i++)
    {
        durationArr[i] = durationArr[i + 1];
    }
    durationArr[9] = duration;
}
float USSensor::getFoilWidth()
{
    //Serial.println("foilWidth --> count: " + String(count));
    registerDuration();

    if (count == 4)
    {
        count = 0;
        calculateFoilWidth();
    }else
    {
        count++;
    }
    
    return foilWidthValue < 0 ? 0.00 : foilWidthValue;
};