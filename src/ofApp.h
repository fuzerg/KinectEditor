#pragma once

#include "ofMain.h"
#include "ofxKinect2.h"
#include "ofxUI.h"
#include "recorder.h"
#include "mapDepthToColor.h"
#include "utils\DepthRemapToRange.h"

class ofApp : public ofBaseApp{

public:
	void setup();
	void update();
	void draw();

	void keyPressed(int key);
	void keyReleased(int key);
	void mouseMoved(int x, int y );
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void windowResized(int w, int h);
	void dragEvent(ofDragInfo dragInfo);
	void gotMessage(ofMessage msg);

	void exit();

	void setParameter();
	void setMesh();

	int margin;
	int panelWidth;
	int mainViewerWidth;
	int mainViewerHeight;
	int imgViewerWidth;
	int imgViewerHeight;
	int modelViewerWidth;
	int modelViewerHeight;
	int imgPosX;
	int imgPosY;
	int modelPosX;
	int modelPosY;

	ofRectangle pointcloud;
	ofEasyCam cam;
	ofMesh mesh;
	ofImage img;
	ofImage *img_color, *img_originColor;
	ofShortImage *img_depth, img_showDepth;

	ofxKinect2::Device* device_;
	ofxKinect2::IrStream ir_;
	ofxKinect2::ColorStream color_;
	ofxKinect2::DepthStream depth_;
	MapDepthToColor depthProcessor;
	int roundClock;

	int frameNum;
	int loadFrame;
	int lastFrameLength;

	lockFreeQueue<ofImage *> * queue_color, *queue_originColor;
	lockFreeQueue<ofShortImage *> * queue_depth;
	ofxVideoDataWriterThread recorder;
	string prefix;

	ofxUICanvas *gui;
	bool stop;
	void guiEvent(ofxUIEventArgs &e);
	void setFrame();
};
