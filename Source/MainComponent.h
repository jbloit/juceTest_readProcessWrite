#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
using namespace juce;
class MainComponent  : public juce::AudioAppComponent,
private Thread,
public Timer
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
   
    TextButton actionButton{"read"};
    
    void doNextAction();
    void initFiles();
    /** load file in buffer */
    void loadFile(std::function<void()> doneCallback=nullptr);
    void processBuffer(std::function<void()> doneCallback=nullptr);
    void writeToFile(std::function<void()> doneCallback=nullptr);
    
    
    enum State{
        idle,
        should_load, // for worker thread to pickup
        loading,
        readyToProcess,
        should_process, // for worker thread to pickup
        processing,
        readyToWrite,
        should_write, // for worker thread to pickup
        writing
    };
    State state {idle};
    
    bool uiNeedsRefresh = true;
    
    File audioInputFile;
    File audioOutputFile;
    
    
#pragma mark - thread
    void run() override;
    
#pragma mark - timer
void timerCallback() override;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
