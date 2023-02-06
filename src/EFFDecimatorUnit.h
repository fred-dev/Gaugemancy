
#pragma once

#include "ofxPDSP.h"


class EFFDecimatorUnit  {
public:

    ofParameter<int> _e_decomator_in_rate;
    ofParameter<int> _e_decomator_in_rateMin;
    ofParameter<int> _e_decomator_in_rateMax;
    ofParameter<int> _e_decomator_in_rateConnectTo;
    ofParameterGroup ParamGroup;
    bool usesSend = false;
    bool isMono = true;
    
    
    void setup(){
        ParamGroup.add(_e_decomator_in_rate.set("Rate", 44100, 1, 44100));
        ParamGroup.add(_e_decomator_in_rateMin.set("Rate Min", 1, 1, 44100));
        ParamGroup.add(_e_decomator_in_rateMax.set("Rate Max", 44100, 1, 44100));
        ParamGroup.add(_e_decomator_in_rateConnectTo.set("Rate Connect To", 0, 0, 6));
    }
    
    ofParameterGroup getParamGroup(){
        return ParamGroup;
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
    
    bool getIsMono(){
        return isMono;
    }
    
    void exit() {
        
    }
    
    
 
    
    
    
};
