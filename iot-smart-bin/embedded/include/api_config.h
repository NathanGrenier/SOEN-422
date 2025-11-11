#pragma once

#include "credentials.h"

namespace Api
{
    // Base URL for the API
    constexpr const char *BASE_URL = WEB_URL;

    // API Endpoints
    namespace Endpoints
    {
        constexpr const char *GET_SONG = "/song";
    }

    namespace UrlBuilder
    {

        /**
         * @brief Builds the URL for getting a song by its Name.
         * @param songName The name of the song.
         * @return The complete URL as a String.
         * @example https://iotjukebox.onrender.com/song?name=<name>
         */
        String getSongByName(const String &songName)
        {
            return String(BASE_URL) + Endpoints::GET_SONG + "?name=" + songName;
        }

        /**
         * @brief Builds the URL for getting a random song.
         * @return The complete URL as a String.
         * @example https://iotjukebox.onrender.com/song
         */
        String getSong()
        {
            return String(BASE_URL) + Endpoints::GET_SONG;
        }
    }
}