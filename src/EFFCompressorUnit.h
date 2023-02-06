
#pragma once

#include "ofxPDSP.h"


class EFFCompressorUnit  {
public:
    
    
    ofParameter<float> _e_compressor_in_gain;

    ofParameter<float> _e_compressor_in_attack;
    ofParameter<float> _e_compressor_in_knee;
    ofParameter<float> _e_compressor_in_ratio;
    ofParameter<float> _e_compressor_in_release;
    ofParameter<float> _e_compressor_in_threshold;
    ofParameter<float>  _e_compressor_comp_meter;


    
    ofParameterGroup ParamGroup;
    
    bool usesSend = false;
    bool isMono = false;
    
    
    void setup(){
        ParamGroup.add(_e_compressor_in_gain.set("In gain", 1.0, 0.0, 10.0));
        ParamGroup.add(_e_compressor_in_threshold.set("Threshold",0.0f, -50.0f, 0.0f ));
        ParamGroup.add(_e_compressor_comp_meter.set("GR meter", 0.0f, 0.0f, -25.0f) );
        ParamGroup.add(_e_compressor_in_attack.set("Attack (MS)", 10.0f, 1.0f, 60.0f));
        ParamGroup.add(_e_compressor_in_release.set("Release (MS)", 50.0f, 10.0f, 250.0f ));
        ParamGroup.add(_e_compressor_in_knee.set("Knee", 1, 0, 1));
        ParamGroup.add(_e_compressor_in_ratio.set("Ratio", 10, 1, 40));
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
