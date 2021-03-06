/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include "vk_common.h"
#include "vk_core.h"
#include "api/replay/renderdoc_replay.h"
#include "replay/replay_driver.h"
#include "core/core.h"

struct TextPrintState
{
	VkCommandBuffer cmd;
	VkRenderPass rp;
	VkFramebuffer fb;
	uint32_t w, h;
};

struct MeshDisplayPipelines
{
	enum
	{
		ePipe_Wire = 0,
		ePipe_WireDepth,
		ePipe_Solid,
		ePipe_SolidDepth,
		ePipe_Lit,
		ePipe_Secondary,
		ePipe_Count,
	};

	VkPipeline pipes[ePipe_Count];
};

struct VulkanPostVSData
{
	struct StageData
	{
		VkBuffer buf;
		VkDeviceMemory bufmem;
		VkPrimitiveTopology topo;

		uint32_t numVerts;
		uint32_t vertStride;
		uint32_t instStride;

		bool useIndices;
		VkBuffer idxBuf;
		VkDeviceMemory idxBufMem;
		VkIndexType idxFmt;

		bool hasPosOut;

		float nearPlane;
		float farPlane;
	} vsin, vsout, gsout;

	VulkanPostVSData()
	{
		RDCEraseEl(vsin);
		RDCEraseEl(vsout);
		RDCEraseEl(gsout);
	}

	const StageData &GetStage(MeshDataStage type)
	{
		if(type == eMeshDataStage_VSOut)
			return vsout;
		else if(type == eMeshDataStage_GSOut)
			return gsout;
		else
			RDCERR("Unexpected mesh data stage!");

		return vsin;
	}
};

class VulkanResourceManager;

class VulkanDebugManager
{
	public:
		VulkanDebugManager(WrappedVulkan *driver, VkDevice dev);
		~VulkanDebugManager();

		void BeginText(const TextPrintState &textstate);
		void RenderText(const TextPrintState &textstate, float x, float y, const char *fmt, ...);
		void EndText(const TextPrintState &textstate);

		ResourceId RenderOverlay(ResourceId texid, TextureDisplayOverlay overlay, uint32_t eventID, const vector<uint32_t> &passEvents);

		void InitPostVSBuffers(uint32_t eventID);

		// indicates that EID alias is the same as eventID
		void AliasPostVSBuffers(uint32_t eventID, uint32_t alias) { m_PostVSAlias[alias] = eventID; }

		MeshFormat GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage);
		void GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, vector<byte> &ret);

		struct GPUBuffer
		{
			enum CreateFlags
			{
				eGPUBufferReadback = 0x1,
				eGPUBufferVBuffer = 0x2,
				eGPUBufferSSBO = 0x4,
			};
			GPUBuffer() : buf(VK_NULL_HANDLE), mem(VK_NULL_HANDLE) {}
			void Create(WrappedVulkan *driver, VkDevice dev, VkDeviceSize size, uint32_t ringSize, uint32_t flags);
			void Destroy(const VkLayerDispatchTable *vt, VkDevice dev);

			void FillDescriptor(VkDescriptorBufferInfo &desc);

			void *Map(const VkLayerDispatchTable *vt, VkDevice dev, VkDeviceSize &bindoffset, VkDeviceSize usedsize = 0);
			void *Map(const VkLayerDispatchTable *vt, VkDevice dev, uint32_t *bindoffset = NULL, VkDeviceSize usedsize = 0);
			void Unmap(const VkLayerDispatchTable *vt, VkDevice dev);

			VkDeviceSize sz;
			VkBuffer buf;
			VkDeviceMemory mem;

			// uniform buffer alignment requirement
			VkDeviceSize align;

			// for handling ring allocations
			VkDeviceSize totalsize;
			VkDeviceSize curoffset;
			
			VulkanResourceManager *GetResourceManager() { return m_ResourceManager; }
			VulkanResourceManager *m_ResourceManager;
		};

		VkDescriptorPool m_DescriptorPool;
		VkSampler m_LinearSampler, m_PointSampler;

		VkDescriptorSetLayout m_CheckerboardDescSetLayout;
		VkPipelineLayout m_CheckerboardPipeLayout;
		VkDescriptorSet m_CheckerboardDescSet;
		VkPipeline m_CheckerboardPipeline;
		VkPipeline m_CheckerboardMSAAPipeline;
		GPUBuffer m_CheckerboardUBO;

		VkDescriptorSetLayout m_TexDisplayDescSetLayout;
		VkPipelineLayout m_TexDisplayPipeLayout;
		VkDescriptorSet m_TexDisplayDescSet[16]; // ring buffered to allow multiple texture renders between flushes
		uint32_t m_TexDisplayNextSet;
		VkPipeline m_TexDisplayPipeline, m_TexDisplayBlendPipeline, m_TexDisplayF32Pipeline;
		GPUBuffer m_TexDisplayUBO;

		VkImage m_TexDisplayDummyImages[12];
		VkImageView m_TexDisplayDummyImageViews[12];
		VkWriteDescriptorSet m_TexDisplayDummyWrites[12];
		VkDescriptorImageInfo m_TexDisplayDummyInfos[12];
		VkDeviceMemory m_TexDisplayDummyMemory;

		VkDescriptorSet GetTexDisplayDescSet()
		{
			m_TexDisplayNextSet = (m_TexDisplayNextSet+1)%ARRAY_COUNT(m_TexDisplayDescSet);
			return m_TexDisplayDescSet[m_TexDisplayNextSet];
		}

		VkDeviceMemory m_PickPixelImageMem;
		VkImage m_PickPixelImage;
		VkImageView m_PickPixelImageView;
		GPUBuffer m_PickPixelReadbackBuffer;
		VkFramebuffer m_PickPixelFB;
		VkRenderPass m_PickPixelRP;
		
		VkDescriptorSetLayout m_TextDescSetLayout;
		VkPipelineLayout m_TextPipeLayout;
		VkDescriptorSet m_TextDescSet;
		VkPipeline m_TextPipeline;
		GPUBuffer m_TextGeneralUBO;
		GPUBuffer m_TextGlyphUBO;
		GPUBuffer m_TextStringUBO;
		VkImage m_TextAtlas;
		VkDeviceMemory m_TextAtlasMem;
		VkImageView m_TextAtlasView;
		GPUBuffer m_TextAtlasUpload;
		
		VkDeviceMemory m_OverlayImageMem;
		VkImage m_OverlayImage;
		VkImageView m_OverlayImageView;
		VkFramebuffer m_OverlayNoDepthFB;
		VkRenderPass m_OverlayNoDepthRP;
		VkExtent2D m_OverlayDim;
		VkDeviceSize m_OverlayMemSize;

		GPUBuffer m_OverdrawRampUBO;
		VkDescriptorSetLayout m_QuadDescSetLayout;
		VkDescriptorSet m_QuadDescSet;
		VkPipelineLayout m_QuadResolvePipeLayout;
		VkPipeline m_QuadResolvePipeline;
		vector<uint32_t> *m_QuadSPIRV;
		
		VkDescriptorSetLayout m_MeshDescSetLayout;
		VkPipelineLayout m_MeshPipeLayout;
		VkDescriptorSet m_MeshDescSet;
		GPUBuffer m_MeshUBO, m_MeshBBoxVB, m_MeshAxisFrustumVB;
		VkShaderModule m_MeshModules[3];
		
		enum TextureType
		{
			eTexType_1D = 1, // implicitly an array
			eTexType_2D, // implicitly an array
			eTexType_3D,
			eTexType_2DMS,
			eTexType_Max
		};
		
		GPUBuffer m_MinMaxTileResult;                    // tile result buffer
		GPUBuffer m_MinMaxResult, m_MinMaxReadback;      // Vec4f[2] final result buffer
		GPUBuffer m_HistogramBuf, m_HistogramReadback;   // uint32_t * num buckets buffer
		VkDescriptorSetLayout m_HistogramDescSetLayout;
		VkPipelineLayout m_HistogramPipeLayout;
		VkDescriptorSet m_HistogramDescSet[2];
		GPUBuffer m_HistogramUBO;
		VkPipeline m_HistogramPipe[eTexType_Max][3]; // float, uint, sint
		VkPipeline m_MinMaxTilePipe[eTexType_Max][3]; // float, uint, sint
		VkPipeline m_MinMaxResultPipe[3]; // float, uint, sint

		VkDescriptorSetLayout m_OutlineDescSetLayout;
		VkPipelineLayout m_OutlinePipeLayout;
		VkDescriptorSet m_OutlineDescSet;
		VkPipeline m_OutlinePipeline;
		GPUBuffer m_OutlineUBO;
		
		GPUBuffer m_ReadbackWindow;
		
		VkDescriptorSetLayout m_MeshFetchDescSetLayout;
		VkDescriptorSet m_MeshFetchDescSet;

		MeshDisplayPipelines CacheMeshDisplayPipelines(const MeshFormat &primary, const MeshFormat &secondary);
		void MakeGraphicsPipelineInfo(VkGraphicsPipelineCreateInfo &pipeCreateInfo, ResourceId pipeline);
	private:
		void InitDebugData();
		void ShutdownDebugData();

		VulkanResourceManager *GetResourceManager() { return m_ResourceManager; }

		static const uint32_t m_ShaderCacheMagic   = 0xf00d00d5;
		static const uint32_t m_ShaderCacheVersion = 1;

		bool m_ShaderCacheDirty, m_CacheShaders;
		map<uint32_t, vector<uint32_t>*> m_ShaderCache;
		
		string GetSPIRVBlob(SPIRVShaderStage shadType, const std::vector<std::string> &sources, vector<uint32_t> **outBlob);
		
		void PatchFixedColShader(VkShaderModule &mod, float col[4]);
		
		void RenderTextInternal(const TextPrintState &textstate, float x, float y, const char *text);
		static const uint32_t FONT_TEX_WIDTH = 256;
		static const uint32_t FONT_TEX_HEIGHT = 128;

		LogState m_State;

		float m_FontCharAspect;
		float m_FontCharSize;

		vector<uint32_t> *m_FixedColSPIRV;
		
		map<uint64_t, MeshDisplayPipelines> m_CachedMeshPipelines;

		map<uint32_t, VulkanPostVSData> m_PostVSData;
		map<uint32_t, uint32_t> m_PostVSAlias;
		
		WrappedVulkan *m_pDriver;
		VulkanResourceManager *m_ResourceManager;

		VkDevice m_Device;
};
