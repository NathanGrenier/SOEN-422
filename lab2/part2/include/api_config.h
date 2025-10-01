#pragma once

#include <Arduino.h>

namespace Api
{
    // Base URL for the API
    constexpr const char *BASE_URL = "https://iotjukebox.onrender.com";

    // API Endpoints
    namespace Endpoints
    {
        constexpr const char *GET_SONG = "/song";
        constexpr const char *POST_PREFERENCE = "/preference";
        constexpr const char *GET_PREFERENCE = "/preference";
    }

    namespace UrlBuilder
    {

        /**
         * @brief Builds the URL for getting a song by its Name.
         * @param songName The name of the song.
         * @return The complete URL as a String.
         * @example https://iotjukebox.onrender.com/song?name=<name>
         */
        String getSong(const String &songName)
        {
            return String(BASE_URL) + Endpoints::GET_SONG + "?name=" + songName;
        }

        /**
         * @brief Builds the URL for setting a song preference.
         * @param studentId The student's ID.
         * @param bluetoothAddress The Bluetooth address of the device.
         * @param songName The ID of the song.
         * @return The complete URL as a String.
         * @example https://iotjukebox.onrender.com/preference?id=00000&key=mydevice&value=harrypotter`
         */
        String postPreference(unsigned int studentId, const String &bluetoothAddress, const String &songName)
        {
            return String(BASE_URL) + Endpoints::POST_PREFERENCE +
                   "?id=" + String(studentId) +
                   "&key=" + bluetoothAddress +
                   "&value=" + songName;
        }

        /**
         * @brief Builds the URL for retrieving a song preference.
         * @param studentId The student's ID.
         * @param bluetoothAddress The Bluetooth address of the device.
         * @return The complete URL as a String.
         * @example https://iotjukebox.onrender.com/preference?id=00000&key=mydevice
         */
        String getPreference(unsigned int studentId, const String &bluetoothAddress)
        {
            return String(BASE_URL) + Endpoints::GET_PREFERENCE +
                   "?id=" + String(studentId) +
                   "&key=" + bluetoothAddress;
        }
    }
}