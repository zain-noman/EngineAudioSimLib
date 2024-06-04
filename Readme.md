# Simulating Engine Audio From Scratch

To get started with this repository i suggest reading the [Medium Article](https://medium.com/@zaina526/implementing-engine-audio-simulation-from-scratch-998490d1f4b0) first

## Repository Structure
- esp-idf implementation
  - Contains code for running engine sound simulation on ESP32 microcontrollers
  - its currently a bit outdated so be warned
- pulse_audio_implementation
  - implementation that works on linux using pulse audio
  - this is the updated version referenced in the article

## Running the Linux Implementation
Dependencies
- Cmake
- libpulse-dev
- libwave (used as a git submodule)
- build-essential (Make, gcc, etc)

Once the dependancies are installed run the following commands
    
    cd pulse_audio_implementation
    mkdir build
    cd build
    cmake ..
    make
    ./engineSound