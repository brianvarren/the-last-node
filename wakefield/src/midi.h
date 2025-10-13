#ifndef MIDI_H
#define MIDI_H

#include <RtMidi.h>
#include <vector>
#include <string>

// MIDI message status bytes
constexpr unsigned char MIDI_NOTE_OFF = 0x80;
constexpr unsigned char MIDI_NOTE_ON = 0x90;
constexpr unsigned char MIDI_CONTROL_CHANGE = 0xB0;

// MIDI helper functions
class MidiHandler {
public:
    MidiHandler();
    ~MidiHandler();
    
    // Initialize MIDI input
    bool initialize();
    
    // List available MIDI ports
    void listPorts();
    
    // Open a specific port (or default)
    bool openPort(unsigned int portNumber = 1);
    
    // Get pending messages (call this from audio thread)
    void processMessages(void (*noteOnCallback)(int note, int velocity),
                        void (*noteOffCallback)(int note),
                        void (*ccCallback)(int controller, int value) = nullptr);

    // Add this to the public section of MidiHandler class
    int findPortByName(const std::string& searchString);
    
    // Get current port info
    std::string getCurrentPortName() const;
    int getCurrentPortNumber() const;
    
    // Get port count and names
    int getPortCount();
    std::string getPortName(int port);
    
    // Set UI pointer for console messages
    void setUI(void* uiPtr);
    
private:
    int currentPort;
    RtMidiIn* midiIn;
    static void* uiPointer;  // Static pointer to UI for error callback
    
    // Parse a MIDI message
    void parseMessage(const std::vector<unsigned char>& message,
                     void (*noteOnCallback)(int note, int velocity),
                     void (*noteOffCallback)(int note),
                     void (*ccCallback)(int controller, int value));
    
    // Static error callback for RtMidi - routes to console
    static void midiErrorCallback(RtMidiError::Type type, const std::string& errorText, void* userData);
};

#endif // MIDI_H