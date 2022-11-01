#include "simple_compute.h"

int main()
{
  constexpr int LENGTH = 4000000;
  constexpr int VULKAN_DEVICE_ID = 0; // Intel UHD Graphics 630

  std::shared_ptr<ICompute> app = std::make_unique<SimpleCompute>(LENGTH);
  if(app == nullptr)
  {
    std::cout << "Can't create render of specified type" << std::endl;
    return 1;
  }

  app->InitVulkan(nullptr, 0, VULKAN_DEVICE_ID);

  app->Execute();

  // Sample output:
  // 10 runs, 64 work group size
  // GPU, no shared: last output=0.00017694882 time=9036 μs
  //    GPU, shared: last output=0.00017695571 time=8689 μs
  //            CPU: last output=0.00017694899 time=137063 μs

  return 0;
}
