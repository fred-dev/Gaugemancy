
#pragma once

#include "ofxPDSP.h"


class EFFBitCrushUnit  {
public:
    
 
    ofParameter<int> _e_bitcrush_in_bits;
    ofParameter<int> _e_bitcrush_in_bitsMin;
    ofParameter<int> _e_bitcrush_in_bitsMax;
    ofParameter<int> _e_bitcrush_in_bitsConnectTo;
    ofParameterGroup ParamGroup;
    bool usesSend =true;
    bool isMono = true;
    
    
    void setup(){
        ParamGroup.add(_e_bitcrush_in_bits.set("Bits", 16, 1, 16));
        ParamGroup.add(_e_bitcrush_in_bitsMin.set("Bits Min", 1, 1, 16));
        ParamGroup.add(_e_bitcrush_in_bitsMax.set("Bits Max", 16, 1, 16));
        ParamGroup.add(_e_bitcrush_in_bitsConnectTo.set("Bits Connect To", 0, 0, 6));
    }
    
    ofParameterGroup getParamGroup(){
        return ParamGroup;
    }
    
    void DoClear(){
        ParamGroup.clear();
    }
    
    void setParameterGroupName(std::string name){
        ParamGroup.setName(name);
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
