///*
// * GigaVoxels is a ray-guided streaming library used for efficient
// * 3D real-time rendering of highly detailed volumetric scenes.
// *
// * Copyright (C) 2011-2012 INRIA <http://www.inria.fr/>
// *
// * Authors : GigaVoxels Team
// *
// * This program is free software: you can redistribute it and/or modify
// * it under the terms of the GNU General Public License as published by
// * the Free Software Foundation, either version 3 of the License, or
// * (at your option) any later version.
// *
// * This program is distributed in the hope that it will be useful,
// * but WITHOUT ANY WARRANTY; without even the implied warranty of
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// * GNU General Public License for more details.
// *
// * You should have received a copy of the GNU General Public License
// * along with this program.  If not, see <http://www.gnu.org/licenses/>.
// */
//
///** 
// * @version 1.0
// */
//
//#ifndef _CLIPPING_PLANE_WIDGET_H_
//#define _CLIPPING_PLANE_WIDGET_H_
//
///******************************************************************************
// ******************************* INCLUDE SECTION ******************************
// ******************************************************************************/
//
//// OpenGL
//#include <GL/glew.h>
//
//// Qt
//#include <QGLViewer/manipulatedCameraFrame.h>
//
///******************************************************************************
// ************************* DEFINE AND CONSTANT SECTION ************************
// ******************************************************************************/
//
///******************************************************************************
// ***************************** TYPE DEFINITION ********************************
// ******************************************************************************/
//
///******************************************************************************
// ******************************** CLASS USED **********************************
// ******************************************************************************/
//
///******************************************************************************
// ****************************** CLASS DEFINITION ******************************
// ******************************************************************************/
//
///** 
// * @class ClippingPlaneWidget
// *
// * @brief The SampleViewer class provides...
// *
// * A clipping plane is manipulated using a ManipulatedFrame.
// */
//class ClippingPlaneWidget : public qglviewer::ManipulatedFrame
//{
//
//	/**************************************************************************
//	 ***************************** PUBLIC SECTION *****************************
//	 **************************************************************************/
//
//public:
//
//	/******************************* ATTRIBUTES *******************************/
//
//	/******************************** METHODS *********************************/
//
//	/**
//	 * Constructor
//	 */
//	SampleViewer();
//
//	/**
//	 * Destructor
//	 */
//	virtual ~SampleViewer();
//
//	/**************************************************************************
//	 **************************** PROTECTED SECTION ***************************
//	 **************************************************************************/
//
//protected:
//
//	/******************************* ATTRIBUTES *******************************/
//
//	/**
//	 * Clipping plane
//	 */
//	qglviewer::ManipulatedFrame* _clippingPlane;
//
//	/******************************** METHODS *********************************/
//
//	/**
//	 * Initialize the viewer
//	 */
//	virtual void init();
//
//	/**
//	 * Draw function called each frame
//	 */
//	virtual void draw();
//
//	/**
//	 * Resize GL event handler
//	 *
//	 * @param width the new width
//	 * @param height the new height
//	 */
//	virtual void resizeGL( int width, int height );
//
//	/**
//	 * Get the viewer size hint
//	 *
//	 * @return the viewer size hint
//	 */
//	virtual QSize sizeHint() const;
//
//	/**
//	 * Key press event handler
//	 *
//	 * @param e the event
//	 */
//	virtual void keyPressEvent( QKeyEvent* e );
//	
//	/**************************************************************************
//	 ***************************** PRIVATE SECTION ****************************
//	 **************************************************************************/
//
//private:
//
//	/******************************* ATTRIBUTES *******************************/
//
//	/**
//	 * Core class containing the GigaVoxels pipeline
//	 */
//	SampleCore* _sampleCore;
//
//	/**
//	 * QGL viewer Manipulated Frame used to draw and manipulate a light in the 3D view
//	 */
//	qglviewer::ManipulatedFrame* _light1;
//
//	/******************************** METHODS *********************************/
//
//};
//
//#endif // !_CLIPPING_PLANE_WIDGET_H_
