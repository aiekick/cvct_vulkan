@for %%I IN (*.vert; *.tesc; *.tese; *.geom; *.frag; *.comp; *.def) DO (%VULKAN_SDK%/Bin32/glslangValidator.exe -V "%%I" -o "%%~nI%%~xI.spv")
pause