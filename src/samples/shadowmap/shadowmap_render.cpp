#include "shadowmap_render.h"

#include <geom/vk_mesh.h>
#include <vk_pipeline.h>
#include <vk_buffers.h>
#include <iostream>
#include <cstdlib>

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>
#include <etna/RenderTargetStates.hpp>
#include <vulkan/vulkan_core.h>


/// RESOURCE ALLOCATION

void SimpleShadowmapRender::AllocateResources()
{
  mainViewDepth = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{m_width, m_height, 1},
    .format = vk::Format::eD32Sfloat,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment
  });

  shadowMap = m_context->createImage(etna::Image::CreateInfo
  {
    .extent = vk::Extent3D{2048, 2048, 1},
    .format = vk::Format::eD16Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled
  });

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{});
  constants = m_context->createBuffer(etna::Buffer::CreateInfo
  {
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY
  });

    m_uboMappedMem = constants.map();

   m_instanceDataBuffer = m_context->createBuffer(etna::Buffer::CreateInfo{
     .size       = sizeof(InstanceData) * total_instances,
    .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU
   });

   m_indirectCmdBuffer = m_context->createBuffer(etna::Buffer::CreateInfo{
     .size        = sizeof(VkDrawIndexedIndirectCommand) * m_instanceCounts.size(),
     .bufferUsage = vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
     .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU 
   });

   m_visibleIdxBuffer = m_context->createBuffer(etna::Buffer::CreateInfo{
     .size        = sizeof(uint) * total_instances,
     .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer,
     .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU 
   });

   m_modelDataBuffer = m_context->createBuffer(etna::Buffer::CreateInfo{
     .size        = sizeof(ModelData) * m_instanceCounts.size(),
     .bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
     .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU 
   });
}

void SimpleShadowmapRender::LoadScene(const char* path, bool transpose_inst_matrices)
{
  m_pScnMgr->LoadSceneXML(path, transpose_inst_matrices);
  assert(m_instanceCounts.size() == m_pScnMgr->InstancesNum()); // TODO load scene before resourses?
  InstanceData *instanceData         = reinterpret_cast<InstanceData *>(m_instanceDataBuffer.map());
  VkDrawIndexedIndirectCommand *cmds = reinterpret_cast<VkDrawIndexedIndirectCommand *>(m_indirectCmdBuffer.map());
  ModelData *modelData               = reinterpret_cast<ModelData *>(m_modelDataBuffer.map());
  int m = 0;
  for (int i = 0; i < m_pScnMgr->InstancesNum(); i++)
  {
    // Construct indirect command
    auto mesh_info = m_pScnMgr->GetMeshInfo(m_pScnMgr->GetInstanceInfo(i).mesh_id);
    cmds[i].indexCount    = mesh_info.m_indNum;
    cmds[i].instanceCount = m_instanceCounts[i];
    cmds[i].firstIndex    = mesh_info.m_indexOffset;
    cmds[i].vertexOffset  = mesh_info.m_vertexOffset;
    cmds[i].firstInstance = m;
    // Fill models info
    auto bbox             = m_pScnMgr->GetInstanceBbox(i);
    modelData[i].bbox.min      = bbox.boxMin;
    modelData[i].bbox.max = bbox.boxMax;
    // Generate instances
    for (int k = 0; k < m_instanceCounts[i]; k++) 
    {
      instanceData[m].transform = LiteMath::translate4x4(LiteMath::float3(
        (static_cast<float>(std::rand()) / RAND_MAX - .5) * 100,
        (static_cast<float>(std::rand()) / RAND_MAX - .5) * 100,
        (static_cast<float>(std::rand()) / RAND_MAX - .5) * 100)
      );
      instanceData[m].model = i;
      m++;
    }
  }
  m_instanceDataBuffer.unmap();
  m_indirectCmdBuffer.unmap();
  m_modelDataBuffer.unmap();

  // TODO: Make a separate stage
  loadShaders();
  PreparePipelines();

  auto loadedCam = m_pScnMgr->GetCamera(0);
  m_cam.fov = loadedCam.fov;
  m_cam.pos = float3(loadedCam.pos);
  m_cam.up  = float3(loadedCam.up);
  m_cam.lookAt = float3(loadedCam.lookAt);
  m_cam.tdist  = loadedCam.farPlane;
}

void SimpleShadowmapRender::DeallocateResources()
{
  mainViewDepth.reset(); // TODO: Make an etna method to reset all the resources
  shadowMap.reset();
  m_swapchain.Cleanup();
  vkDestroySurfaceKHR(GetVkInstance(), m_surface, nullptr);  

  constants = etna::Buffer();
}





/// PIPELINES CREATION

void SimpleShadowmapRender::PreparePipelines()
{
  // create full screen quad for debug purposes
  // 
  m_pFSQuad = std::make_shared<vk_utils::QuadRenderer>(0,0, 512, 512);
  m_pFSQuad->Create(m_context->getDevice(),
    VK_GRAPHICS_BASIC_ROOT "/resources/shaders/quad3_vert.vert.spv",
    VK_GRAPHICS_BASIC_ROOT "/resources/shaders/quad.frag.spv",
    vk_utils::RenderTargetInfo2D{
      .size          = VkExtent2D{ m_width, m_height },// this is debug full screen quad
      .format        = m_swapchain.GetFormat(),
      .loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD,// seems we need LOAD_OP_LOAD if we want to draw quad to part of screen
      .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 
    }
  );
  SetupSimplePipeline();
}

static void print_prog_info(const std::string &name)
{
  auto info = etna::get_shader_program(name);
  std::cout << "Program Info " << name << "\n";

  for (uint32_t set = 0u; set < etna::MAX_PROGRAM_DESCRIPTORS; set++)
  {
    if (!info.isDescriptorSetUsed(set))
      continue;
    auto setInfo = info.getDescriptorSetInfo(set);
    for (uint32_t binding = 0; binding < etna::MAX_DESCRIPTOR_BINDINGS; binding++)
    {
      if (!setInfo.isBindingUsed(binding))
        continue;
      auto &vkBinding = setInfo.getBinding(binding);

      std::cout << "Binding " << binding << " " << vk::to_string(vkBinding.descriptorType) << ", count = " << vkBinding.descriptorCount << " ";
      std::cout << " " << vk::to_string(vkBinding.stageFlags) << "\n"; 
    }
  }

  auto pc = info.getPushConst();
  if (pc.size)
  {
    std::cout << "PushConst " << " size = " << pc.size << " stages = " << vk::to_string(pc.stageFlags) << "\n";
  }
}

void SimpleShadowmapRender::loadShaders()
{
  etna::create_program("culling", { VK_GRAPHICS_BASIC_ROOT "/resources/shaders/culling.comp.spv" });
  etna::create_program("simple_material",
    {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple_shadow.frag.spv", VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});
  etna::create_program("simple_shadow", {VK_GRAPHICS_BASIC_ROOT"/resources/shaders/simple.vert.spv"});
}

void SimpleShadowmapRender::SetupSimplePipeline()
{
  std::vector<std::pair<VkDescriptorType, uint32_t> > dtypes = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             1},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     2}
  };

  m_pBindings = std::make_shared<vk_utils::DescriptorMaker>(m_context->getDevice(), dtypes, 2);
  
  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
  m_pBindings->BindImage(0, shadowMap.getView({}), defaultSampler.get(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  m_pBindings->BindEnd(&m_quadDS, &m_quadDSLayout);

  etna::VertexShaderInputDescription sceneVertexInputDesc
    {
      .bindings = {etna::VertexShaderInputDescription::Binding
        {
          .byteStreamDescription = m_pScnMgr->GetVertexStreamDescription()
        }}
    };

  auto& pipelineManager = etna::get_context().getPipelineManager();
  m_basicForwardPipeline = pipelineManager.createGraphicsPipeline("simple_material",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .colorAttachmentFormats = {static_cast<vk::Format>(m_swapchain.GetFormat())},
          .depthAttachmentFormat = vk::Format::eD32Sfloat
        }
    });
  m_shadowPipeline = pipelineManager.createGraphicsPipeline("simple_shadow",
    {
      .vertexShaderInput = sceneVertexInputDesc,
      .fragmentShaderOutput =
        {
          .depthAttachmentFormat = vk::Format::eD16Unorm
        }
    });
  m_cullingPipeline = pipelineManager.createComputePipeline("culling", {} );

  print_prog_info("simple_material");
  print_prog_info("simple_shadow");
  print_prog_info("culling");
}

void SimpleShadowmapRender::DestroyPipelines()
{
  m_pFSQuad     = nullptr; // smartptr delete it's resources
}



/// COMMAND BUFFER FILLING

void SimpleShadowmapRender::DrawSceneCmd(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp)
{
  VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT);

  VkDeviceSize zero_offset = 0u;
  VkBuffer vertexBuf = m_pScnMgr->GetVertexBuffer();
  VkBuffer indexBuf  = m_pScnMgr->GetIndexBuffer();
  
  vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vertexBuf, &zero_offset);
  vkCmdBindIndexBuffer(a_cmdBuff, indexBuf, 0, VK_INDEX_TYPE_UINT32);

  pushConst2M.projView = a_wvp; // TODO v wtf is this??? that's a master branch bug, not mine!
  vkCmdPushConstants(a_cmdBuff, m_basicForwardPipeline.getVkPipelineLayout(), stageFlags, 0, sizeof(pushConst2M), &pushConst2M);
  vkCmdDrawIndexedIndirect(a_cmdBuff, m_indirectCmdBuffer.get(), 0, m_pScnMgr->InstancesNum(), sizeof(VkDrawIndexedIndirectCommand));
}

void SimpleShadowmapRender::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkImage a_targetImage, VkImageView a_targetImageView)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  {
    VkDescriptorSet vkSet = etna::create_descriptor_set(etna::get_shader_program("culling").getDescriptorLayoutId(0), {
        etna::Binding{ 0, vk::DescriptorBufferInfo{ m_visibleIdxBuffer.get(), 0, VK_WHOLE_SIZE } },
        etna::Binding{ 1, vk::DescriptorBufferInfo{ m_instanceDataBuffer.get(), 0, VK_WHOLE_SIZE } } ,
        etna::Binding{ 2, vk::DescriptorBufferInfo{ m_modelDataBuffer.get(), 0, VK_WHOLE_SIZE } } ,
        etna::Binding{ 3, vk::DescriptorBufferInfo{ m_indirectCmdBuffer.get(), 0, VK_WHOLE_SIZE } },
    }).getVkSet();

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_cullingPipeline.getVkPipeline());
    vkCmdPushConstants(a_cmdBuff, m_cullingPipeline.getVkPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstCulling), &pushConstCulling);
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_cullingPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);
    vkCmdDispatch(a_cmdBuff, total_instances / 32 + 1, 1, 1);
  }

  {
    std::array barriers
      {
        // Transfer the shadowmap to depth write layout
        VkImageMemoryBarrier2
        {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
          .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
          .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = shadowMap.get(),
          .subresourceRange =
            {
              .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
            }
        },
      };
    VkDependencyInfo depInfo
      {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        .imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
        .pImageMemoryBarriers = barriers.data(),
      };
    vkCmdPipelineBarrier2(a_cmdBuff, &depInfo);
  }


  //// draw scene to shadowmap
  //
  // TODO culling breaks shadows, ideally we need a seperate culling for shadow pipeline, but it's out of scope of this task
  {
    etna::RenderTargetState renderTargets(a_cmdBuff, {2048, 2048}, {}, shadowMap.getView({}));
    {
      VkDescriptorSet vkSet   = etna::create_descriptor_set(etna::get_shader_program("simple_shadow").getDescriptorLayoutId(0), { 
        etna::Binding{ 2, vk::DescriptorBufferInfo{ m_instanceDataBuffer.get(), 0, VK_WHOLE_SIZE }} ,
        etna::Binding{ 3, vk::DescriptorBufferInfo{ m_visibleIdxBuffer.get(), 0, VK_WHOLE_SIZE } }
      }).getVkSet();

      vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);
      vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.getVkPipeline());
      DrawSceneCmd(a_cmdBuff, m_lightMatrix);
    }
  }

  {
    std::array barriers
      {
        // Transfer the shadowmap from depth write to shader read 
        VkImageMemoryBarrier2
        {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
          .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
          .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
          .oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
          .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = shadowMap.get(),
          .subresourceRange =
            {
              .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
            }
        },
        // Wait for the semaphore to signal that the swapchain image is available
        VkImageMemoryBarrier2
        {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          // Our semo signals this stage
          .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          .srcAccessMask = 0,
          .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = a_targetImage,
          .subresourceRange =
            {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
            }
        },
      };
    VkDependencyInfo depInfo
      {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        .imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
        .pImageMemoryBarriers = barriers.data(),
      };
    vkCmdPipelineBarrier2(a_cmdBuff, &depInfo);
  }

  //// draw final scene to screen
  //
  {

    VkDescriptorSet vkSet = etna::create_descriptor_set(etna::get_shader_program("simple_material").getDescriptorLayoutId(0), {
      etna::Binding {0, vk::DescriptorBufferInfo {constants.get(), 0, VK_WHOLE_SIZE}},
      etna::Binding {1, vk::DescriptorImageInfo {defaultSampler.get(), shadowMap.getView({}), vk::ImageLayout::eShaderReadOnlyOptimal}},
      etna::Binding {2, vk::DescriptorBufferInfo{ m_instanceDataBuffer.get(), 0, VK_WHOLE_SIZE } } ,
      etna::Binding {3, vk::DescriptorBufferInfo{ m_visibleIdxBuffer.get(), 0, VK_WHOLE_SIZE } }
    }).getVkSet();

    etna::RenderTargetState renderTargets(a_cmdBuff, {m_width, m_height}, {{a_targetImageView}}, mainViewDepth.getView({}));

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_basicForwardPipeline.getVkPipeline());
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_basicForwardPipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

    DrawSceneCmd(a_cmdBuff, m_worldViewProj);
  }

  if(m_input.drawFSQuad)
  {
    float scaleAndOffset[4] = {0.5f, 0.5f, -0.5f, +0.5f};
    m_pFSQuad->SetRenderTarget(a_targetImageView);
    m_pFSQuad->DrawCmd(a_cmdBuff, m_quadDS, scaleAndOffset);
  }
  
  {
    std::array barriers
      {
        // Transfer swapchain to present layout
        VkImageMemoryBarrier2
        {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
          .dstAccessMask = 0,
          .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = a_targetImage,
          .subresourceRange =
            {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
            }
        },
      };
    VkDependencyInfo depInfo
      {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        .imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
        .pImageMemoryBarriers = barriers.data(),
      };
    vkCmdPipelineBarrier2(a_cmdBuff, &depInfo);
  }

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}
