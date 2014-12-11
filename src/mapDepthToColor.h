#ifndef MAPDEPTHTOCOLOR_H
#define MAPDEPTHTOCOLOR_H

#include "ofxKinect2.h"



#define NUI_DEPTH_RAW_WIDTH 512
#define NUI_DEPTH_RAW_HEIGHT 424

class MapDepthToColor
{
public:
	void init(int depthWidth, int depthHeight, int colorWidth, int colorHeight, ICoordinateMapper * pMapper);
	bool isCoordinateMapChanged();
	void setupUndistortion();
	ofPixels mapDepthToColor(ofPixels &, ofShortPixels &);
	void clear();

	int cDepthWidth;
	int cDepthHeight;
	int cColorWidth;
	int cColorHeight;
	int cDepthVisibilityTestThreshold;

	ICoordinateMapper* m_pMapper;
	CameraIntrinsics m_cameraParameters;
	DepthSpacePoint *m_pDepthDistortionMap;
	UINT *m_pDepthDistortionLT;
	UINT16 *m_pDepthVisibilityTestMap;
	bool m_bHaveValidCameraParameters;
};

#endif //MAPDEPTHTOCOLOR_H