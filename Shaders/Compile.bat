@echo Compile shaders ...
@rem
@rem C:\VulkanSDK\1.3.211.0\Bin\glslc SimpleTriangle.vert.glsl -o vert.spv
@rem C:\VulkanSDK\1.3.211.0\Bin\glslc SimpleTriangle.frag.glsl -o frag.spv
C:\VulkanSDK\1.3.211.0\Bin\glslangValidator -V SimpleTriangle.vert.glsl -o SimpleTriangle.vert.spv
C:\VulkanSDK\1.3.211.0\Bin\glslangValidator -V SimpleTriangle.frag.glsl -o SimpleTriangle.frag.spv
@pause