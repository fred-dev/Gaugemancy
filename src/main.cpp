
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
	ofGLWindowSettings settings;
	settings.setGLVersion(3, 2);
	settings.setSize(1920, 1080);
	settings.windowMode = OF_FULLSCREEN;
	auto window = ofCreateWindow(settings);
	ofRunApp(new ofApp());
	ofRunMainLoop();

}

#else

#include "ofAppNoWindow.h"

int main() {

    auto window = make_shared<ofAppNoWindow>();
    
    ofRunApp(window, make_shared<ofApp>());
    ofRunMainLoop();
}
#endif // !HAS_ADC

