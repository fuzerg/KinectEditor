#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
	margin = 10;
	panelWidth = OFX_UI_GLOBAL_CANVAS_WIDTH;

	ofSetWindowTitle("MAIN WINDOW");

	ofSetFrameRate(30);
	roundClock = 0;

	//////////////////////////////////////////////////////////////////////////////
	device_ = new ofxKinect2::Device();
	device_->setup();
	device_->setDepthColorSyncEnabled();

	if(depth_.setup(*device_))
	{
		depth_.open();
	}

	if (color_.setup(*device_))
	{
		color_.open();
	}

	if (ir_.setup(*device_))
	{
		ir_.open();
	}
	depthProcessor.init(512, 424, 1920, 1080, device_->getMapper());
	//////////////////////////////////////////////////////////////////////////////

	gui = new ofxUICanvas();
	gui->addLabel("Tools", OFX_UI_FONT_LARGE);
	gui->addSpacer();

	gui->addToggle("live", true);
	gui->addToggle("write mesh", false);
	gui->addToggle("record", false);
	gui->addLabelButton("show point cloud", false);

	gui->addLabelButton("save sequence", false);
	gui->addLabelButton("load sequence", false);
	gui->addLabelButton("clear sequence", false);

	gui->addSpacer();
	gui->addLabelButton("play", false);
	gui->addLabelButton("previous", false);
	gui->addLabelButton("next", false);
	gui->addIntSlider("frame", 0, 0, 0);

	gui->autoSizeToFitWidgets();
	ofAddListener(gui->newGUIEvent,this,&ofApp::guiEvent);

	stop = true;

	setParameter();

	//////////////////////////////////////////////////////////////////////////////
	prefix.assign("frames/");
	img.loadImage("linzer.png");
	img_color = new ofImage();
	img_depth = new ofShortImage();
	img_color->allocate(img.getWidth(), img.getHeight(), OF_IMAGE_COLOR);
	img_depth->allocate(img.getWidth(), img.getHeight(), OF_IMAGE_GRAYSCALE);
	for (int i = 0; i < img.getWidth(); ++ i)
	{
		for (int j = 0; j < img.getHeight(); ++ j)
		{
			img_depth->setColor(i, j, ofColor((float)img.getColor(i, j).a));
			img_color->setColor(i, j, img.getColor(i, j));
		}
	}
	img_color->reloadTexture();
	img_depth->reloadTexture();

	glEnable(GL_POINT_SMOOTH); // use circular points instead of square points
	//glPointSize(3); // make the points bigger

	queue_color = nullptr;
	queue_depth = nullptr;

}

void ofApp::guiEvent(ofxUIEventArgs &e)
{
	string name = e.widget->getName(); 
	int kind = e.widget->getKind(); 

	ofxUIToggle* live = (ofxUIToggle*)gui->getWidget("live");
	ofxUIToggle* record = (ofxUIToggle*)gui->getWidget("record");
	ofxUIIntSlider * frame = (ofxUIIntSlider *) gui->getWidget("frame");
	ofxUIToggle* writeMesh = (ofxUIToggle*)gui->getWidget("write mesh");

	if (name == "play")
	{
		ofxUILabelButton * button = (ofxUILabelButton *) e.widget;
		if (button->getValue())
		{
			button->setName("stop");
			stop = false;
		}
	}
	else if (name == "stop")
	{
		ofxUILabelButton * button = (ofxUILabelButton *) e.widget;
		if (button->getValue())
		{
			button->setName("play");
			stop = true;
		}
	}
	else if (name == "show point cloud")
	{
		ofxUILabelButton * button = (ofxUILabelButton *) e.widget;
		if (button->getValue())
			setMesh();
	}
	else if (name == "save sequence")
	{
		ofxUILabelButton * button = (ofxUILabelButton *) e.widget;
		if (button->getValue())
		{
		}
	}
	else if (name == "load sequence")
	{
		ofxUILabelButton * button = (ofxUILabelButton *) e.widget;
		if (button->getValue() && lastFrameLength > 0)
		{
			//setFrame();
		}
	}
	else if (name == "clear sequence")
	{
		if (live->getValue())
		{
			//setFrame();
		}
	}
	else if (name == "previous")
	{
		ofxUILabelButton * button = (ofxUILabelButton *) e.widget;
		if (stop && loadFrame > frame->getMin() && button->getValue())
		{
			-- loadFrame;
			setFrame();
		}
	}
	else if (name == "next")
	{
		ofxUILabelButton * button = (ofxUILabelButton *) e.widget;
		if (stop && loadFrame < frame->getMax() && button->getValue())
		{
			++ loadFrame;
			setFrame();
		}
	}
	else if (name == "frame" && !live->getValue() && frame->getValue() > 0)
	{
		loadFrame = frame->getValue();
		setFrame();
	}
	else if (name == "record")
	{

		if (record->getValue())
		{
			cout << "aaaaaaaa\n";
			if (queue_color != nullptr) delete queue_color;
			if (queue_depth != nullptr) delete queue_depth;
			queue_color = new lockFreeQueue<ofImage *>();
			queue_depth = new lockFreeQueue<ofShortImage *>();
			recorder.setup(prefix, queue_color, queue_depth, &depthProcessor, writeMesh->getValue());
			frameNum = 0;
		}
		else
		{
			cout << "bbbbbbbb\n";
			//recorder.close();
			recorder.stopThread();
			frame->setMaxAndMin(frameNum, 1);
		}
	}
}

void ofApp::setFrame()
{
	ofxUIIntSlider * frame = (ofxUIIntSlider *) gui->getWidget("frame");
	frame->setValue(loadFrame);
	char x[100];
	string frameName = prefix + string("color_") + itoa(loadFrame, x, 10) + string(".png");
	img_color->loadImage(frameName);
	frameName = prefix + string("depth_") + itoa(loadFrame, x, 10) + string(".png");
	img_depth->loadImage(frameName);
	frameName = prefix + string("pointCloud_") + itoa(loadFrame, x, 10) + string(".ply");
	recorder.loadMesh(frameName, mesh);
}

//--------------------------------------------------------------
void ofApp::update(){
	//++ roundClock;
	//if (roundClock % 2 != 0) return;

	ofxUIToggle* live = (ofxUIToggle*)gui->getWidget("live");
	ofxUIToggle* record = (ofxUIToggle*)gui->getWidget("record");
	ofxUIIntSlider * frame = (ofxUIIntSlider *) gui->getWidget("frame");

	if (!stop)
	{
		if (loadFrame < frame->getMax())
		{
			++ loadFrame;
			setFrame();
		}
	}

	device_->update();
	if (live->getValue() || frame->getValue() == 0)
	{
		if (color_.isFrameNew() && depth_.isFrameNew())
		{
			img_color->setFromPixels(depthProcessor.mapDepthToColor(color_.getPixelsRef(), depth_.getPixelsRef()));
			img_depth->setFromPixels(depth_.getPixelsRef(depth_.getNear(), depth_.getFar()));
			//img_depth->setFromPixels(depth_.getPixelsRef());

			img_color->mirror(false, true);
			img_depth->mirror(false, true);
		}
		if (record->getValue() && color_.isFrameNew() && depth_.isFrameNew())
		{
			queue_color->Produce(img_color);
			queue_depth->Produce(img_depth);
			recorder.signal();
			img_color = new ofImage(*img_color);
			img_depth = new ofShortImage(*img_depth);
			++ frameNum;
		}
	}
	setParameter();
}

//--------------------------------------------------------------
void ofApp::draw(){

	ofPushStyle();
	ofSetColor(10, 10, 10);
	ofRect(pointcloud);
	ofPopStyle();

	img_color->draw(imgPosX, imgPosY, imgViewerWidth, imgViewerHeight);
	img_depth->draw(imgPosX, imgPosY + imgViewerHeight + margin, imgViewerWidth, imgViewerHeight);

	cam.begin(pointcloud);
	ofEnableDepthTest();
	ofScale(10, 10, 10); // flip the y axis and zoom in a bit
	//ofRotateY(90);
	//ofTranslate(-img.getWidth() / 2, -img.getHeight() / 2);
	mesh.draw();
	ofDisableDepthTest();
	cam.end();

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}

//--------------------------------------------------------------
void ofApp::exit()
{
	img_color->clear();
	img_depth->clear();
	delete img_color;
	delete img_depth;

	depthProcessor.clear();
	device_->exit();
	delete device_;
	device_ = NULL;
}

void ofApp::setParameter()
{
	mainViewerWidth = ofGetWidth() - 2 * margin - panelWidth;
	mainViewerHeight = ofGetHeight() - 2 * margin;
	imgViewerWidth = (mainViewerWidth - margin) / 2;
	imgViewerHeight = (mainViewerHeight - margin) / 2;
	modelViewerWidth = imgViewerWidth;
	modelViewerHeight = imgViewerHeight * 2 + margin;
	imgPosX = margin + panelWidth;
	imgPosY = margin;
	modelPosX = imgPosX + imgViewerWidth + margin;
	modelPosY = imgPosY;

	pointcloud.x = modelPosX;
	pointcloud.y = modelPosY;
	pointcloud.width = modelViewerWidth;
	pointcloud.height = modelViewerHeight;
}

void ofApp::setMesh()
{
	mesh.clear();
	mesh.setMode(ofPrimitiveMode::OF_PRIMITIVE_POINTS);
	int pixelNum = depth_.getPixelsRef().size();
	CameraSpacePoint *v3d = new CameraSpacePoint[pixelNum];
	device_->getMapper()->MapDepthFrameToCameraSpace(pixelNum, img_depth->getPixels(), pixelNum, v3d);
	for (int i = 0; i < depth_.getHeight(); ++ i)
	{
		for (int j = 0; j < depth_.getWidth(); ++ j)
		{
			int idx = i * depth_.getWidth() + j;
			if (img_color->getColor(j, i) != ofColor(0))
			{
				mesh.addVertex(ofVec3f(v3d[idx].X, v3d[idx].Y, -v3d[idx].Z));
				mesh.addColor(img_color->getColor(j, i));
			}
		}
	}
}