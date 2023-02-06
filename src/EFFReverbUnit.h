
#pragma once

#include "ofxPDSP.h"


class EFFReverbUnit  {
public:
    

    ofParameter<float> _e_reverb_in_mix;
    ofParameter<float> _e_reverb_in_mixMin;
    ofParameter<float> _e_reverb_in_mixMax;
    ofParameter<int> _e_reverb_in_mixConnectTo;
    
    ofParameter<float> _e_reverb_in_damping;
    ofParameter<float> _e_reverb_in_dampingMin;
    ofParameter<float> _e_reverb_in_dampingMax;
    ofParameter<int> _e_reverb_in_dampingConnectTo;
    
    ofParameter<float> _e_reverb_in_density;
    ofParameter<float> _e_reverb_in_densityMin;
    ofParameter<float> _e_reverb_in_densityMax;
    ofParameter<int> _e_reverb_in_densityConnectTo;

    ofParameter<float> _e_reverb_in_hiCut;
    ofParameter<float> _e_reverb_in_hiCutMin;
    ofParameter<float> _e_reverb_in_hiCutMax;
    ofParameter<int> _e_reverb_in_hiCutConnectTo;

    ofParameter<float> _e_reverb_in_modAmount;
    ofParameter<float> _e_reverb_in_modAmountMin;
    ofParameter<float> _e_reverb_in_modAmountMax;
    ofParameter<int> _e_reverb_in_modAmountConnectTo;

    ofParameter<float> _e_reverb_in_modFreq;
    ofParameter<float> _e_reverb_in_modFreqMin;
    ofParameter<float> _e_reverb_in_modFreqMax;
    ofParameter<int> _e_reverb_in_modFreqConnectTo;

    ofParameter<float> _e_reverb_in_time;
    ofParameter<float> _e_reverb_in_timeMin;
    ofParameter<float> _e_reverb_in_timeMax;
    ofParameter<int> _e_reverb_in_timeConnectTo;

    ofParameterGroup ParamGroup;
    bool usesSend =true;
    bool isMono = false;
    
    void setup(){
        ParamGroup.add(_e_reverb_in_mix.set("Send ", 0.0, 0, 1));
        ParamGroup.add(_e_reverb_in_mixMin.set("Send Min", 0.0, 0, 1));
        ParamGroup.add(_e_reverb_in_mixMax.set("Send Max", 1, 0, 1));
        ParamGroup.add(_e_reverb_in_mixConnectTo.set("Send Connect To", 0, 1, 6));
        
        ParamGroup.add(_e_reverb_in_time.set("Time ", 0.0, 0.0, 20));
        ParamGroup.add(_e_reverb_in_timeMin.set("Time Min", 0.0, 0.0, 20));
        ParamGroup.add(_e_reverb_in_timeMax.set("Time Max", 20, 0.0, 20));
        ParamGroup.add(_e_reverb_in_timeConnectTo.set("Time Connect To", 0, 1, 6));

        ParamGroup.add(_e_reverb_in_damping.set("Damping ", 0.0, 0, 1));
        ParamGroup.add(_e_reverb_in_dampingMin.set("Damping Min", 0.0, 0, 1));
        ParamGroup.add(_e_reverb_in_dampingMax.set("Damping Max", 1, 0, 1));
        ParamGroup.add(_e_reverb_in_dampingConnectTo.set("Damping Connect To", 0, 1, 6));

        ParamGroup.add(_e_reverb_in_density.set("Density ", 0.0, 0, 1));
        ParamGroup.add(_e_reverb_in_densityMin.set("Density Min", 0.0, 0, 1));
        ParamGroup.add(_e_reverb_in_densityMax.set("Density Max", 1, 0, 1));
        ParamGroup.add(_e_reverb_in_densityConnectTo.set("Density Connect To", 0, 1, 6));

        ParamGroup.add(_e_reverb_in_hiCut.set("Hi cut ", 0.0, 0, 20000));
        ParamGroup.add(_e_reverb_in_hiCutMin.set("Hi cut Min", 0.0, 0, 20000));
        ParamGroup.add(_e_reverb_in_hiCutMax.set("Hi cut Max", 20000, 0, 20000));
        ParamGroup.add(_e_reverb_in_hiCutConnectTo.set("Hi cut Connect To", 0, 1, 6));

        ParamGroup.add(_e_reverb_in_modFreq.set("Mod freq ", 0.0, 0, 20000));
        ParamGroup.add(_e_reverb_in_modFreqMin.set("Mod freq Min", 0.0, 0, 20000));
        ParamGroup.add(_e_reverb_in_modFreqMax.set("Mod freq Max", 20000, 0, 20000));
        ParamGroup.add(_e_reverb_in_modFreqConnectTo.set("Mod freq Connect To", 0.0, 0, 20000));

        ParamGroup.add(_e_reverb_in_modAmount.set("Mod amount ", 0.0, 0, 1));
        ParamGroup.add(_e_reverb_in_modAmountMin.set("Mod amount Min", 0.0, 0, 1));
        ParamGroup.add(_e_reverb_in_modAmountMax.set("Mod amount Max", 1, 0, 1));
        ParamGroup.add(_e_reverb_in_modAmountConnectTo.set("Mod amount Connect To",0, 1, 6));

    }
    
    ofParameterGroup getParamGroup(){
        
        return ParamGroup;
    }
    
    bool getIsMono(){
        return isMono;
    }
    
    void setParameterGroupName(std::string name){
        ParamGroup.setName(name);
    }
    
    void DoClear(){
        ParamGroup.clear();
    }
    bool getUsesSend(){
        return usesSend;
    }
    
    void exit() {
        
        ofLogVerbose()<<"MyTestObject::exit() - goodbye!\n"<<endl;
    }
    

    
    void draw() {
        
        
        
    }
    
    
};
