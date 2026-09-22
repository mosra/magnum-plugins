# Subset of these gets commented out depending on STBI_NO_* macros defined
${STB_BMP_SUPPORT}provides=BmpImporter
${STB_GIF_SUPPORT}provides=GifImporter
${STB_HDR_SUPPORT}provides=HdrImporter
${STB_JPEG_SUPPORT}provides=JpegImporter
${STB_PNM_SUPPORT}provides=PgmImporter
${STB_PIC_SUPPORT}provides=PicImporter
${STB_PNG_SUPPORT}provides=PngImporter
${STB_PNM_SUPPORT}provides=PpmImporter
${STB_PSD_SUPPORT}provides=PsdImporter
${STB_TGA_SUPPORT}provides=TgaImporter

# [configuration_]
[configuration]
# Override image channel count. Allowed values are 0-4, with zero keeping the
# original channel count. If set to something different than the image has,
# stb_image does a rather complex conversion. The main rules are the
# following, for precise behavior please see the stb_image source:
#
# - for a two- and four-channel output, the last channel is treated as alpha,
#   and either copied from the source (if it's two-/four-channel) or set to
#   255 / 65535 / 1.0f
# - reducing a three- or four-channel image to one or two channels will cause
#   the first channel to be filled with Y (luminance) of the RGB input, the
#   second (if any) channel is then alpha, filled according to the first rule
# - expanding a single- or two-channel image to three or four channels will
#   cause the first channel to be repeated three times and second channel (if
#   any) treated as alpha according to the first rule
forceChannelCount=0

# Override channel bit depth. Allowed values are 0, 8, 16 and 32, with zero
# keeping the original bit depth. Value of 32 imports the channels as 32-bit
# floating point values.
forceBitDepth=0
# [configuration_]
