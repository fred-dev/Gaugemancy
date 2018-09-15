
#pragma once

#include "ofxPDSP.h"


class EFFChorusUnit  {
public:
    


    ofParameter<float> _e_chorus_in_depth;
    ofParameter<float> _e_chorus_in_depthMin;
    ofParameter<float> _e_chorus_in_depthMax;
    ofParameter<int> _e_chorus_in_depthConnectTo;
    
    ofParameter<float> _e_chorus_in_speed;
    ofParameter<float> _e_chorus_in_speedMin;
    ofParameter<float> _e_chorus_in_speedMax;
    ofParameter<int> _e_chorus_in_speedConnectTo;
    
    ofParameter<float> _e_chorus_in_delay;
    ofParameter<float> _e_chorus_in_delayMin;
    ofParameter<float> _e_chorus_in_delayMax;
    ofParameter<int> _e_chorus_in_delayConnectTo;

    
    ofParameterGroup ParamGroup;
    
    bool usesSend = false;
    bool isMono = false;
    
    
    void setup(){
        ParamGroup.add(_e_chorus_in_depth.set("Depth", 0, 0, 10));
        ParamGroup.add(_e_chorus_in_depthMin.set("Depth Min", 0, 0, 10));
        ParamGroup.add(_e_chorus_in_depthMax.set("Depth Max", 10, 0, 10));
        ParamGroup.add(_e_chorus_in_depthConnectTo.set("Depth connect to", 0, 0, 6));
        
        ParamGroup.add(_e_chorus_in_speed.set("Speed", 0, 0, 200));
        ParamGroup.add(_e_chorus_in_speedMin.set("Speed Min", 0, 0, 200));
        ParamGroup.add(_e_chorus_in_speedMax.set("Speed Max", 200, 0, 1));
        ParamGroup.add(_e_chorus_in_speedConnectTo.set("Speed connect to", 0, 0, 6));
        
        ParamGroup.add(_e_chorus_in_delay.set("Delay", 0, 0, 200));
        ParamGroup.add(_e_chorus_in_delayMin.set("Delay Min", 0, 0, 200));
        ParamGroup.add(_e_chorus_in_delayMax.set("Delay Max", 200, 0, 200));
        ParamGroup.add(_e_chorus_in_delayConnectTo.set("Delay connect to", 0, 0, 6));
    }
    
    void setParameterGroupName(std::string name){
        ParamGroup.setName(name);
    }
    ofParameterGroup getParamGroup(){
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
