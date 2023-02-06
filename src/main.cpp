
#include "ofMain.h"
#include "ofApp.h"


#ifndef TARGET_OSX
#ifndef TARGET_WIN32
#define HAS_ADC
#else
#endif
#endif

#ifndef HAS_ADC


//========================================================================
int main( ){
    ofSetupOpenGL(1920,1080, OF_FULLSCREEN);            // <-------- setup the GL context

    // this kicks off the running of my app
    // can be OF_WINDOW or OF_FULLSCREEN
    // pass in width and height too:
    ofRunApp(new ofApp());

}

#else

#include "ofAppNoWindow.h"

int main() {

    auto window = make_shared<ofAppNoWindow>();
    
    ofRunApp(window, make_shared<ofApp>());
    ofRunMainLoop();
}
#endif // !HAS_ADC

