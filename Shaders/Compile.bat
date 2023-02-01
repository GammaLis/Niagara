@echo Compile shaders ...
@rem
@rem C:\VulkanSDK\1.3.236.0\Bin\glslc SimpleTriangle.vert.glsl -o vert.spv
@rem C:\VulkanSDK\1.3.236.0\Bin\glslc SimpleTriangle.frag.glsl -o frag.spv

@set VULKAN_BIN=C:\VulkanSDK\1.3.236.0\Bin

%VULKAN_BIN%\glslangValidator DrawCommand.comp.glsl -V --target-env vulkan1.3 -o ../Src/CompiledShaders/DrawCommand.comp.spv
%VULKAN_BIN%\glslangValidator HiZBuild.comp.glsl -V --target-env vulkan1.3 -o ../Src/CompiledShaders/HiZBuild.comp.spv

%VULKAN_BIN%\glslangValidator SimpleMesh.vert.glsl -V --target-env vulkan1.3 -o ../Src/CompiledShaders/SimpleMesh.vert.spv
%VULKAN_BIN%\glslangValidator SimpleMesh.task.glsl -V --target-env vulkan1.3 -o ../Src/CompiledShaders/SimpleMesh.task.spv
%VULKAN_BIN%\glslangValidator SimpleMesh.mesh.glsl -V --target-env vulkan1.3 -o ../Src/CompiledShaders/SimpleMesh.mesh.spv
%VULKAN_BIN%\glslangValidator SimpleMesh.frag.glsl -V --target-env vulkan1.3 -o ../Src/CompiledShaders/SimpleMesh.frag.spv

@pause
