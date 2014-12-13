#include "recorder.h"

ofxVideoDataWriterThread::ofxVideoDataWriterThread(){};
void ofxVideoDataWriterThread::setup(string filePath, lockFreeQueue<ofImage *> * qc, lockFreeQueue<ofShortImage *> * qd, MapDepthToColor* _mapper, bool _writeMesh){
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
				frameName = filePath + string("pointCloud_") + itoa(number+1, x, 10) + string(".ply");
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
			frameName = filePath + string("pointCloud_") + itoa(number+1, x, 10) + string(".ply");
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
	mapper->m_pMapper->MapDepthFrameToCameraSpace(pixelNum, img_depth.getPixels(), pixelNum, v3d);

	int vertexNum = 0;
	for (int i = 0; i < img_depth.height; ++ i)
	{
		for (int j = 0; j < img_depth.width; ++ j)
		{
			/*if (img_color.getColor(j, i) != ofColor(0))
				++ vertexNum;*/
			int idx = i * img_depth.width + j;
			//if (v3d[idx].X != -INFINITE && v3d[idx].Y != -INFINITE && v3d[idx].Z != -INFINITE)
			if (v3d[idx].X + 1 != v3d[idx].X - 1 && img_color.getColor(j, i) != ofColor(0))
				++ vertexNum;
		}
	}

	fout << "ply\n";
	fout << "format binary_little_endian 1.0\n";
	fout << "element vertex " << vertexNum << "\n";
	fout << "property float x\n" << "property float y\n" << "property float z\n";
	fout << "end_header\n";

	for (int i = 0; i < img_depth.height; ++ i)
	{
		for (int j = 0; j < img_depth.width; ++ j)
		{
			int idx = i * img_depth.width + j;
			ofColor &color = img_color.getColor(j, i);
			//if (v3d[idx].X != -INFINITE && v3d[idx].Y != -INFINITE && v3d[idx].Z != -INFINITE)
			if (v3d[idx].X + 1 != v3d[idx].X - 1 && color != ofColor(0))
			{
				fout.write((char*)&v3d[idx].X, sizeof(float));
				fout.write((char*)&v3d[idx].Y, sizeof(float));
				v3d[idx].Z = -v3d[idx].Z;
				fout.write((char*)&v3d[idx].Z, sizeof(float));
				//fout << v3d[idx].X << ' ' << v3d[idx].Y << ' ' << v3d[idx].Z << endl;
			}
		}
	}
	delete[]v3d;

	fout.close();
}

void ofxVideoDataWriterThread::loadMesh(string filename, ofMesh &mesh)
{
	ofFile fin(filename, ofFile::ReadOnly, true);
	char x[100];
	fin.getline(x, 100);
	fin.getline(x, 100);
	string tmp;
	int vertexNum;
	fin >> tmp >> tmp >> vertexNum;
	fin.getline(x, 100);
	fin.getline(x, 100);
	fin.getline(x, 100);
	fin.getline(x, 100);
	fin.getline(x, 100);
	mesh.clear();
	mesh.setMode(ofPrimitiveMode::OF_PRIMITIVE_POINTS);
	for (int i = 0; i < vertexNum; ++ i)
	{
		float x, y, z;
		fin.read((char *) &x, sizeof(float));
		fin.read((char *) &y, sizeof(float));
		fin.read((char *) &z, sizeof(float));
		mesh.addVertex(ofVec3f(x, y, z));
	}
}

void ofxVideoDataWriterThread::signal(){
	condition.signal();
}