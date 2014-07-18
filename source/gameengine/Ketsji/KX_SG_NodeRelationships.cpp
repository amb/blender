/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Ketsji/KX_SG_NodeRelationships.cpp
 *  \ingroup ketsji
 */


#include "KX_SG_NodeRelationships.h"


/** 
 * KX_NormalParentRelation - a regular parent/child relation, the child's coordinates are relative to the parent
 */

KX_NormalParentRelation* KX_NormalParentRelation::New()
{
	return new KX_NormalParentRelation();
}

bool KX_NormalParentRelation::UpdateChildCoordinates(SG_Spatial *child, const SG_Spatial *parent, bool &parentUpdated)
{
	MT_assert(child != NULL);

	/* If nothing changed in the parent or child, there is nothing to do here */
	if (!parentUpdated && !child->IsModified())
		return false;

	/* The child has no parent, it is a root object.
	 * The world and local coordinates should be the same and applied directly. */
	if (parent==NULL) {
		child->SetWorldFromLocalTransform();
	}
	/* The child has a parent. The child's coordinates are defined relative to the parent's.
	 * The parent's coordinates should be applied to the child's local ones to calculate the real world position. */
	else {
		const MT_Vector3 & p_world_scale = parent->GetWorldScaling();
		const MT_Point3 & p_world_pos = parent->GetWorldPosition();
		const MT_Matrix3x3 & p_world_orientation = parent->GetWorldOrientation();
		const MT_Vector3 & local_scale = child->GetLocalScale();
		const MT_Point3 & local_pos = child->GetLocalPosition();
		const MT_Matrix3x3 & local_orientation = child->GetLocalOrientation();

		child->SetWorldScale(p_world_scale * local_scale);
		child->SetWorldOrientation(p_world_orientation * local_orientation);
		child->SetWorldPosition(p_world_pos + p_world_scale * local_scale * (p_world_orientation * local_orientation * local_pos));
	}

	parentUpdated = true;  //this variable is going to be used to update the children of this child
	child->ClearModified();
	return true;
}

SG_ParentRelation* KX_NormalParentRelation::NewCopy()
{
	return new KX_NormalParentRelation();
}

KX_NormalParentRelation::~KX_NormalParentRelation()
{
	//nothing to do
}

KX_NormalParentRelation::KX_NormalParentRelation()
{
	// nothing to do
}





/**
 * KX_VertexParentRelation - the child only inherits the position, not the orientation or scale
 */

KX_VertexParentRelation* KX_VertexParentRelation::New()
{
	return new KX_VertexParentRelation();
}

bool KX_VertexParentRelation::UpdateChildCoordinates(SG_Spatial *child, const SG_Spatial *parent, bool &parentUpdated)
{
	MT_assert(child != NULL);

	/* If nothing changed in the parent or child, there is nothing to do here */
	if (!parentUpdated && !child->IsModified())
		return false;

	/* The parent (if existing) is a vertex, so only position should be applied
	 * to the child's local coordinates to calculate the real world position. */

	if (parent==NULL)
		child->SetWorldPosition(child->GetLocalPosition());
	else
		child->SetWorldPosition(child->GetLocalPosition()+parent->GetWorldPosition());

	child->SetWorldScale(child->GetLocalScale());
	child->SetWorldOrientation(child->GetLocalOrientation());

	parentUpdated = true;  //this variable is going to be used to update the children of this child
	child->ClearModified();
	return true;
}

SG_ParentRelation* KX_VertexParentRelation::NewCopy()
{
	return new KX_VertexParentRelation();
}

KX_VertexParentRelation::~KX_VertexParentRelation()
{
	//nothing to do
}

KX_VertexParentRelation::KX_VertexParentRelation()
{
	//nothing to do
}



/**
 * Slow parent relationship
 */

	KX_SlowParentRelation *
KX_SlowParentRelation::
New(
	MT_Scalar relaxation
) {
	return new 	KX_SlowParentRelation(relaxation);
}

/** 
 * Method inherited from KX_ParentRelation
 */

	bool
KX_SlowParentRelation::
UpdateChildCoordinates(
	SG_Spatial * child,
	const SG_Spatial * parent,
	bool& parentUpdated
) {
	MT_assert(child != NULL);

	// the child will move even if the parent is not
	parentUpdated = true;

	const MT_Vector3 & child_scale = child->GetLocalScale();
	const MT_Point3 & child_pos = child->GetLocalPosition();
	const MT_Matrix3x3 & child_rotation = child->GetLocalOrientation();

	// the childs world locations which we will update.
	
	MT_Vector3 child_w_scale;
	MT_Point3 child_w_pos;
	MT_Matrix3x3 child_w_rotation;
		
	if (parent) {

		// This is a slow parent relation
		// first compute the normal child world coordinates.

		MT_Vector3 child_n_scale;
		MT_Point3 child_n_pos;
		MT_Matrix3x3 child_n_rotation;

		const MT_Vector3 & p_world_scale = parent->GetWorldScaling();
		const MT_Point3 & p_world_pos = parent->GetWorldPosition();
		const MT_Matrix3x3 & p_world_rotation = parent->GetWorldOrientation();

		child_n_scale = p_world_scale * child_scale;
		child_n_rotation = p_world_rotation * child_rotation;

		child_n_pos = p_world_pos + p_world_scale * 
			(p_world_rotation * child_pos);


		if (m_initialized) {

			// get the current world positions

			child_w_scale = child->GetWorldScaling();
			child_w_pos = child->GetWorldPosition();
			child_w_rotation = child->GetWorldOrientation();

			// now 'interpolate' the normal coordinates with the last 
			// world coordinates to get the new world coordinates.

			MT_Scalar weight = MT_Scalar(1)/(m_relax + 1);
			child_w_scale = (m_relax * child_w_scale + child_n_scale) * weight;
			child_w_pos = (m_relax * child_w_pos + child_n_pos) * weight;
			// for rotation we must go through quaternion
			MT_Quaternion child_w_quat = child_w_rotation.getRotation().slerp(child_n_rotation.getRotation(), weight);
			child_w_rotation.setRotation(child_w_quat);
			//FIXME: update physics controller.
		} else {
			child_w_scale = child_n_scale;
			child_w_pos = child_n_pos;
			child_w_rotation = child_n_rotation;
			m_initialized = true;
		}
			
	} else {

		child_w_scale = child_scale;
		child_w_pos = child_pos;
		child_w_rotation = child_rotation;
	}

	child->SetWorldScale(child_w_scale);
	child->SetWorldPosition(child_w_pos);
	child->SetWorldOrientation(child_w_rotation);
	child->ClearModified();
	// this node must always be updated, so reschedule it for next time
	child->ActivateRecheduleUpdateCallback();
	
	return true; //parent != NULL;
}

/** 
 * Method inherited from KX_ParentRelation
 */

	SG_ParentRelation *
KX_SlowParentRelation::
NewCopy(
) {
	return new 	KX_SlowParentRelation(m_relax);
}

KX_SlowParentRelation::
KX_SlowParentRelation(
	MT_Scalar relaxation
):
	m_relax(relaxation),
	m_initialized(false)
{
	//nothing to do
}

KX_SlowParentRelation::
~KX_SlowParentRelation(
) {
	//nothing to do
}




