# Subset of these gets commented out depending on STBI_NO_* macros defined
${STB_BMP_SUPPORT}provides=StbBmpImageConverter
${STB_HDR_SUPPORT}provides=StbHdrImageConverter
${STB_JPEG_SUPPORT}provides=StbJpegImageConverter
${STB_PNG_SUPPORT}provides=StbPngImageConverter
${STB_TGA_SUPPORT}provides=StbTgaImageConverter
${STB_BMP_SUPPORT}provides=BmpImageConverter
${STB_HDR_SUPPORT}provides=HdrImageConverter
${STB_JPEG_SUPPORT}provides=JpegImageConverter
${STB_PNG_SUPPORT}provides=PngImageConverter
${STB_TGA_SUPPORT}provides=TgaImageConverter

# [configuration_]
[configuration]
# Compression quality for JPEG output (0 - 1, 1 is the best). Corresponds to
# the same option in JpegImageConverter.
jpegQuality=0.8
# [configuration_]
