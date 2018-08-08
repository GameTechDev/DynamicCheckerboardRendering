//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  Alex Nankervis
//

#include "Model.h"
#include "Utility.h"
#include "TextureManager.h"
#include "GraphicsCore.h"
#include "DescriptorHeap.h"
#include "CommandContext.h"
#include <stdio.h>

bool Model::LoadH3D(const char *filename)
{
    FILE *file = nullptr;
    if (0 != fopen_s(&file, filename, "rb"))
        return false;

    bool ok = false;

    if (1 != fread(&m_Header, sizeof(Header), 1, file)) goto h3d_load_fail;

    m_pMesh = new Mesh [m_Header.meshCount];
    m_pMaterial = new Material [m_Header.materialCount];

	m_pMaterialConstants = new RenderMaterial[m_Header.materialCount];

    if (m_Header.meshCount > 0)
        if (1 != fread(m_pMesh, sizeof(Mesh) * m_Header.meshCount, 1, file)) goto h3d_load_fail;
    if (m_Header.materialCount > 0)
        if (1 != fread(m_pMaterial, sizeof(Material) * m_Header.materialCount, 1, file)) goto h3d_load_fail;

    m_VertexStride = m_pMesh[0].vertexStride;
    m_VertexStrideDepth = m_pMesh[0].vertexStrideDepth;
#if _DEBUG
    for (uint32_t meshIndex = 1; meshIndex < m_Header.meshCount; ++meshIndex)
    {
        const Mesh& mesh = m_pMesh[meshIndex];
        ASSERT(mesh.vertexStride == m_VertexStride);
        ASSERT(mesh.vertexStrideDepth == m_VertexStrideDepth);
    }
    for (uint32_t meshIndex = 0; meshIndex < m_Header.meshCount; ++meshIndex)
    {
        const Mesh& mesh = m_pMesh[meshIndex];

        ASSERT( mesh.attribsEnabled ==
            (attrib_mask_position | attrib_mask_texcoord0 | attrib_mask_normal | attrib_mask_tangent | attrib_mask_bitangent) );
        ASSERT(mesh.attrib[0].components == 3 && mesh.attrib[0].format == Model::attrib_format_float); // position
        ASSERT(mesh.attrib[1].components == 2 && mesh.attrib[1].format == Model::attrib_format_float); // texcoord0
        ASSERT(mesh.attrib[2].components == 3 && mesh.attrib[2].format == Model::attrib_format_float); // normal
        ASSERT(mesh.attrib[3].components == 3 && mesh.attrib[3].format == Model::attrib_format_float); // tangent
        ASSERT(mesh.attrib[4].components == 3 && mesh.attrib[4].format == Model::attrib_format_float); // bitangent

        ASSERT( mesh.attribsEnabledDepth ==
            (attrib_mask_position) );
        ASSERT(mesh.attrib[0].components == 3 && mesh.attrib[0].format == Model::attrib_format_float); // position
    }
#endif

    m_pVertexData = new unsigned char[ m_Header.vertexDataByteSize ];
    m_pIndexData = new unsigned char[ m_Header.indexDataByteSize ];
    m_pVertexDataDepth = new unsigned char[ m_Header.vertexDataByteSizeDepth ];
    m_pIndexDataDepth = new unsigned char[ m_Header.indexDataByteSize ];

    if (m_Header.vertexDataByteSize > 0)
        if (1 != fread(m_pVertexData, m_Header.vertexDataByteSize, 1, file)) goto h3d_load_fail;
    if (m_Header.indexDataByteSize > 0)
        if (1 != fread(m_pIndexData, m_Header.indexDataByteSize, 1, file)) goto h3d_load_fail;

    if (m_Header.vertexDataByteSizeDepth > 0)
        if (1 != fread(m_pVertexDataDepth, m_Header.vertexDataByteSizeDepth, 1, file)) goto h3d_load_fail;
    if (m_Header.indexDataByteSize > 0)
        if (1 != fread(m_pIndexDataDepth, m_Header.indexDataByteSize, 1, file)) goto h3d_load_fail;

    m_VertexBuffer.Create(L"VertexBuffer", m_Header.vertexDataByteSize / m_VertexStride, m_VertexStride, m_pVertexData);
    m_IndexBuffer.Create(L"IndexBuffer", m_Header.indexDataByteSize / sizeof(uint16_t), sizeof(uint16_t), m_pIndexData);
    delete [] m_pVertexData;
    m_pVertexData = nullptr;
    delete [] m_pIndexData;
    m_pIndexData = nullptr;

    m_VertexBufferDepth.Create(L"VertexBufferDepth", m_Header.vertexDataByteSizeDepth / m_VertexStrideDepth, m_VertexStrideDepth, m_pVertexDataDepth);
    m_IndexBufferDepth.Create(L"IndexBufferDepth", m_Header.indexDataByteSize / sizeof(uint16_t), sizeof(uint16_t), m_pIndexDataDepth);
    delete [] m_pVertexDataDepth;
    m_pVertexDataDepth = nullptr;
    delete [] m_pIndexDataDepth;
    m_pIndexDataDepth = nullptr;

    LoadTextures();

	m_MaterialConstants.Create(L"MaterialConstantsBuffer", m_Header.materialCount, sizeof(RenderMaterial), (unsigned char*) m_pMaterialConstants);
	delete[] m_pMaterialConstants;

    ok = true;

h3d_load_fail:

    if (EOF == fclose(file))
        ok = false;

    return ok;
}

bool Model::SaveH3D(const char *filename) const
{
    FILE *file = nullptr;
    if (0 != fopen_s(&file, filename, "wb"))
        return false;

    bool ok = false;

    if (1 != fwrite(&m_Header, sizeof(Header), 1, file)) goto h3d_save_fail;

    if (m_Header.meshCount > 0)
        if (1 != fwrite(m_pMesh, sizeof(Mesh) * m_Header.meshCount, 1, file)) goto h3d_save_fail;
    if (m_Header.materialCount > 0)
        if (1 != fwrite(m_pMaterial, sizeof(Material) * m_Header.materialCount, 1, file)) goto h3d_save_fail;

    if (m_Header.vertexDataByteSize > 0)
        if (1 != fwrite(m_pVertexData, m_Header.vertexDataByteSize, 1, file)) goto h3d_save_fail;
    if (m_Header.indexDataByteSize > 0)
        if (1 != fwrite(m_pIndexData, m_Header.indexDataByteSize, 1, file)) goto h3d_save_fail;

    if (m_Header.vertexDataByteSizeDepth > 0)
        if (1 != fwrite(m_pVertexDataDepth, m_Header.vertexDataByteSizeDepth, 1, file)) goto h3d_save_fail;
    if (m_Header.indexDataByteSize > 0)
        if (1 != fwrite(m_pIndexDataDepth, m_Header.indexDataByteSize, 1, file)) goto h3d_save_fail;

    ok = true;

h3d_save_fail:

    if (EOF == fclose(file))
        ok = false;

    return ok;
}

void Model::ReleaseTextures()
{
    /*
    if (m_Textures != nullptr)
    {
        for (uint32_t materialIdx = 0; materialIdx < m_Header.materialCount; ++materialIdx)
        {
            for (int n = 0; n < Material::texCount; n++)
            {
                if (m_Textures[materialIdx * Material::texCount + n])
                    m_Textures[materialIdx * Material::texCount + n]->Release();
                m_Textures[materialIdx * Material::texCount + n] = nullptr;
            }
        }
        delete [] m_Textures;
    }
    */
}

void Model::LoadTextures(void)
{
    ReleaseTextures();

	const int numTexChannels = MaterialTexChannel_Count;

    m_SRVs = new D3D12_CPU_DESCRIPTOR_HANDLE[m_Header.materialCount * numTexChannels];

    const ManagedTexture* MatTextures[numTexChannels] = {};

    for (uint32_t materialIdx = 0; materialIdx < m_Header.materialCount; ++materialIdx)
    {
        const Material& material = m_pMaterial[materialIdx];
		RenderMaterial& renderMat = m_pMaterialConstants[materialIdx];
		//renderMat.diffuse = material.diffuse;
		//renderMat.ambient = material.ambient;
		//renderMat.emissive = material.emissive;
		//renderMat.shininess = material.shininess;
		//renderMat.specular = material.specular;
		XMStoreFloat3(&renderMat.diffuse, material.diffuse);
		XMStoreFloat3(&renderMat.specular, material.specular);
		XMStoreFloat3(&renderMat.emissive, material.emissive);
		renderMat.shininess = material.shininess;
		renderMat.textureMask = 0;


        // Load diffuse
		TextureManager::ReportLoadErrors(true);
		MatTextures[MaterialTexChannel_Diffuse] = TextureManager::LoadFromFile(material.texDiffusePath, true);
		TextureManager::ReportLoadErrors(false);
		
		if (!MatTextures[MaterialTexChannel_Diffuse]->IsValid()) {
			MatTextures[MaterialTexChannel_Diffuse] = TextureManager::LoadFromFile("default", true);
		}
		else {
			renderMat.textureMask |= 1;
		}

        // Load specular
		TextureManager::ReportLoadErrors(true);
		MatTextures[MaterialTexChannel_Specular] = TextureManager::LoadFromFile(material.texSpecularPath, true);
		TextureManager::ReportLoadErrors(false);
		
		if (!MatTextures[MaterialTexChannel_Specular]->IsValid())
        {
            MatTextures[MaterialTexChannel_Specular] = TextureManager::LoadFromFile(std::string(material.texDiffusePath) + "_specular", true);
            if (!MatTextures[MaterialTexChannel_Specular]->IsValid())
                MatTextures[MaterialTexChannel_Specular] = TextureManager::LoadFromFile("default_specular", true);
        }
		else {
			renderMat.textureMask |= 2;
		}

        //// Load emissive
        ////MatTextures[2] = TextureManager::LoadFromFile(pMaterial.texEmissivePath, true);

        // Load normal
		TextureManager::ReportLoadErrors(true);
		MatTextures[MaterialTexChannel_Normal] = TextureManager::LoadFromFile(material.texNormalPath, false);
		printf("Normal map: %s\n", material.texNormalPath);
		TextureManager::ReportLoadErrors(false);
		if (!MatTextures[MaterialTexChannel_Normal]->IsValid())
        {
            MatTextures[MaterialTexChannel_Normal] = TextureManager::LoadFromFile(std::string(material.texDiffusePath) + "_normal", false);
            if (!MatTextures[MaterialTexChannel_Normal]->IsValid())
                MatTextures[MaterialTexChannel_Normal] = TextureManager::LoadFromFile("default_normal", false);
        }
		else {
			renderMat.textureMask |= 4;
		}
		TextureManager::ReportLoadErrors(false);

		MatTextures[MaterialTexChannel_Opacity] = (ManagedTexture*) &TextureManager::GetWhiteTex2D(); // TextureManager::LoadFromFile("default", true);
		if (material.hasMask) {
			printf("Alpha mask: %s\n", material.texOpacityPath);
			TextureManager::ReportLoadErrors(true);
			auto manTex = TextureManager::LoadFromFile(material.texOpacityPath, false);
			TextureManager::ReportLoadErrors(false);

			if (manTex->IsValid()) {
				MatTextures[MaterialTexChannel_Opacity] = manTex;
			}

		}

		// Load lightmap
        //MatTextures[4] = TextureManager::LoadFromFile(pMaterial.texLightmapPath, true);

        // Load reflection
        //MatTextures[5] = TextureManager::LoadFromFile(pMaterial.texReflectionPath, true);

		const int matBaseOffset = materialIdx * numTexChannels;
		m_SRVs[matBaseOffset + MaterialTexChannel_Diffuse] = MatTextures[MaterialTexChannel_Diffuse]->GetSRV();
        m_SRVs[matBaseOffset + MaterialTexChannel_Specular] = MatTextures[MaterialTexChannel_Specular]->GetSRV();
        m_SRVs[matBaseOffset + MaterialTexChannel_Emissive] = MatTextures[0]->GetSRV(); // yet unsupported
        m_SRVs[matBaseOffset + MaterialTexChannel_Normal] = MatTextures[MaterialTexChannel_Normal]->GetSRV();
		m_SRVs[matBaseOffset + MaterialTexChannel_Lightmap] = MatTextures[0]->GetSRV();
        m_SRVs[matBaseOffset + MaterialTexChannel_Reflection] = MatTextures[0]->GetSRV();
		m_SRVs[matBaseOffset + MaterialTexChannel_Opacity] = MatTextures[MaterialTexChannel_Opacity]->GetSRV();
		//m_SRVs[matBaseOffset + MaterialTexChannel_Constants] = m_MaterialConstants.GetSRV();

	}
}
