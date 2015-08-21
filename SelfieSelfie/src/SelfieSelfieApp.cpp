#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

#include "cinder/gl/Texture.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/Log.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/Shader.h"
#include "cinder/Timer.h"

#include "IntroSequence.h"
#include "SelfieExperience.h"

#include "cinder/MotionManager.h"
#include "cinder/Timeline.h"
#include "asio/asio.hpp"

#if defined(CINDER_ANDROID)
	#include "cinder/android/CinderAndroid.h"
#elsif defined(CINDER_COCOA_TOUCH)
	#include "cinder/cocoa/CinderCocoaTouch.h"
#endif

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace soso;

class SelfieSelfieApp : public App {
public:
	SelfieSelfieApp();

	void setup() override;
	void playIntroAndGetOrientation();
	void determineSizeIndicator();
	void update() override;
	void draw() override;

  void focusGained();
  void focusLost();

	void updateCamera();
	void updateOrientationOffset();
	void showLandscape();

	void touchesBegan(TouchEvent event) override;
	void touchesEnded(TouchEvent event) override;

private:
	IntroSequence											introduction;
	std::unique_ptr<SelfieExperience> selfieExperience;
	std::string												sizeIndicator = "xhdpi";
	ci::Timer													touchTimer;
	uint32_t													touchId = 0;
	bool															doSaveImage = false;
	std::vector<std::future<void>>		saveActions;
	gl::FboRef												readbackFbo;

	void saveComplete();
};

SelfieSelfieApp::SelfieSelfieApp()
{
	#ifdef CINDER_ANDROID
		ci::android::setActivityGainedFocusCallback( [this] { focusGained(); } );
		ci::android::setActivityLostFocusCallback( [this] { focusLost(); } );
	#endif
}

void SelfieSelfieApp::touchesBegan(cinder::app::TouchEvent event)
{
	touchId = event.getTouches().back().getId();
	touchTimer.start();
}

void SelfieSelfieApp::touchesEnded(cinder::app::TouchEvent event)
{
	if (touchId == event.getTouches().back().getId())
	{
		touchTimer.stop();
		if (touchTimer.getSeconds() < 0.16f) {
			doSaveImage = true;
		}
	}
}

void SelfieSelfieApp::setup()
{
  CI_LOG_I("Setting up selfie_x_selfie");

	determineSizeIndicator();
	auto image_path = fs::path("img") / sizeIndicator;
	introduction.setup( image_path );
	introduction.setFinishFn( [this] { showLandscape(); } );

	readbackFbo = gl::Fbo::create(toPixels(getWindowWidth()), toPixels(getWindowHeight()));
}

void SelfieSelfieApp::focusGained()
{
	CI_LOG_I("Focus Gained");
	if( selfieExperience ) {
		selfieExperience->resume();
	}
}

void SelfieSelfieApp::focusLost()
{
	CI_LOG_I("Focus Lost");
	if( selfieExperience ) {
		selfieExperience->pause();
	}
}

void SelfieSelfieApp::determineSizeIndicator()
{
	auto large_side = toPixels( max( getWindowWidth(), getWindowHeight() ) );

	if( large_side <= 1280 ) {
		sizeIndicator = "xhdpi";
	}
	else if( large_side <= 1920 ) {
		sizeIndicator = "xxhdpi";
	}
	else {
		sizeIndicator = "xxxhdpi";
	}

	CI_LOG_I( "Device size: " << large_side << " using images for: " << sizeIndicator );
}

void SelfieSelfieApp::update()
{
	if( selfieExperience )
	{
		selfieExperience->update();
	}
	else
	{
		io_service().post( [this] {
			auto image_path = fs::path("img") / sizeIndicator;
			selfieExperience = unique_ptr<SelfieExperience>( new SelfieExperience( image_path ) );
			introduction.start();
		} );
	}
}

void SelfieSelfieApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) );

	if( selfieExperience )
	{
		selfieExperience->draw();
	}

	if( doSaveImage && saveActions.empty() )
	{
		doSaveImage = false;
		ci::Timer timer(true);
		{
			gl::ScopedFramebuffer fbo( readbackFbo );
			gl::clear( Color( 0, 0, 0 ) );
			selfieExperience->drawScene();
		}

		saveActions.emplace_back( std::async(launch::async, [this, source=readbackFbo->getColorTexture()->createSource()] {
			cocoa::writeToSavedPhotosAlbum(source);
			dispatchAsync( [this] { saveComplete(); } );
		}));
		timer.stop();
		CI_LOG_I( "Save time on main thread: " << timer.getSeconds() * 1000 << "ms" );

	}

	introduction.draw();

	#if DEBUG
		auto err = gl::getError();
		if( err ) {
			CI_LOG_E( "Draw gl error: " << gl::getErrorString(err) );
		}
	#endif
}

void SelfieSelfieApp::saveComplete()
{
	saveActions.clear();
}

void SelfieSelfieApp::showLandscape()
{
	if( ! selfieExperience ) {
		auto image_path = fs::path("img") / sizeIndicator;
		selfieExperience = unique_ptr<SelfieExperience>( new SelfieExperience( image_path ) );
	}
	selfieExperience->showLandscape();
}

inline void prepareSettings( app::App::Settings *iSettings )
{
  iSettings->setMultiTouchEnabled();
	iSettings->setHighDensityDisplayEnabled();
	#ifdef CINDER_ANDROID
		iSettings->setKeepScreenOn(true);
	#endif
}

CINDER_APP( SelfieSelfieApp, RendererGl, &prepareSettings )
