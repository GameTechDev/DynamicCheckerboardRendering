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

#include "pch.h"

#include "Scene.h"

#include "document.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

using namespace ART;
using namespace Math;
using namespace std::experimental;

#define STRMATCH(x, y) (strcmp(x, y) == 0)

bool Scene::LoadJson(const char* path) {

	std::ifstream file;
	file.open(path, std::ios::binary);
	if (!file) {
		std::cout << "Failed to open scene file: " << path << std::endl;
		return false;
	}
	std::stringstream ss;
	ss << file.rdbuf();
	file.close();

	m_ShadowDim = Math::Vector3(0);

	filesystem::path fullPath(filesystem::absolute( filesystem::path(path) ));

	filesystem::path sceneRootDir = fullPath.remove_filename();
	m_Root = sceneRootDir.generic_string();

	//std::cout << "Scene root dir: " << sceneRootDir;

	filesystem::path modelPath;
	filesystem::path animPath;
	filesystem::path textureRootPath = sceneRootDir / filesystem::path("Textures");

	//bool hasCamera = false;
	//Camera sceneCam;
	bool hasAnimation = false;

	rapidjson::Document doc;
	if (doc.Parse(ss.str().c_str()).HasParseError()) {
		std::cout << "Error parsing JSON: document not recognized as object." << std::endl;
		return false;
	}

	for (rapidjson::Value::MemberIterator it = doc.MemberBegin(); it != doc.MemberEnd(); it++) {

		const char* name = it->name.GetString();
		rapidjson::Value& val = it->value;

		try {

			if (STRMATCH(name, "ModelPath")) {
				if (!val.IsString())
					throw SerializationException("'ModelPath' : expected string");
				modelPath = val.GetString();
				if (modelPath.is_relative()) {
					modelPath = sceneRootDir / modelPath;
				}
			}
			else if (STRMATCH(name, "Animation")) {
				if (!val.IsString())
					throw SerializationException("'Animation' : expected string");
				animPath = val.GetString();
				if (animPath.is_relative()) {
					animPath = sceneRootDir / animPath;
				}
				hasAnimation = true;
			}
			else if (STRMATCH(name, "TextureRoot")) {
				if (!val.IsString())
					throw SerializationException("'TextureRoot' : expected string");
				textureRootPath = val.GetString();
				if (textureRootPath.is_relative()) {
					textureRootPath = sceneRootDir / textureRootPath;
				}
			}
			else if (STRMATCH(name, "Camera")) {
				if (!val.IsObject())
					throw SerializationException("'Camera' : expected JSON object");
				// To be supported in the future...
			}
			else if (STRMATCH(name, "ShadowDim")) {
				if (!val.IsArray() || val.Size() != 3)
					throw SerializationException("'ShadowDim' : expected array of 3 floats");
				m_ShadowDim.SetX((float)val[0].GetDouble());
				m_ShadowDim.SetY((float)val[1].GetDouble());
				m_ShadowDim.SetZ((float)val[2].GetDouble());
			}
		}
		catch (SerializationException ex) {
			std::stringstream msg;
			msg << "Error parsing scene: " << ex.Message;
			Utility::Print(msg.str().c_str());
			return false;
		}

	}

	Cleanup();

	// no real JSON load yet, just migrated the code from ModelViewer
	std::cout << std::endl;
	std::cout << "Model path: " << modelPath.generic_string() << std::endl;
	std::cout << "Texture root: " << textureRootPath << std::endl;

	TextureManager::Initialize(textureRootPath.wstring() + L"/");

	bool success = m_Model.Load(modelPath.generic_string().c_str());
	ASSERT(success, "Failed to load model");
	ASSERT(m_Model.m_Header.meshCount > 0, "Model contains no meshes");

	auto box = m_Model.m_Header.boundingBox.max - m_Model.m_Header.boundingBox.min;
	std::cout << "BBox size: [" << box.GetX() << ", " << box.GetY() << ", " << box.GetZ() << "]" << std::endl;

	m_ModelRadius = Length(m_Model.m_Header.boundingBox.max - m_Model.m_Header.boundingBox.min) * .5f;
	m_Center = (m_Model.m_Header.boundingBox.min + m_Model.m_Header.boundingBox.max) * .5f;
	const Vector3 eye = (m_Model.m_Header.boundingBox.min + m_Model.m_Header.boundingBox.max) * .5f + Vector3(m_ModelRadius * .5f, 0.0f, 0.0f);
	m_Camera.SetEyeAtUp(eye, Vector3(kZero), Vector3(kYUnitVector));
	m_Camera.SetZRange(0.0001f * m_ModelRadius, 10.0f * m_ModelRadius);

	m_SceneAnimation = std::make_shared<SceneAnimation>();
	// set default animation if load fails
	if (!hasAnimation || !m_SceneAnimation->LoadJson(animPath.generic_string().c_str())) {
		m_SceneAnimation->SetTimeSpan(0, 5);
		m_SceneAnimation->SetTime(0);
		animPath = sceneRootDir / "default.anim";
	}

	m_AnimationPath = animPath.generic_string();
	m_TextureRoot = textureRootPath.generic_string() + std::string("/");

	return success;
}

bool Scene::UpdateAnimation() {
	// attempt to load the animation file again
	auto newAnimation = std::make_shared<SceneAnimation>();
	if (newAnimation->LoadJson(m_AnimationPath.c_str())) {
		m_SceneAnimation = newAnimation;
		return true;
	}
	return false;
}

void Scene::SaveAnimation() {
	m_SceneAnimation->SaveJson(m_AnimationPath.c_str());
}

void Scene::Cleanup() {
	m_Model.Clear();
	m_SceneAnimation = nullptr;
}
