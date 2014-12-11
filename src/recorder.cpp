#include "recorder.h"

ofxVideoDataWriterThread::ofxVideoDataWriterThread(){};
void ofxVideoDataWriterThread::setup(string filePath, lockFreeQueue<ofImage *> * qc, lockFreeQueue<ofShortImage *> * qd){
	this->filePath = filePath;
	queue_color = qc;
	queue_depth = qd;
	bIsWriting = false;
	bClose = false;
	number = 0;
	stopwriting = false;
	startThread(true, false);
}

void ofxVideoDataWriterThread::threadedFunction(){

	while(isThreadRunning())
	{
		ofShortImage * frame_depth = NULL;
		ofImage * frame_color = NULL;
		if(queue_depth->Consume(frame_depth)){
			queue_color->Consume(frame_color);
			bIsWriting = true;
			char x[100];
			string frameName = filePath + string("color_") + itoa(number+1, x, 10) + string(".png");
			frame_color->saveImage(frameName);
			frameName = filePath + string("depth_") + itoa(number+1, x, 10) + string(".png");
			frame_depth->saveImage(frameName);
			bIsWriting = false;
			delete frame_depth;
			delete frame_color;
			++ number;
			//cout << queue_depth->size() << endl;
			//cout << number << endl;
		}
		else{
			condition.wait(conditionMutex);
		}
	}
	ofShortImage * frame_depth = NULL;
	ofImage * frame_color = NULL;
	while (queue_depth->Consume(frame_depth)){
		queue_color->Consume(frame_color);
		bIsWriting = true;
		char x[100];
		string frameName = filePath + string("color_") + itoa(number+1, x, 10) + string(".png");
		frame_color->saveImage(frameName);
		frameName = filePath + string("depth_") + itoa(number+1, x, 10) + string(".png");
		frame_depth->saveImage(frameName);
		bIsWriting = false;
		delete frame_depth;
		delete frame_color;
		++ number;
		//cout << queue_depth->size() << endl;
		//cout << number << endl;
	}
}

void ofxVideoDataWriterThread::signal(){
	condition.signal();
}