///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2018, Intel Corporation
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of 
// the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
// SOFTWARE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Camera.h"
#include "CameraController.h"
#include "Model.h"

#include "Art/Animation/SceneAnimation.h"

// Simple scene class to encapsulate animation and asset paths

class Scene {

public:

	Scene() {};

	bool LoadJson(const char* path);

	void Cleanup();

	// reloads the animation file
	bool UpdateAnimation();
	
	// overwrites the animation file
	void SaveAnimation();

	const std::string& GetName() const {
		return m_Name;
	}

	Model& GetModel() { return m_Model; }
	ART::SceneAnimation_ptr& GetAnimation() { return m_SceneAnimation; }
	Math::Camera& GetCamera() { return m_Camera; };

	const std::string& GetTextureRoot() const {
		return m_TextureRoot;
	}

	const std::string& GetRootFolder() const {
		return m_Root;
	}

	float GetModelRadius() const { return m_ModelRadius; }
	const Math::Vector3& GetCenter()const { return m_Center; }
	const Math::Vector3& GetShadowDim() const { return m_ShadowDim; }

protected:

	std::string					m_Name;
	std::string					m_AnimationPath;
	std::string					m_TextureRoot;
	std::string					m_Root;

	Model						m_Model;
	float						m_ModelRadius;
	Math::Vector3				m_Center;
	Math::Vector3				m_ShadowDim;
	ART::SceneAnimation_ptr		m_SceneAnimation;
	Math::Camera				m_Camera;

};
