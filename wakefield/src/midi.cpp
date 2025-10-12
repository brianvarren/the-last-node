#include "midi.h"
#include <iostream>
#include <algorithm>
#include <cctype>

MidiHandler::MidiHandler() : midiIn(nullptr) {
}

MidiHandler::~MidiHandler() {
    if (midiIn) {
        delete midiIn;
    }
}

bool MidiHandler::initialize() {
    try {
        midiIn = new RtMidiIn();
        return true;
    } catch (RtMidiError& error) {
        std::cerr << "MIDI Error: " << error.getMessage() << std::endl;
        return false;
    }
}

void MidiHandler::listPorts() {
    if (!midiIn) return;
    
    unsigned int portCount = midiIn->getPortCount();
    std::cout << "\nFound " << portCount << " MIDI input port(s):\n";
    
    for (unsigned int i = 0; i < portCount; i++) {
        try {
            std::string portName = midiIn->getPortName(i);
            std::cout << "  Port " << i << ": " << portName << "\n";
        } catch (RtMidiError& error) {
            std::cerr << "Error getting port name: " << error.getMessage() << std::endl;
        }
    }
    std::cout << std::endl;
}

bool MidiHandler::openPort(unsigned int portNumber) {
    if (!midiIn) return false;
    
    try {
        unsigned int portCount = midiIn->getPortCount();
        
        if (portCount == 0) {
            std::cout << "No MIDI input ports available.\n";
            std::cout << "Synth will run in drone mode (no MIDI control).\n";
            return false;
        }
        
        if (portNumber >= portCount) {
            std::cerr << "Invalid port number. Using port 0.\n";
            portNumber = 0;
        }
        
        midiIn->openPort(portNumber);
        
        // Don't ignore any message types
        midiIn->ignoreTypes(false, false, false);
        
        std::string portName = midiIn->getPortName(portNumber);
        std::cout << "Opened MIDI port: " << portName << "\n";
        
        return true;
        
    } catch (RtMidiError& error) {
        std::cerr << "Error opening MIDI port: " << error.getMessage() << std::endl;
        return false;
    }
}

void MidiHandler::parseMessage(const std::vector<unsigned char>& message,
                               void (*noteOnCallback)(int note, int velocity),
                               void (*noteOffCallback)(int note),
                               void (*ccCallback)(int controller, int value)) {
    
    if (message.size() < 1) return;
    
    unsigned char status = message[0] & 0xF0;  // Upper nibble is message type
    unsigned char channel = message[0] & 0x0F;  // Lower nibble is channel
    
    // Note On
    if (status == MIDI_NOTE_ON && message.size() >= 3) {
        unsigned char note = message[1];
        unsigned char velocity = message[2];
        
        // Note: MIDI Note On with velocity 0 is actually a Note Off
        if (velocity == 0) {
            std::cout << "Note Off: " << (int)note << " (ch " << (int)channel << ")\n";
            if (noteOffCallback) {
                noteOffCallback(note);
            }
        } else {
            std::cout << "Note On:  " << (int)note << " velocity " << (int)velocity 
                     << " (ch " << (int)channel << ")\n";
            if (noteOnCallback) {
                noteOnCallback(note, velocity);
            }
        }
    }
    // Note Off
    else if (status == MIDI_NOTE_OFF && message.size() >= 3) {
        unsigned char note = message[1];
        std::cout << "Note Off: " << (int)note << " (ch " << (int)channel << ")\n";
        if (noteOffCallback) {
            noteOffCallback(note);
        }
    }
    // Control Change
    else if (status == MIDI_CONTROL_CHANGE && message.size() >= 3) {
        unsigned char controller = message[1];
        unsigned char value = message[2];
        if (ccCallback) {
            ccCallback(controller, value);
        }
    }
}

void MidiHandler::processMessages(void (*noteOnCallback)(int note, int velocity),
                                 void (*noteOffCallback)(int note),
                                 void (*ccCallback)(int controller, int value)) {
    if (!midiIn) return;
    
    std::vector<unsigned char> message;
    double stamp;
    
    // Process all pending MIDI messages
    while (true) {
        stamp = midiIn->getMessage(&message);
        
        if (message.empty()) {
            break;  // No more messages
        }
        
        parseMessage(message, noteOnCallback, noteOffCallback, ccCallback);
    }
}

int MidiHandler::findPortByName(const std::string& searchString) {
    if (!midiIn) return -1;
    
    unsigned int portCount = midiIn->getPortCount();
    
    for (unsigned int i = 0; i < portCount; i++) {
        try {
            std::string portName = midiIn->getPortName(i);
            // Case-insensitive search
            std::string portNameLower = portName;
            std::string searchLower = searchString;
            
            // Convert to lowercase for comparison
            std::transform(portNameLower.begin(), portNameLower.end(), 
                          portNameLower.begin(), ::tolower);
            std::transform(searchLower.begin(), searchLower.end(), 
                          searchLower.begin(), ::tolower);
            
            if (portNameLower.find(searchLower) != std::string::npos) {
                return i;
            }
        } catch (RtMidiError& error) {
            continue;
        }
    }
    
    return -1;  // Not found
}