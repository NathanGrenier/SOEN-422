#include "TiltSensor.h"

TiltSensor::TiltSensor(uint8_t pin, bool normallyClosed)
{
    _pin = pin;
    _normallyClosed = normallyClosed;
}

void TiltSensor::begin()
{
    pinMode(_pin, INPUT_PULLUP);
}

bool TiltSensor::isTilted()
{
    int reading = digitalRead(_pin);

    if (_normallyClosed)
    {
        return (reading == HIGH);
    }
    else
    {
        return (reading == LOW);
    }
}

bool TiltSensor::isUpright()
{
    return !isTilted();
}