
#pragma once

#include "ofxPDSP.h"


class EFFDelayUnit  {
public:
    

    
    ofParameter<float> _e_delay_in_send;
    ofParameter<float> _e_delay_in_sendMin;
    ofParameter<float> _e_delay_in_sendMax;
    ofParameter<int> _e_delay_in_sendConnectTo;
    
    ofParameter<float> _e_delay_in_time;
    ofParameter<float> _e_delay_in_timeMin;
    ofParameter<float> _e_delay_in_timeMax;
    ofParameter<int> _e_delay_in_timeConnectTo;
    
    ofParameter<float> _e_delay_in_damping;
    ofParameter<float> _e_delay_in_dampingMin;
    ofParameter<float> _e_delay_in_dampingMax;
    ofParameter<int> _e_delay_in_dampingConnectTo;
    
    ofParameter<float> _e_delay_in_feedback;
    ofParameter<float> _e_delay_in_feedbackMin;
    ofParameter<float> _e_delay_in_feedbackMax;
    ofParameter<int> _e_delay_in_feedbackConnectTo;
    

    
    ofParameterGroup ParamGroup;
    bool usesSend =true;
    bool isMono = true;
    
    void setup(){
        ParamGroup.add(_e_delay_in_send.set("Send ", 0.0, 0, 1));
        ParamGroup.add(_e_delay_in_sendMin.set("Send Min", 0.0, 0, 1));
        ParamGroup.add(_e_delay_in_sendMax.set("Send Max", 1.0, 0, 1));
        ParamGroup.add(_e_delay_in_sendConnectTo.set("Send Connect To", 0, 0, 6));
        
        ParamGroup.add(_e_delay_in_time.set("Time ", 0.0, 0.0, 400));
        ParamGroup.add(_e_delay_in_timeMin.set("Time Min", 0.0, 0.0, 400));
        ParamGroup.add(_e_delay_in_timeMax.set("Time Max", 400, 0.0, 400));
        ParamGroup.add(_e_delay_in_timeConnectTo.set("Time Connect To", 0, 0, 6));
        
        ParamGroup.add(_e_delay_in_damping.set("Damping ", 0.0, 0, 1));
        ParamGroup.add(_e_delay_in_dampingMin.set("Damping Min", 0.0, 0, 1));
        ParamGroup.add(_e_delay_in_dampingMax.set("Damping Max", 1.0, 0, 1));
        ParamGroup.add(_e_delay_in_dampingConnectTo.set("Damping Connect To", 0, 0, 6));
        
        ParamGroup.add(_e_delay_in_feedback.set("feedback ", 0.0, 0, 1));
        ParamGroup.add(_e_delay_in_feedbackMin.set("feedback Min", 0.0, 0, 1));
        ParamGroup.add(_e_delay_in_feedbackMax.set("feedback Max", 1.0, 0, 1));
        ParamGroup.add(_e_delay_in_feedbackConnectTo.set("feedback Connect To", 0, 0, 6));
        
    }
    
    ofParameterGroup getParamGroup(){
        
        return ParamGroup;
    }
    void DoClear(){
        ParamGroup.clear();
    }
    bool getIsMono(){
        return isMono;
    }
    
    void setParameterGroupName(std::string name){
        ParamGroup.setName(name);
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
