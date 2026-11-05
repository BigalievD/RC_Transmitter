#include "FailsafeProcessor.h"
#include "OutputProcessor.h"

FailsafeProcessor failsafeProcessor;

void FailsafeProcessor::reset(){
    for(uint8_t ch=0;ch<MAX_MODEL_CHANNELS;ch++){
        channels[ch].mode=FailsafeMode::HOLD;
        channels[ch].valueUs=1500;
    }
}

void FailsafeProcessor::process(const ModelConfig &model){
    uint8_t channelCount=constrain(model.channelCount,0,MAX_MODEL_CHANNELS);
    for(uint8_t ch=0;ch<channelCount;ch++){
        const OutputConfig &config=model.outputs[ch];
        channels[ch].mode=config.failsafeMode;
        switch(config.failsafeMode){
            case FailsafeMode::HOLD:
                channels[ch].valueUs=1500;
                break;
            case FailsafeMode::CUSTOM:
                channels[ch].valueUs=outputProcessor.calculateUs(config.failsafeValue,config);
                break;
            case FailsafeMode::NOPULSE:
                channels[ch].valueUs=1500;
                break;
        }
    }
    for(uint8_t ch=channelCount;ch<MAX_MODEL_CHANNELS;ch++){
        channels[ch].mode=FailsafeMode::HOLD;
        channels[ch].valueUs=1500;
    }
}

FailsafeMode FailsafeProcessor::getMode(uint8_t channel) const{
    if(channel>=MAX_MODEL_CHANNELS) return FailsafeMode::HOLD;
    return channels[channel].mode;
}

int16_t FailsafeProcessor::getValueUs(uint8_t channel) const{
    if(channel>=MAX_MODEL_CHANNELS) return 1500;
    return channels[channel].valueUs;
}

const FailsafeChannelState &FailsafeProcessor::getChannel(uint8_t channel) const{
    static const FailsafeChannelState invalidChannel{};
    if(channel>=MAX_MODEL_CHANNELS) return invalidChannel;
    return channels[channel];
}

const FailsafeChannelState *FailsafeProcessor::getChannels() const{
    return channels;
}