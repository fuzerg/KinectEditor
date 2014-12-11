#include "mapDepthToColor.h"

void MapDepthToColor::init(int depthWidth, int depthHeight, int colorWidth, int colorHeight, ICoordinateMapper * pMapper)
{
	cDepthVisibilityTestThreshold = 50;
	
	cDepthWidth = depthWidth;
	cDepthHeight = depthHeight;
	
	cColorWidth = colorWidth;
	cColorHeight = colorHeight;

	m_pMapper = pMapper;
	m_pDepthDistortionMap = new DepthSpacePoint[cDepthWidth * cDepthHeight];
	m_pDepthDistortionLT = new UINT[cDepthWidth * cDepthHeight];
	m_pDepthVisibilityTestMap = nullptr;
}

void MapDepthToColor::clear()
{
	delete[]m_pDepthDistortionMap;
	delete[]m_pDepthDistortionLT;
	if (m_pDepthVisibilityTestMap != nullptr)
		delete[]m_pDepthVisibilityTestMap;
}

bool MapDepthToColor::isCoordinateMapChanged()
{
	CameraIntrinsics intrinsics = {};

	m_pMapper->GetDepthCameraIntrinsics(&intrinsics);

	float focalLengthX = intrinsics.FocalLengthX / cDepthWidth;
	float focalLengthY = intrinsics.FocalLengthY / cDepthHeight;
	float principalPointX = intrinsics.PrincipalPointX / cDepthWidth;
	float principalPointY = intrinsics.PrincipalPointY / cDepthHeight;

	if (m_cameraParameters.FocalLengthX == focalLengthX && m_cameraParameters.FocalLengthY == focalLengthY &&
		m_cameraParameters.PrincipalPointX == principalPointX && m_cameraParameters.PrincipalPointY == principalPointY)
		return false; 

	m_cameraParameters.FocalLengthX = focalLengthX;
	m_cameraParameters.FocalLengthY = focalLengthY;
	m_cameraParameters.PrincipalPointX = principalPointX;
	m_cameraParameters.PrincipalPointY = principalPointY;

	return true;
}

void MapDepthToColor::setupUndistortion()
{
	if (m_cameraParameters.PrincipalPointX != 0)
	{
		const UINT depthBufferSize = cDepthWidth * cDepthHeight;

		CameraSpacePoint cameraFrameCorners[4] = //at 1 meter distance. Take into account that depth frame is mirrored
		{
			/*LT*/ { -m_cameraParameters.PrincipalPointX / m_cameraParameters.FocalLengthX, m_cameraParameters.PrincipalPointY / m_cameraParameters.FocalLengthY, 1.f }, 
			/*RT*/ { (1.f - m_cameraParameters.PrincipalPointX) / m_cameraParameters.FocalLengthX, m_cameraParameters.PrincipalPointY / m_cameraParameters.FocalLengthY, 1.f }, 
			/*LB*/ { -m_cameraParameters.PrincipalPointX / m_cameraParameters.FocalLengthX, (m_cameraParameters.PrincipalPointY - 1.f) / m_cameraParameters.FocalLengthY, 1.f }, 
			/*RB*/ { (1.f - m_cameraParameters.PrincipalPointX) / m_cameraParameters.FocalLengthX, (m_cameraParameters.PrincipalPointY - 1.f) / m_cameraParameters.FocalLengthY, 1.f }
		};

		for(UINT rowID = 0; rowID < cDepthHeight; rowID++)
		{
			const float rowFactor = float(rowID) / float(cDepthHeight - 1);
			const CameraSpacePoint rowStart = 
			{
				cameraFrameCorners[0].X + (cameraFrameCorners[2].X - cameraFrameCorners[0].X) * rowFactor,
				cameraFrameCorners[0].Y + (cameraFrameCorners[2].Y - cameraFrameCorners[0].Y) * rowFactor,
				1.f
			};

			const CameraSpacePoint rowEnd = 
			{
				cameraFrameCorners[1].X + (cameraFrameCorners[3].X - cameraFrameCorners[1].X) * rowFactor,
				cameraFrameCorners[1].Y + (cameraFrameCorners[3].Y - cameraFrameCorners[1].Y) * rowFactor,
				1.f
			};

			const float stepFactor = 1.f / float(cDepthWidth - 1);
			const CameraSpacePoint rowDelta = 
			{
				(rowEnd.X - rowStart.X) * stepFactor,
				(rowEnd.Y - rowStart.Y) * stepFactor,
				0
			};

			_ASSERT(cDepthWidth == NUI_DEPTH_RAW_WIDTH);
			CameraSpacePoint cameraCoordsRow[NUI_DEPTH_RAW_WIDTH];

			CameraSpacePoint currentPoint = rowStart;
			for(UINT i = 0; i < cDepthWidth; i++)
			{
				cameraCoordsRow[i] = currentPoint;
				currentPoint.X += rowDelta.X;
				currentPoint.Y += rowDelta.Y;
			}

			m_pMapper->MapCameraPointsToDepthSpace(cDepthWidth, cameraCoordsRow, cDepthWidth, &m_pDepthDistortionMap[rowID * cDepthWidth]);
		}

		UINT* pLT = m_pDepthDistortionLT;
		for(UINT i = 0; i < depthBufferSize; i++, pLT++)
		{
			//nearest neighbor depth lookup table 
			UINT x = UINT(m_pDepthDistortionMap[i].X + 0.5f);
			UINT y = UINT(m_pDepthDistortionMap[i].Y + 0.5f);

			*pLT = (x < cDepthWidth && y < cDepthHeight)? x + y * cDepthWidth : UINT_MAX; 
		} 
		m_bHaveValidCameraParameters = true;
	}
	else
	{
		m_bHaveValidCameraParameters = false;
	}
}

ofPixels MapDepthToColor::mapDepthToColor(ofPixels & tmpColorImg, ofShortPixels & tmpDepthImg)
{
	ofPixels colorImg(tmpColorImg);
	ofShortPixels depthImg(tmpDepthImg);
	if (isCoordinateMapChanged())
		setupUndistortion();

	ColorSpacePoint *m_pColorCoordinates = new ColorSpacePoint[cDepthWidth * cDepthHeight];

	// Get the coordinates to convert color to depth space
	m_pMapper->MapDepthFrameToColorSpace(cDepthWidth * cDepthHeight, depthImg.getPixels(), 
		cDepthWidth * cDepthHeight, m_pColorCoordinates);

	// construct dense depth points visibility test map so we can test for depth points that are invisible in color space
	const UINT16* const pDepthEnd = depthImg.getPixels() + cDepthWidth * cDepthHeight;
	const ColorSpacePoint* pColorPoint = m_pColorCoordinates;

	const int cVisibilityTestQuantShift = 2;
	const UINT testMapWidth = UINT(cColorWidth >> cVisibilityTestQuantShift);
	const UINT testMapHeight = UINT(cColorHeight >> cVisibilityTestQuantShift);
	if (m_pDepthVisibilityTestMap == nullptr) m_pDepthVisibilityTestMap = new UINT16[testMapWidth * testMapHeight];
	ZeroMemory(m_pDepthVisibilityTestMap, testMapWidth * testMapHeight * sizeof(UINT16));

	for(const UINT16* pDepth = depthImg.getPixels(); pDepth < pDepthEnd; pDepth++, pColorPoint++)
	{
		const int x = int(pColorPoint->X + 0.5f) >> cVisibilityTestQuantShift;
		const int y = int(pColorPoint->Y + 0.5f) >> cVisibilityTestQuantShift;
		if(x >= 0 && x < testMapWidth && y >= 0 && y < testMapHeight)
		{
			const int idx = y * testMapWidth + x;
			const UINT16 oldDepth = m_pDepthVisibilityTestMap[idx];
			const UINT16 newDepth = *pDepth;
			if(!oldDepth || oldDepth > newDepth)
			{
				m_pDepthVisibilityTestMap[idx] = newDepth;
			}
		}
	}

	/*for (UINT y = 0; y < cDepthHeight; ++ y)
	{
		for (UINT x = 0; x < cDepthWidth; ++ x)
		{
			UINT cx = UINT(pColorPoint[x + y * cDepthWidth].X + 0.5f) >> cVisibilityTestQuantShift;
			UINT cy = UINT(pColorPoint[x + y * cDepthWidth].Y + 0.5f) >> cVisibilityTestQuantShift;
			if(cx < testMapWidth && cy < testMapHeight)
			{
				UINT idx = cx + cy * testMapWidth;
				UINT16 oldDepth = m_pDepthVisibilityTestMap[idx];
				UINT16 newDepth = depthImg.getColor(x, y).r;
				if(!oldDepth || oldDepth > newDepth)
				{
					m_pDepthVisibilityTestMap[idx] = newDepth;
				}
			}
		}
	}*/

	ofPixels colorDataInDepthFrame;
	colorDataInDepthFrame.allocate(cDepthWidth, cDepthHeight, OF_IMAGE_COLOR);
	for(UINT y = 0; y < cDepthHeight; ++ y)
	{
		const UINT depthWidth = cDepthWidth;
		const UINT depthImagePixels = cDepthWidth * cDepthHeight;
		const UINT colorHeight = cColorHeight;
		const UINT colorWidth = cColorWidth;

		UINT destIndex = y * depthWidth;
		for (UINT x = 0; x < depthWidth; ++x, ++destIndex)
		{
			ofColor pixelColor(0);
			const UINT mappedIndex = m_pDepthDistortionLT[destIndex];
			//const UINT mappedIndex = destIndex;
			if(mappedIndex < depthImagePixels)
			{
				// retrieve the depth to color mapping for the current depth pixel
				const ColorSpacePoint colorPoint = m_pColorCoordinates[mappedIndex];

				// make sure the depth pixel maps to a valid point in color space
				const int colorX = (int)(colorPoint.X + 0.5f);
				const int colorY = (int)(colorPoint.Y + 0.5f);
				if (colorX >= 0 && colorX < colorWidth && colorY >= 0 && colorY < colorHeight)
				{
					const UINT16 depthValue = depthImg.getPixels()[mappedIndex];
					const UINT testX = colorX >> cVisibilityTestQuantShift;
					const UINT testY = colorY >> cVisibilityTestQuantShift;
					const UINT testIdx = testY * testMapWidth + testX;
					const UINT16 depthTestValue = m_pDepthVisibilityTestMap[testIdx];
					_ASSERT(depthValue >= depthTestValue);
					if(depthValue - depthTestValue < cDepthVisibilityTestThreshold)
					{
						// calculate index into color array
						//const UINT colorIndex = colorX + (colorY * colorWidth);
						pixelColor = colorImg.getColor(colorX, colorY);
					}
				}
			}
			colorDataInDepthFrame.setColor(x, y, pixelColor);
		}
	}
	return colorDataInDepthFrame;
}