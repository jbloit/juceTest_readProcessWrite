#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent(): Thread("background")
{

    addAndMakeVisible(actionButton);
    actionButton.onClick = [this]{doNextAction();};

    formatManager.registerBasicFormats();
    initFiles();
    startTimer(100); // UI udates
    startThread();  // background worker
    
    setSize (800, 600);

    // Some platforms require permissions to open input channels so request that here
    if (juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
        && ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
                                           [&] (bool granted) { setAudioChannels (granted ? 2 : 0, 2); });
    }
    else
    {
        // Specify the number of input and output channels that we want to open
        setAudioChannels (2, 2);
    }
}

MainComponent::~MainComponent()
{
    stopTimer();
    stopThread(4000);
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // This function will be called when the audio device is started, or when
    // its settings (i.e. sample rate, block size, etc) are changed.

    // You can use this function to initialise any resources you might need,
    // but be careful - it will be called on the audio thread, not the GUI thread.

    // For more details, see the help for AudioProcessor::prepareToPlay()
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    // Your audio-processing code goes here!

    // For more details, see the help for AudioProcessor::getNextAudioBlock()

    // Right now we are not producing any data, in which case we need to clear the buffer
    // (to prevent the output of random noise)
    bufferToFill.clearActiveBufferRegion();
}

void MainComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.

    // For more details, see the help for AudioProcessor::releaseResources()
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    actionButton.setBounds(area.reduced(getWidth()/4));
    
}


#pragma mark - helpers

void MainComponent::doNextAction()
{
    DBG("do next");
    
    switch (state) {
        case idle:
            DBG("should load file");
            state = should_load;
            break;
            
        case readyToProcess:
            DBG("should process file");
            state = should_process;
            break;
            
        case readyToWrite:
            DBG("should write buffer to file");
            state = should_write;
            break;
            
        default:
            break;
    }
    
    
}

void MainComponent::initFiles()
{
    auto documentsDirectory = File::getSpecialLocation (File::userDocumentsDirectory);
    audioInputFile = documentsDirectory.getChildFile("sounds").getChildFile("input.wav");
    audioOutputFile = documentsDirectory.getChildFile("sounds").getChildFile("processed.wav");
    
    jassert(audioInputFile.exists());
}

#pragma mark - workers

void MainComponent::loadFile(std::function<void()> doneCallback)
{
    DBG("loading file now");
    
    FileInputSource inputSource(audioInputFile, false);
    
    reader.reset(formatManager.createReaderFor(audioInputFile));
    
    
    if (reader != nullptr)
    {
        sampleRate = reader->sampleRate;
        auto duration = reader->lengthInSamples / reader->sampleRate;
        
        
        if (duration > 0)
        {
            buffer.reset(new AudioSampleBuffer((int) reader->numChannels, (int) reader->lengthInSamples));
            reader->read(buffer.get(), 0, (int) reader->lengthInSamples, 0, true, true);
        }
    }
    
    DBG("done loading file, with lengthInSamples " << reader->lengthInSamples );
    if (doneCallback != nullptr)
    {
        doneCallback();
    }
}

void MainComponent::processBuffer(std::function<void()> doneCallback)
{
    DBG("processing buffer now");
    
    if (buffer != nullptr)
    {
        buffer.get()->reverse(0, buffer.get()->getNumSamples());
    }
    
    DBG("done processing");
    if (doneCallback != nullptr)
    {
        doneCallback();
    }
}

void MainComponent::writeToFile(std::function<void()> doneCallback)
{
    DBG("writing to file now");
    
    audioOutputFile.deleteFile();
    
        if (buffer != nullptr)
        {
            if (buffer.get()->getNumSamples() > 0)
            {
                auto* outputTo = audioOutputFile.createOutputStream().release();
                if (outputTo->openedOk())
                {
                    WavAudioFormat wavFormat;
                    writer.reset(wavFormat.createWriterFor(outputTo, sampleRate, 2, 16, {}, 0));
                    
                    if (writer != nullptr)
                    {
                        writer.get()->writeFromAudioSampleBuffer(*buffer.get(), 0, buffer.get()->getNumSamples());
                        writer->flush();
                    }
                }

            }
        }

    DBG("done writing file");
    if (doneCallback != nullptr)
    {
        doneCallback();
    }
}

#pragma mark - thread
void MainComponent::run()
{
    
    while (!threadShouldExit())
    {
    
        switch (state) {
            case should_load:
                state = loading;
                uiNeedsRefresh = true;
                loadFile([this]{
                    state = readyToProcess;
                    uiNeedsRefresh = true;
                });
                break;
                
            case should_process:
                state = processing;
                uiNeedsRefresh = true;
                processBuffer([this]{
                    state = readyToWrite;
                    uiNeedsRefresh = true;
                });
                break;
                
            case should_write:
                state = writing;
                uiNeedsRefresh = true;
                writeToFile([this]{
                    state = idle;
                    uiNeedsRefresh = true;
                });
                break;
                
            default:
//                DBG("default");
                break;
        }
        
        wait(10);
    }
}


#pragma mark - timer
void MainComponent::timerCallback()
{
    if (uiNeedsRefresh)
    {
        switch (state) {
            case idle:
                actionButton.setButtonText("read");
                break;
                
            case loading:
                actionButton.setButtonText("loading...");
                break;
                
            case readyToProcess:
                actionButton.setButtonText("do process");
                break;
                
            case processing:
                actionButton.setButtonText("processing...");
                break;
                
            case readyToWrite:
                actionButton.setButtonText("write to file");
                break;
                
            case writing:
                actionButton.setButtonText("writing...");
                break;

            default:
                break;
        }
        uiNeedsRefresh = false;
    }
}
