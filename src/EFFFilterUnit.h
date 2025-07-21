
#pragma once

#include "ofxPDSP.h"


class EFFFilterUnit  : public EffectBase{
public:

    ofParameter<int> _e_MLAD_in_freq;
    ofParameter<int> _e_MLAD_in_freqMin;
    ofParameter<int> _e_MLAD_in_freqMax;
    ofParameter<int> _e_MLAD_in_freqConnectTo;
    
    ofParameter<float> _e_MLAD_in_reso;
    ofParameter<float> _e_MLAD_in_resoMin;
    ofParameter<float> _e_MLAD_in_resoMax;
    ofParameter<int> _e_MLAD_in_resoConnectTo;
    
    ofParameterGroup ParamGroup;
    
    bool usesSend = false;
    bool isMono = true;
    
    
    void setup(){
        ParamGroup.add(_e_MLAD_in_freq.set("Freq", 20000, 1, 20000));
        ParamGroup.add(_e_MLAD_in_freqMin.set("Freq Min", 1, 1, 20000));
        ParamGroup.add(_e_MLAD_in_freqMax.set("Freq Max", 20000, 1, 20000));
        ParamGroup.add(_e_MLAD_in_freqConnectTo.set("Freq connect to", 0, 0, 6));
        
        ParamGroup.add(_e_MLAD_in_reso.set("Reso", 0, 0, 1));
        ParamGroup.add(_e_MLAD_in_resoMin.set("Reso Min", 0, 0, 1));
        ParamGroup.add(_e_MLAD_in_resoMax.set("Reso Max", 1, 0, 1));
        ParamGroup.add(_e_MLAD_in_resoConnectTo.set("Reso connect to", 0, 0, 6));
    }
    
    void update(const ofParameter<float>& sensorValue) override {
        // Update frequency
        float freq = sensorValue * (_e_MLAD_in_freqMax - _e_MLAD_in_freqMin) + _e_MLAD_in_freqMin;
        _e_MLAD_in_freq.set(freq);

        // Update resonance
        float reso = sensorValue * (_e_MLAD_in_resoMax - _e_MLAD_in_resoMin) + _e_MLAD_in_resoMin;
        _e_MLAD_in_reso.set(reso);
    }
    
    void setParameterGroupName(std::string name){
        ParamGroup.setName(name);
    }
    ofParameterGroup& getParamGroup() {
        return ParamGroup;
    }
    
    void DoClear(){
        ParamGroup.clear();
    }
    
    bool getUsesSend(){
        return usesSend;
    }
    
    bool getIsMono(){
        return isMono;
    }
    
    void exit() {
        
    }
    

};
