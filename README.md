---
title: Music Box
author: Gaelen Guzman
date: 2025-12-31
---

## A quick rant

This note is mostly to air my shame. I sincerely hope no one finds this repo, lest their eyes get burnt out of their skulls with how convoluted this silly box is.

I'm pushing this whole repo as one big lump thing because I foolishly worked on it in just a local repository thinking that I would never ever need to have remote access to this, but then I left my computer home for the one time I actually needed it -- and because of that, there are annoying bugs in the music box that I made for Ansel! Ugh! Hopefully I'll get to fix it someday soon...

I'm not proud of it, but I had to lean on ChatGPT a lot to build this -- I was pretty in over my head trying to get these boxes all constructed and coded up before starting my new job and having it all done in time for Christmas... I do feel like I'm starting to learn a lot about C++, but I'm skeptical that diving into big, real projects and pretending that I'm just using ChatGPT as a liferaft when I'm actually just living on it as a yacht. I would have learned much more by going through some fundamentals first, but the silly Earth-destroying, lying machine has helped me build a working product at least.

## A Music Box for Babies!

This project started when I thought to myself that Wendell would probably love to be in charge of selecting music to listen to. And as we're a screen-free house(as much as we can be at least), I wanted some kind of baby user interface that was 1. screenless, 2. intuitive for tiny hands, and 3. pretty robust to things like drool and cottage cheese and getting thrown on the floor. I thought a little record player would do the trick -- and this box is where I ended up.

## How it works, from 10,000 ft

At its heart, this box is just and MP3 player with an RFID/NFC reader -- an ESP32 mediates between the two of them to read NFC tags and send corresponding play/pause/music selection commands to the MP3 player.

Every 0.75 seconds, the NFC reader scans for nearby tags encoded with filepath info (and some bonus metadata). If there's a tag, it triggers playback from the folder on the on-board SD card of the MP3 player (e.g. folder `03`, which has a Little Folkies album on it).

The NFC reader continues to scan every 0.75 seconds to see if there's still a tag there -- if so, it just keeps playing the songs. If there's no tag for 2 sequential reads, it halts playback (and triggers a cute record scratch track).

Most of the code is focused on this main loop -- check for tags, take appropriate action if there's a change in the presence or absence of a tag (e.g. extract the NFC tag metadata or trigger a break in playback). The remainder is helper code for battery management -- **this is my first project with an on-board battery!** -- and health checkins from the peripherals. Oh! And it also checks for button presses on the two volume control buttons. That's like... it.

I'll eventually go through and clean this up a little -- there are some holdovers from when I wanted it to trigger a low-power sleep mode when the power switch was connected to a GPIO input (it's now a hard power cut, effectively disconnecting the battery -- which is fucked for its own dumb reasons).

## Future updates

While this is a pretty nice, complete little project that has been gifted to two very cute babies, there are still improvements to be made to future versions (Wendell does have two other cousins I could make boxes for!).

1. Power management -- the hard-cut power switch is stupid and means that the box has to be on if you want to charge the battery. I hate this and think it's dumb. I originally had the "power" switch just put the ESP32 into deep sleep mode, but I discovered that the Vcc pin delivers power whether or not the ESP32 is awake -- so the NFC reader and the MP3 player were both drawing battery if the box was off.
    - In the future, I'd probably include some power management circuitry that cuts delivery of power when the ESP32 goes to sleep.
    - I'm thinking a mosfet that regulates whether the NFC reader, buttons, and MP3 modules get power.

2. A shuffle mode -- to go with the standard records that I made (Little Folkies, A Charlie Brown Christmas, etc), I also conscripted all my friends and family to make little recordings with special messages for the babies. 
    - This did turn out awesome, but I really wanted the messages to play in a random order every time the disk was played. The DFRobot Miniplayer doesn't have a within-a-single-folder shuffle command though, so there's no way to do this...
    - I'd probably have to overhaul the whole project to make a true shuffle work.... but with just a little more work, I could easily have the special baby record start at a random point in the tracklist... Which could be more fun and interesting for the babies!
    - Alternatively, there are other ways to do the whole thing -- I might do some reading about other audio ouput devices that can interface with an ESP32.