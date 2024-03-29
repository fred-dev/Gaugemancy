
// AudioPlayer.cpp
// Nicola Pisanti, MIT License, 2016

#include "AudioPlayer.h"

int AudioPlayer::number = 0;

void AudioPlayer::patch (){
   
    addModuleOutput("0", fader0 );
    pitchControl >> sampler0.in_pitch();
    
    sampleTrig >> sampler0 >> amp0;
    envGate    >> env      >> amp0.in_mod();
    sampler0 >> amp0 >> fader0;
    faderControl >> dBtoLin  >> fader0.in_mod();
    sampler0.addSample( &sample, 0 );

    smoothControl >> env.in_attack();
    smoothControl >> env.in_release();

    ui.setName( "pdsp player " + ofToString(++number));
    ui.add( faderControl.set("volume", 0, -48, 24) );
    ui.add( loadButton.set( "load", false ));
    ui.add( sampleName.set("sample", "no sample"));
    ui.add( samplePath.set("path", "no path"));
    ui.add( pitchControl.set("pitch", 0, -24, 24));
    ui.add( smoothControl.set("fade ms", 0, 0, 50)); 
    ui.add( bPlay.set("play", false) );
    ui.add( bPause.set("pause", false) );
    ui.add( bStop.set("stop", true) );
    
    loadButton.addListener(this, &AudioPlayer::loadButtonCall );
    samplePath.addListener(this, &AudioPlayer::sampleChangedCall );
    bPlay.addListener(this, &AudioPlayer::onPlay );
    bPause.addListener(this, &AudioPlayer::onPause );
    bStop.addListener(this, &AudioPlayer::onStop );
    
    bSemaphore = true;
    sample.setVerbose(true);
	loaded = false;
}

void AudioPlayer::onPlay( bool & value ) {
    
    if( bSemaphore ) {
        bSemaphore = false;
        if ( bStop ) {
            bPlay = true;
            bStop = false;
            envGate.trigger(1.0f);
            sampleTrig.trigger(1.0f);
            ofLogVerbose() << "[pdsp] player: playing\n";
        } else if( bPause ) {
            ofLogVerbose() << "[pdsp] player: unpaused\n";
            bPlay = true;
            bPause = false;
            envGate.trigger(1.0f);            
        } else {
            bPlay = true;
            sampleTrig.trigger(1.0f);
        }
        bSemaphore = true;
    }

}

void AudioPlayer::onPause( bool & value ) {
    
    if( bSemaphore ) {
        bSemaphore = false;
        if ( bPlay ) {
            bPause = true;
            bPlay = false;
            ofLogVerbose() << "[pdsp] player: paused\n";
            envGate.off();
        } else if( bStop ) {
            bPause = false;
            ofLogVerbose() << "[pdsp] player: impossible to pause on stop";
        } else {
            ofLogVerbose() << "[pdsp] player: unpaused\n";
            bPlay = true;
            bPause = false;
            envGate.trigger(1.0f);        
        }
        bSemaphore = true;
    }
}

void AudioPlayer::onStop( bool & value ) {

    if( bSemaphore ) {
        bSemaphore = false;
        if ( bPlay || bPause ) {
            bStop = true;
            bPlay = false;
            bPause = false;
            ofLogVerbose() << "[pdsp] player: stopped\n";
            envGate.off();          
        }
        bSemaphore = true;
    }
}

void AudioPlayer::loadButtonCall( bool & value ) {
    if(value){

        float fvalue = faderControl.get();
        faderControl.set(0.0f);
        
        //Open the Open File Dialog
        ofFileDialogResult openFileResult= ofSystemLoadDialog("select an audio sample"); 
        
        //Check if the user opened a file
        if (openFileResult.bSuccess){
            
            string path = openFileResult.getPath();
            
            samplePath = path;
        
            ofLogVerbose("file loaded");
			loaded = true;
            
        }else {
            ofLogVerbose("User hit cancel");
			loaded = false;
        }

        // switch to mono if the sample has just one channel
            sampler0.setSample( &sample, 0, 0 );
       
        loadButton = false;
        
        faderControl.set(fvalue);
        
        bool dummy = true;
        onStop( dummy );
    }
}

void AudioPlayer::sampleChangedCall( string & value ) {
    
    ofLogVerbose( "loading" + value );
    loadSample( samplePath );    

    auto v = ofSplitString(samplePath, "/" );
    sampleName = v[v.size()-1];
}
    
void AudioPlayer::loadSample( string path ) {
    sample.load( path );
}

void AudioPlayer::load( string path ) {
    samplePath = path;
	loaded = true;
}

void AudioPlayer::play() {
    bPlay = bPlay ? false : true;
}

void AudioPlayer::pause(bool pause){
    bPause = pause;
}

void AudioPlayer::stop() {
    bStop = bStop ? false : true;
}

float AudioPlayer::getPosition()
{
	return sampler0.meter_position();
}
	
bool AudioPlayer::isLoaded()
{
	return loaded;
}

void AudioPlayer::setVolume(float vol)
{
	if (loaded)
	{
		faderControl.set(vol);
	}
}

void AudioPlayer::setPosition(float pos)
{
	pos >> sampler0.in_start();
}

bool AudioPlayer::getIsPaused()
{
	return bPause;
}

