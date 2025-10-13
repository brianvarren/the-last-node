#include "midi.h"
#include <iostream>
#include <algorithm>
#include <cctype>

MidiHandler::MidiHandler() : midiIn(nullptr), currentPort(-1) {
}

MidiHandler::~MidiHandler() {
    if (midiIn) {
        delete midiIn;
    }
}

// Static error callback to suppress MIDI error messages
void MidiHandler::midiErrorCallback(RtMidiError::Type type, const std::string& errorText, void* userData) {
    // Suppress the messages - they were causing UI to scroll
    // Optionally, we could log critical errors only
    if (type == RtMidiError::WARNING) {
        // Ignore warnings like "message queue limit reached"
        return;
    }
    // For other errors, silently ignore or could add to console if needed
}

bool MidiHandler::initialize() {
    try {
        // Create RtMidiIn with error callback to suppress messages
        midiIn = new RtMidiIn(RtMidi::UNSPECIFIED, "WakefieldSynth", 100);
        
        // Set error callback to suppress error messages
        midiIn->setErrorCallback(&MidiHandler::midiErrorCallback, nullptr);
        
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
        
        // Store current port number
        currentPort = portNumber;
        
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
            if (noteOffCallback) {
                noteOffCallback(note);
            }
        } else {
            if (noteOnCallback) {
                noteOnCallback(note, velocity);
            }
        }
    }
    // Note Off
    else if (status == MIDI_NOTE_OFF && message.size() >= 3) {
        unsigned char note = message[1];
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

std::string MidiHandler::getCurrentPortName() const {
    if (!midiIn || currentPort < 0) {
        return "Not connected";
    }
    
    try {
        return midiIn->getPortName(currentPort);
    } catch (RtMidiError& error) {
        return "Error reading port name";
    }
}

int MidiHandler::getCurrentPortNumber() const {
    return currentPort;
}