/*
 * GigaVoxels is a ray-guided streaming library used for efficient
 * 3D real-time rendering of highly detailed volumetric scenes.
 *
 * Copyright (C) 2011-2013 INRIA <http://www.inria.fr/>
 *
 * Authors : GigaVoxels Team
 *
 * GigaVoxels is distributed under a dual-license scheme.
 * You can obtain a specific license from Inria at gigavoxels-licensing@inria.fr.
 * Otherwise the default license is the GPL version 3.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** 
 * @version 1.0
 */

#ifndef _CUSTOM_SECTION_EDITOR_H_
#define _CUSTOM_SECTION_EDITOR_H_

/******************************************************************************
 ******************************* INCLUDE SECTION ******************************
 ******************************************************************************/

// GvViewer
#include <GvvSectionEditor.h>

// Project
#include "UI_GvQCustomEditor.h"

/******************************************************************************
 ************************* DEFINE AND CONSTANT SECTION ************************
 ******************************************************************************/

/******************************************************************************
 ***************************** TYPE DEFINITION ********************************
 ******************************************************************************/

/******************************************************************************
 ******************************** CLASS USED **********************************
 ******************************************************************************/

// GvViewer
namespace GvViewerCore
{
	class GvvBrowsable;
}

/******************************************************************************
 ****************************** CLASS DEFINITION ******************************
 ******************************************************************************/

/** 
 * @class CustomSectionEditor
 *
 * @brief The CustomSectionEditor class provides the user custom editor to
 * this GigaVoxels pipeline effect.
 *
 * This editor is a child of the parent CustomEditor. It holds a user interface
 * to edit the GigaVoxels pipeline parameters.
 * It is a GvvSectionEditor, so it is reprenseted by a separate page of parameters
 * in the main viewer user interface.
 */
class CustomSectionEditor : public GvViewerGui::GvvSectionEditor, public Ui::GvQCustomEditor
{
	// Qt Macro
	Q_OBJECT

	/**************************************************************************
	 ***************************** PUBLIC SECTION *****************************
	 **************************************************************************/

public:

	/******************************* ATTRIBUTES *******************************/
	
	/******************************** METHODS *********************************/

	/**
	 * Default constructor
	 *
	 * @param pParent parent widget
	 * @param pFlags the window flags
	 */
	CustomSectionEditor( QWidget* pParent = 0, Qt::WindowFlags pFlags = 0 );

	/**
	 * Destructor
	 */
	virtual ~CustomSectionEditor();

	/**************************************************************************
	 **************************** PROTECTED SECTION ***************************
	 **************************************************************************/

protected:
	
	/******************************* ATTRIBUTES *******************************/

	/******************************** METHODS *********************************/
	
	/**
	 * Populates this editor with the specified browsable
	 *
	 * @param pBrowsable specifies the browsable to be edited
	 */
	virtual void populate( GvViewerCore::GvvBrowsable* pBrowsable );
	
	/**************************************************************************
	 ***************************** PRIVATE SECTION ****************************
	 **************************************************************************/

private:
	
	/******************************* ATTRIBUTES *******************************/
	
	/******************************** METHODS *********************************/

private slots:

	/**
	 * Slot called when the 3D model file button has been clicked (released)
	 */
	void on__3DModelToolButton_released();

	/**
	 * Slot called when max depth value has changed
	 */
	void on__xTranslationSpinBox_valueChanged( double pValue );

	/**
	 * Slot called when max depth value has changed
	 */
	void on__yTranslationSpinBox_valueChanged( double pValue );

	/**
	 * Slot called when max depth value has changed
	 */
	void on__zTranslationSpinBox_valueChanged( double pValue );

	/**
	 * Slot called when max depth value has changed
	 */
	void on__xRotationSpinBox_valueChanged( double pValue );

	/**
	 * Slot called when max depth value has changed
	 */
	void on__yRotationSpinBox_valueChanged( double pValue );

	/**
	 * Slot called when max depth value has changed
	 */
	void on__zRotationSpinBox_valueChanged( double pValue );

	/**
	 * Slot called when max depth value has changed
	 */
	void on__angleRotationSpinBox_valueChanged( double pValue );

	/**
	 * Slot called when max depth value has changed
	 */
	void on__uniformScaleSpinBox_valueChanged( double pValue );

	// Shadow Caster

	/**
	 * Slot called when the 3D model file button has been clicked (released)
	 */
	void on__3DModelToolButton_2_released();

	/**
	 * Slot called when max depth value has changed
	 */
	void on__xTranslationSpinBox_2_valueChanged( double pValue );

	/**
	 * Slot called when max depth value has changed
	 */
	void on__yTranslationSpinBox_2_valueChanged( double pValue );

	/**
	 * Slot called when max depth value has changed
	 */
	void on__zTranslationSpinBox_2_valueChanged( double pValue );

	/**
	 * Slot called when max depth value has changed
	 */
	void on__xRotationSpinBox_2_valueChanged( double pValue );

	/**
	 * Slot called when max depth value has changed
	 */
	void on__yRotationSpinBox_2_valueChanged( double pValue );

	/**
	 * Slot called when max depth value has changed
	 */
	void on__zRotationSpinBox_2_valueChanged( double pValue );

	/**
	 * Slot called when max depth value has changed
	 */
	void on__angleRotationSpinBox_2_valueChanged( double pValue );

	/**
	 * Slot called when max depth value has changed
	 */
	void on__uniformScaleSpinBox_2_valueChanged( double pValue );

};

#endif
