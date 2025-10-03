/*
This code was automatically generated using the Riverside-Irvine State machine Builder tool
Version 2.9 --- 10/2/2025 21:14:34 PST
*/

#include "rims.h"

vector<String> songQueue;
signed int currentSongIndex = -1;
JsonDocument currentSongDoc;
String currentSongName = "";
volatile bool isPlaying = false;

unsigned short currentNoteIndex = 0;
unsigned long nextNoteTime = 0;
unsigned char MP_Clk;
void TimerISR()
{
   MP_Clk = 1;
}

enum MP_States
{
   MP_Clear,
   MP_Process,
   MP_FetchNextSong,
   MP_FetchSongData,
   MP_PlayNote,
   MP_SongEnd,
   MP_PrepareNote,
   MP_PlayPause,
   MP_NextSong,
   MP_PreviousSong,
   MP_Start
} MP_State;

TickFct_MusicPlayback()
{
   switch (MP_State)
   { // Transitions
   case -1:
      MP_State = MP_Start;
      break;
   case MP_Clear:
      if (1)
      {
         MP_State = MP_Process;
      }
      break;
   case MP_Process:
      if (currentSongIndex == -1 || currentSongIndex > songQueue.size())
      {
         MP_State = MP_FetchNextSong;
      }
      else if (currentSongDoc.isNull())
      {
         MP_State = MP_FetchSongData;
      }
      else if (currentSongName == currentSongDoc.name)
      {
         MP_State = MP_PrepareNote;
      }
      break;
   case MP_FetchNextSong:
      if (currentSongDoc.isNull())
      {
         MP_State = MP_FetchNextSong;
      }
      else if (!currentSongDoc.isNull())
      {
         MP_State = MP_PrepareNote;
      }
      break;
   case MP_FetchSongData:
      if (currentSongDoc.isNull())
      {
         MP_State = MP_FetchSongData;
      }
      else if (!currentSongDoc.isNull())
      {
         MP_State = MP_PrepareNote;
      }
      break;
   case MP_PlayNote:
      if (noteCounter = < totalNoteTicks)
      {
         MP_State = MP_PlayNote;
         noteCounter++
      }
      else if (noteCounter > totalNoteTicks)
      {
         MP_State = MP_PrepareNote;
      }
      else if (currentNoteIndex > currentSongDoc.melody.size() / 2)
      {
         MP_State = MP_SongEnd;
      }
      break;
   case MP_SongEnd:
      if (1)
      {
         MP_State = MP_Clear;
      }
      break;
   case MP_PrepareNote:
      if (1)
      {
         MP_State = MP_PlayNote;
         noteCounter = 0;
      }
      break;
   case MP_PlayPause:
      break;
   case MP_NextSong:
      if (currentSongIndex <= songQueue.size())
      {
         MP_State = MP_SongEnd;
      }
      else if (currentSongIndex > songQueue.size())
      {
         MP_State = MP_FetchNextSong;
      }
      break;
   case MP_PreviousSong:
      if ((currentSongIndex - 1) > 0)
      {
         MP_State = MP_Clear;
         currentSongIndex--
      }
      break;
   case MP_Start:
      if (1)
      {
         MP_State = MP_Clear;
      }
      break;
   default:
      MP_State = MP_Start;
   } // Transitions

   switch (MP_State)
   { // State actions
   case MP_Clear:
      currentSongDoc.clear();
      currentSongName = "";

      currentNoteIndex = 0;
      nextNoteTime = 0;
      isPlaying = false;
      break;
   case MP_Process:
      if (currentSongIndex > 0 && currentSongIndex <= songQueue.size())
      {
         currentSongName = songQueue[(currentSongIndex ]
      }
      break;
   case MP_FetchNextSong:
      // Fetch next song.
      // Store the name and song data.
      break;
   case MP_FetchSongData:
      // Fetch Song Data using
      // name of song
      break;
   case MP_PlayNote:
      if (noteCounter == 0)
      {
         playNote();
      }
      break;
   case MP_SongEnd:
      currentSongIndex++;

      break;
   case MP_PrepareNote:
      noteCounter = 0;
      wholeNote = (60000 * 4) / tempo;
      durationValue = melody[currentNoteIndex + 1];
      nextNoteTime = wholeNote / abs(durationValue);

      if (durationValue < 0)
      {
         nextNoteTime *= 1.5;
      }

      totalNoteTicks = nextNoteTime / PERIOD;
      break;
   case MP_PlayPause:
      // Go to this state when triggered
      // If in pause state, stay in this state
      // If unpaused, go to last state
      break;
   case MP_NextSong:
      currentSongIndex++ break;
   case MP_PreviousSong:
      // If there's no previous song:
      // Log a message then return to last state
      break;
   case MP_Start:
      break;
   default: // ADD default behaviour below
      break;
   } // State actions
}

int main()
{

   const unsigned int periodMusicPlayback = 5;
   TimerSet(periodMusicPlayback);
   TimerOn();

   MP_State = -1; // Initial state
   B = 0;         // Init outputs

   while (1)
   {
      TickFct_MusicPlayback();
      while (!MP_Clk)
         ;
      MP_Clk = 0;
   } // while (1)
} // Main