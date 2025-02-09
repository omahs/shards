#include "../gfx.hpp"
#include <gfx/texture.hpp>
#include <gfx/error_utils.hpp>
#include <params.hpp>
#include <stdexcept>

using namespace shards;
namespace gfx {

enum ComponentType {
  Float,
  Int8,
  Int16,
};

struct TextureFormatException : public std::runtime_error {
  TextureFormatException(ComponentType componentType, Types::TextureType_ asType)
      : std::runtime_error(formatError(componentType, asType)) {}

  static std::string formatError(ComponentType componentType, Types::TextureType_ asType) {
    return fmt::format("Image with component type '{}' can not be converted to texture type '{}'",
                       magic_enum::enum_name(componentType), magic_enum::enum_name(asType));
  }
};

struct TextureShard {
  static inline shards::Types InputTypes{{CoreInfo::ImageType}};
  static SHTypesInfo inputTypes() { return InputTypes; }
  static SHTypesInfo outputTypes() { return Types::Texture; }
  static SHOptionalString help() { return SHCCSTR("Creates a texture from an image"); }

  TexturePtr texture;
  OwnedVar textureVar;

  PARAM_VAR(asType_, "Type", "Type to interpret image data as. (Default: UNormSRGB for RGBA8 images, UNorm for other formats)",
            {Types::TextureType});
  PARAM_IMPL(TextureShard, PARAM_IMPL_FOR(asType_));

  TextureShard() {
    asType_ = Var::Enum(Types::TextureType_::Default, SHTypeInfo(Types::TextureType).enumeration.vendorId,
                        SHTypeInfo(Types::TextureType).enumeration.typeId);
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup() { PARAM_CLEANUP(); }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    using TextureType = Types::TextureType_;

    auto &image = input.payload.imageValue;

    if (!texture)
      texture = std::make_shared<Texture>();

    textureVar.valueType = SHType::Object;
    textureVar.payload.objectTypeId = Types::TextureTypeId;
    textureVar.payload.objectVendorId = gfx::VendorId;
    textureVar.payload.objectValue = &texture;

    ComponentType componentType;
    TextureType asType;
    if (image.flags & SHIMAGE_FLAGS_32BITS_FLOAT) {
      asType = TextureType::Float;
      componentType = ComponentType::Float;
    } else if (image.flags & SHIMAGE_FLAGS_16BITS_INT) {
      asType = TextureType::UInt;
      componentType = ComponentType::Int16;
    } else {
      componentType = ComponentType::Int8;
      if (image.channels == 4)
        asType = TextureType::UNormSRGB;
      else
        asType = TextureType::UNorm;
    }

    TextureType paramAsType = TextureType(asType_.payload.enumValue);
    if (paramAsType != TextureType::Default) {
      asType = paramAsType;
    }

    TextureFormat format{};
    switch (image.channels) {
    case 1:
      switch (componentType) {
      case ComponentType::Float:
        if (asType == TextureType::Float)
          format.pixelFormat = WGPUTextureFormat_R32Float;
        break;
      case ComponentType::Int16:
        if (asType == TextureType::UInt)
          format.pixelFormat = WGPUTextureFormat_R16Uint;
        else if (asType == TextureType::Int)
          format.pixelFormat = WGPUTextureFormat_R16Sint;
        break;
      case ComponentType::Int8:
        if (asType == TextureType::UNorm)
          format.pixelFormat = WGPUTextureFormat_R8Unorm;
        else if (asType == TextureType::SNorm)
          format.pixelFormat = WGPUTextureFormat_R8Snorm;
        else if (asType == TextureType::Int)
          format.pixelFormat = WGPUTextureFormat_R8Uint;
        else if (asType == TextureType::Int)
          format.pixelFormat = WGPUTextureFormat_R8Sint;
        break;
      }
      break;
    case 2:
      switch (componentType) {
      case ComponentType::Float:
        if (asType == TextureType::Float)
          format.pixelFormat = WGPUTextureFormat_RG32Float;
        break;
      case ComponentType::Int16:
        if (asType == TextureType::UInt)
          format.pixelFormat = WGPUTextureFormat_RG16Uint;
        else if (asType == TextureType::Int)
          format.pixelFormat = WGPUTextureFormat_RG16Sint;
        break;
      case ComponentType::Int8:
        if (asType == TextureType::UNorm)
          format.pixelFormat = WGPUTextureFormat_RG8Unorm;
        else if (asType == TextureType::SNorm)
          format.pixelFormat = WGPUTextureFormat_RG8Snorm;
        else if (asType == TextureType::Int)
          format.pixelFormat = WGPUTextureFormat_RG8Uint;
        else if (asType == TextureType::Int)
          format.pixelFormat = WGPUTextureFormat_RG8Sint;
        break;
      }
      break;
    case 3:
      throw formatException("RGB textures not supported");
    case 4:
      switch (componentType) {
      case ComponentType::Float:
        if (asType == TextureType::Float)
          format.pixelFormat = WGPUTextureFormat_RGBA32Float;
        break;
      case ComponentType::Int16:
        if (asType == TextureType::UInt)
          format.pixelFormat = WGPUTextureFormat_RGBA16Uint;
        else if (asType == TextureType::Int)
          format.pixelFormat = WGPUTextureFormat_RGBA16Sint;
        break;
      case ComponentType::Int8:
        if (asType == TextureType::UNorm)
          format.pixelFormat = WGPUTextureFormat_RGBA8Unorm;
        else if (asType == TextureType::UNormSRGB)
          format.pixelFormat = WGPUTextureFormat_RGBA8UnormSrgb;
        else if (asType == TextureType::SNorm)
          format.pixelFormat = WGPUTextureFormat_RGBA8Snorm;
        else if (asType == TextureType::UInt)
          format.pixelFormat = WGPUTextureFormat_RGBA8Uint;
        else if (asType == TextureType::Int)
          format.pixelFormat = WGPUTextureFormat_RGBA8Sint;
        break;
      }
      break;
    }

    if (format.pixelFormat == WGPUTextureFormat_Undefined)
      throw TextureFormatException(componentType, asType);

    auto &inputFormat = Texture::getInputFormat(format.pixelFormat);
    size_t imageSize = inputFormat.pixelSize * image.width * image.height;

    // Copy the data since we can't keep a reference to the image variable
    ImmutableSharedBuffer isb(image.data, imageSize);
    texture->init(format, int2(image.width, image.height), SamplerState(), std::move(isb));

    return textureVar;
  }
};

void registerTextureShards() { REGISTER_SHARD("GFX.Texture", TextureShard); }
} // namespace gfx
