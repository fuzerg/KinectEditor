#ifndef RECORDER_H
#define RECORDER_H

#include "ofMain.h"
#include "ofxKinect2.h"
#include <set>

template <typename T>
struct lockFreeQueue {
    lockFreeQueue(){
        list.push_back(T());
        iHead = list.begin();
        iTail = list.end();
    }
    void Produce(const T& t){
        list.push_back(t);
        iTail = list.end();
        list.erase(list.begin(), iHead);
    }
    bool Consume(T& t){
        typename TList::iterator iNext = iHead;
        ++iNext;
        if (iNext != iTail)
        {
            iHead = iNext;
            t = *iHead;
            return true;
        }
        return false;
    }
	int size() { return distance(iHead, iTail) - 1; }
    typename std::list<T>::iterator getHead() {return iHead; }
    typename std::list<T>::iterator getTail() {return iTail; }


private:
    typedef std::list<T> TList;
    TList list;
    typename TList::iterator iHead, iTail;
};

class ofxVideoDataWriterThread : public ofThread {
public:
    ofxVideoDataWriterThread();
//    void setup(ofFile *file, lockFreeQueue<ofPixels *> * q);
    void setup(string filePath, lockFreeQueue<ofImage *> * qc, lockFreeQueue<ofShortImage *> * qd, ICoordinateMapper* _mapper, bool _writeMesh = false);
    void threadedFunction();
	void saveMesh(string filename, ofImage &img_color, ofShortImage &img_depth);
    void signal();
    bool isWriting() { return bIsWriting; }
	void close() { stopwriting = true; }
	int getFrameNum() { return number; }
private:
    ofMutex conditionMutex;
    Poco::Condition condition;
    string filePath;
    int number;
    lockFreeQueue<ofImage *> * queue_color;
	lockFreeQueue<ofShortImage *> * queue_depth;
    bool bIsWriting;
    bool bClose;
	bool stopwriting;
	bool writeMesh;
	ICoordinateMapper* mapper;
};


#endif //RECORDER_H