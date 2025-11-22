#ifndef TILT_SENSOR_H
#define TILT_SENSOR_H

#include <Arduino.h>

class TiltSensor
{
private:
    uint8_t _pin;
    bool _normallyClosed; // True if the sensor is closed when "Safe/Upright"
    int _debounceTime;

public:
    /**
     * @param pin The GPIO pin the sensor is connected to.
     * @param normallyClosed If true, the sensor connects to ground (LOW) when upright.
     */
    TiltSensor(uint8_t pin, bool normallyClosed = true);

    void begin();

    /**
     * Returns true if the sensor detects a tilt (alarm state).
     */
    bool isTilted();

    /**
     * Returns true if the sensor is in its standard upright position.
     */
    bool isUpright();
};

#endif