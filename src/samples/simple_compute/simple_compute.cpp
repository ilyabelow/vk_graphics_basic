#include "simple_compute.h"

#include <string>
#include <array>
#include <chrono>
#include <iomanip>
#include <random>

#include <vk_pipeline.h>
#include <vk_buffers.h>
#include <vk_utils.h>

SimpleCompute::SimpleCompute(uint32_t a_length) : m_length(a_length)
{
#ifdef NDEBUG
  m_enableValidation = false;
#else
  m_enableValidation = true;
#endif
}

void SimpleCompute::SetupValidationLayers()
{
  m_validationLayers.push_back("VK_LAYER_KHRONOS_validation");
  m_validationLayers.push_back("VK_LAYER_LUNARG_monitor");
}

void SimpleCompute::InitVulkan(const char** a_instanceExtensions, uint32_t a_instanceExtensionsCount, uint32_t a_deviceId)
{
  m_instanceExtensions.clear();
  for (uint32_t i = 0; i < a_instanceExtensionsCount; ++i) {
    m_instanceExtensions.push_back(a_instanceExtensions[i]);
  }
  SetupValidationLayers();
  VK_CHECK_RESULT(volkInitialize());
  CreateInstance();
  volkLoadInstance(m_instance);

  CreateDevice(a_deviceId);
  volkLoadDevice(m_device);

  m_commandPool = vk_utils::createCommandPool(m_device, m_queueFamilyIDXs.compute, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  m_cmdBufferCompute = vk_utils::createCommandBuffers(m_device, m_commandPool, 1)[0];
  
  m_pCopyHelper = std::make_shared<vk_utils::SimpleCopyHelper>(m_physicalDevice, m_device, m_transferQueue, m_queueFamilyIDXs.compute, 8*1024*1024);
}


void SimpleCompute::CreateInstance()
{
  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pNext = nullptr;
  appInfo.pApplicationName = "VkRender";
  appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  appInfo.pEngineName = "SimpleCompute";
  appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  appInfo.apiVersion = VK_MAKE_VERSION(1, 1, 0);

  m_instance = vk_utils::createInstance(m_enableValidation, m_validationLayers, m_instanceExtensions, &appInfo);
  if (m_enableValidation)
    vk_utils::initDebugReportCallback(m_instance, &debugReportCallbackFn, &m_debugReportCallback);
}

void SimpleCompute::CreateDevice(uint32_t a_deviceId)
{
  m_physicalDevice = vk_utils::findPhysicalDevice(m_instance, true, a_deviceId, m_deviceExtensions);

  m_device = vk_utils::createLogicalDevice(m_physicalDevice, m_validationLayers, m_deviceExtensions,
                                           m_enabledDeviceFeatures, m_queueFamilyIDXs,
                                           VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);

  vkGetDeviceQueue(m_device, m_queueFamilyIDXs.compute, 0, &m_computeQueue);
  vkGetDeviceQueue(m_device, m_queueFamilyIDXs.transfer, 0, &m_transferQueue);
}


void SimpleCompute::SetupSimplePipeline()
{
  std::vector<std::pair<VkDescriptorType, uint32_t> > dtypes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             3}
  };

  // Создание и аллокация буферов
  m_A = vk_utils::createBuffer(m_device, sizeof(float) * m_length, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  m_B = vk_utils::createBuffer(m_device, sizeof(float) * m_length, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  vk_utils::allocateAndBindWithPadding(m_device, m_physicalDevice, {m_A, m_B}, 0);

  m_pBindings = std::make_shared<vk_utils::DescriptorMaker>(m_device, dtypes, 1);

  // Создание descriptor set для передачи буферов в шейдер
  m_pBindings->BindBegin(VK_SHADER_STAGE_COMPUTE_BIT);
  m_pBindings->BindBuffer(0, m_A);
  m_pBindings->BindBuffer(1, m_B);
  m_pBindings->BindEnd(&m_sumDS, &m_sumDSLayout);

  // Заполнение буферов

  m_pCopyHelper->UpdateBuffer(m_A, 0, m_data.data(), sizeof(float) * m_data.size());
}

void SimpleCompute::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkPipeline a_pipeline)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  // Заполняем буфер команд
  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  vkCmdBindPipeline      (a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, a_pipeline);
  vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_COMPUTE, m_layout, 0, 1, &m_sumDS, 0, NULL);

  vkCmdPushConstants(a_cmdBuff, m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(m_length), &m_length);
  vkCmdDispatch(a_cmdBuff, (m_length + m_groupSize - 1) / m_groupSize, 1, 1);
  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}


void SimpleCompute::CleanupPipeline()
{
  if (m_cmdBufferCompute)
  {
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &m_cmdBufferCompute);
  }

  vkDestroyBuffer(m_device, m_A, nullptr);
  vkDestroyBuffer(m_device, m_B, nullptr);

  vkDestroyPipelineLayout(m_device, m_layout, nullptr);
  for (uint i = 0; i < s_shaderCount; i ++) {
    vkDestroyPipeline(m_device, m_pipelines[i], nullptr);
  }
}


void SimpleCompute::Cleanup()
{
  CleanupPipeline();

  if (m_commandPool != VK_NULL_HANDLE)
  {
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
  }
}


void SimpleCompute::CreateComputePipeline()
{
  VkShaderModule shaderModules[s_shaderCount] = {};
  VkPipelineShaderStageCreateInfo shaderStageCreateInfos[s_shaderCount] = {};
  std::array<const char*, s_shaderCount> filenames{"../resources/shaders/simple.comp.spv", "../resources/shaders/simple_with_shared.comp.spv"};

  for (uint i = 0; i < s_shaderCount; i++) {
    // Загружаем шейдер
    std::vector<uint32_t> code = vk_utils::readSPVFile(filenames[i]);
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pCode    = code.data();
    createInfo.codeSize = code.size()*sizeof(uint32_t);
      
    // Создаём шейдер в вулкане
    VK_CHECK_RESULT(vkCreateShaderModule(m_device, &createInfo, NULL, &shaderModules[i]));

    shaderStageCreateInfos[i].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfos[i].stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageCreateInfos[i].module = shaderModules[i];
    shaderStageCreateInfos[i].pName  = "main";
  }

  VkPushConstantRange pcRange = {};
  pcRange.offset = 0;
  pcRange.size = sizeof(m_length);
  pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  // Создаём layout для pipeline
  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
  pipelineLayoutCreateInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCreateInfo.setLayoutCount = 1;
  pipelineLayoutCreateInfo.pSetLayouts    = &m_sumDSLayout;
  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges = &pcRange;
  VK_CHECK_RESULT(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, NULL, &m_layout));

  VkComputePipelineCreateInfo pipelineCreateInfos[s_shaderCount] = {};
  for (uint i = 0; i < s_shaderCount; i++) {
    pipelineCreateInfos[i].sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineCreateInfos[i].stage  = shaderStageCreateInfos[i];
    pipelineCreateInfos[i].layout = m_layout;
  }

  // Создаём pipeline - объект, который выставляет шейдер и его параметры
  VK_CHECK_RESULT(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 2, pipelineCreateInfos, NULL, m_pipelines));

  for (uint i = 0; i < s_shaderCount; i++) {
      vkDestroyShaderModule(m_device, shaderModules[i], nullptr);
  }
}


void SimpleCompute::GenerateData() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dis(-1000, 1000);

  m_data.resize(m_length);
  for (uint64_t i = 0; i < m_length; ++i) {
    m_data[i] = dis(gen);
  }
}


float SimpleCompute::CalcMean(const std::vector<float>& a_values) {
  float sum = 0;
  for (uint i = 0; i < a_values.size(); i++) {
    sum += a_values[i];
  }
  return sum / a_values.size();
}

std::vector<float> SimpleCompute::CPUExecution() {
  std::vector<float> smooth(m_length);
  for (int i = 0; i < m_length; i++) {
    float sum = 0;
    for (int j = -m_w_r; j <= m_w_r; j++) {
      sum += (i + j >= 0 && i + j < m_length) ? m_data[i+j] : 0;
    }
    smooth[i] = m_data[i] - sum / (m_w_r*2+1);
  }
  return smooth;
}

std::vector<float> SimpleCompute::ReadResult() {
  std::vector<float> values(m_length);
  m_pCopyHelper->ReadBuffer(m_B, 0, values.data(), sizeof(float) * values.size());
  return values;
}


void SimpleCompute::Execute()
{
  GenerateData();

  SetupSimplePipeline();
  CreateComputePipeline();

  VkFenceCreateInfo fenceCreateInfo = {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = 0;
  VK_CHECK_RESULT(vkCreateFence(m_device, &fenceCreateInfo, NULL, &m_fence));

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &m_cmdBufferCompute;

  // Time
  std::chrono::steady_clock clock;

  int total_time[s_shaderCount+1] = {0};
  int runs = 10;
  std::array<const char*, s_shaderCount+1> names{"GPU, no shared", "GPU, shared", "CPU"};

  std::array<std::vector<float>, s_shaderCount+1> results = {};

  for (int k = 0; k < runs; k++) {
    { // CPU
      auto before = clock.now();
      auto result = CPUExecution();
      auto after = clock.now();
      total_time[s_shaderCount] += std::chrono::duration_cast<std::chrono::microseconds>(after-before).count();
      if (k == runs - 1) {
        results[s_shaderCount] = result;
      }

    }
    // GPU два варианта
    for (uint i = 0; i < s_shaderCount; i++){
      BuildCommandBufferSimple(m_cmdBufferCompute, m_pipelines[i]);
      auto before = clock.now();
      // Отправляем буфер команд на выполнение
      VK_CHECK_RESULT(vkQueueSubmit(m_computeQueue, 1, &submitInfo, m_fence));
      //Ждём конца выполнения команд
      VK_CHECK_RESULT(vkWaitForFences(m_device, 1, &m_fence, VK_TRUE, 100000000000));
      auto after = clock.now();
      vkResetFences(m_device, 1, &m_fence);
      total_time[i] += std::chrono::duration_cast<std::chrono::microseconds>(after-before).count();
      if (k == runs - 1) {
        results[i] = ReadResult();
      }
    }
  }
  std::cout << runs << " runs, " << m_groupSize << " work group size\n";
  for (int i = 0; i < names.size(); i++) {
        std::cout <<std::setw(14) << names[i] << ": last output="  << std::setprecision(8) << CalcMean(results[i]) 
                << " time=" << total_time[i]/runs  << " μs"
                <<std::endl;
  }

}
