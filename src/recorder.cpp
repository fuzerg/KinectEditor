#include "recorder.h"

ofxVideoDataWriterThread::ofxVideoDataWriterThread(){};
void ofxVideoDataWriterThread::setup(string filePath, lockFreeQueue<ofImage *> * qc, lockFreeQueue<ofShortImage *> * qd, ICoordinateMapper* _mapper, bool _writeMesh){
	this->filePath = filePath;
	queue_color = qc;
	queue_depth = qd;
	bIsWriting = false;
	bClose = false;
	number = 0;
	stopwriting = false;
	mapper = _mapper;
	writeMesh = _writeMesh;
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
			if (writeMesh)
			{
				frameName = filePath + string("pointCloud_") + itoa(number+1, x, 10);
				saveMesh(frameName, *frame_color, *frame_depth);
			}
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
		if (writeMesh)
		{
			frameName = filePath + string("pointCloud_") + itoa(number+1, x, 10);
			saveMesh(frameName, *frame_color, *frame_depth);
		}
		bIsWriting = false;
		delete frame_depth;
		delete frame_color;
		++ number;
		//cout << queue_depth->size() << endl;
		//cout << number << endl;
	}
}

void ofxVideoDataWriterThread::saveMesh(string filename, ofImage &img_color, ofShortImage &img_depth)
{
	cout << img_depth.height << ' ' << img_depth.width << endl;
	ofFile fout(filename, ofFile::WriteOnly, true);

	int pixelNum = img_depth.getPixelsRef().size();
	CameraSpacePoint *v3d = new CameraSpacePoint[pixelNum];
	mapper->MapDepthFrameToCameraSpace(pixelNum, img_depth.getPixels(), pixelNum, v3d);
	for (int i = 0; i < img_depth.height; ++ i)
	{
		for (int j = 0; j < img_depth.width; ++ j)
		{
			int idx = i * img_depth.width + j;
			ofColor &color = img_color.getColor(j, i);
			if (color != ofColor(0))
			{
				fout.write((char*)&v3d[idx].X, sizeof(float));
				fout.write((char*)&v3d[idx].Y, sizeof(float));
				fout.write((char*)&v3d[idx].Z, sizeof(float));
				fout << color.r << color.g << color.b;
			}
		}
	}
	delete[]v3d;

	fout.close();
}

void ofxVideoDataWriterThread::signal(){
	condition.signal();
}