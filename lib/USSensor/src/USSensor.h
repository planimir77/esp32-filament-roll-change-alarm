// USSensor.h
#ifndef USSENSOR_H
#define USSENSOR_H
#define M_PI 3.14159265358979323846 /* pi */

class USSensor
{
    private:
        int trigPin;
        int echoPin;
        int count;
        int tempAir = 30;
        long soundSpeed = 343000 + 606 * tempAir;
        long distanceToObjectIn_mm;
        float foilWidthValue;
        long duration;
        long durationArr[10];
        long getSoundWaveDuration();
        void calculateFoilWidth();
        void registerDuration();
    
    public:
        USSensor(const int trigPinInput, const int echoPinInput);
        float diameterMm;
        void setPins(int trigPin, int echoPin);
        float setDiameter(float diameter);
        float getFoilWidth();
};

#endif