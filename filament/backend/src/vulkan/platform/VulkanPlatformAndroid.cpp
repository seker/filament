/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <backend/platforms/VulkanPlatform.h>

#include <backend/DriverEnums.h>
#include <backend/platforms/VulkanPlatformAndroid.h>
#include <private/backend/BackendUtilsAndroid.h>

#include "vulkan/VulkanConstants.h"

#include <utils/Panic.h>

#include <bluevk/BlueVK.h>

#include <android/hardware_buffer.h>
#include <android/native_window.h>

#include <utility>

using namespace bluevk;

namespace filament::backend {

namespace {

VkFormat transformVkFormat(VkFormat format, bool sRGB) {
    if (!sRGB) {
        return format;
    }

    switch (format) {
        case VK_FORMAT_R8G8B8A8_UNORM:
            format = VK_FORMAT_R8G8B8A8_SRGB;
            break;
        case VK_FORMAT_R8G8B8_UNORM:
            format = VK_FORMAT_R8G8B8_SRGB;
            break;
        default:
            break;
    }

    return format;
}

bool isProtectedFromUsage(uint64_t usage) {
    return (usage & AHARDWAREBUFFER_USAGE_PROTECTED_CONTENT) ? true : false;
}

std::pair<VkFormat, VkImageUsageFlags> getVKFormatAndUsage(const AHardwareBuffer_Desc& desc,
        bool sRGB) {
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags usage = 0;
    // Refer to "11.2.17. External Memory Handle Types" in the spec, and
    // Tables 13/14 for how the following derivation works.
    bool isDepthFormat = false;
    switch (desc.format) {
        case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
            format = VK_FORMAT_R8G8B8A8_UNORM;
            break;
        case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM:
            format = VK_FORMAT_R8G8B8A8_UNORM;
            break;
        case AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM:
            format = VK_FORMAT_R8G8B8_UNORM;
            break;
        case AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM:
            format = VK_FORMAT_R5G6B5_UNORM_PACK16;
            break;
        case AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT:
            format = VK_FORMAT_R16G16B16A16_SFLOAT;
            break;
        case AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM:
            format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
            break;
        case AHARDWAREBUFFER_FORMAT_D16_UNORM:
            isDepthFormat = true;
            format = VK_FORMAT_D16_UNORM;
            break;
        case AHARDWAREBUFFER_FORMAT_D24_UNORM:
            isDepthFormat = true;
            format = VK_FORMAT_X8_D24_UNORM_PACK32;
            break;
        case AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT:
            isDepthFormat = true;
            format = VK_FORMAT_D24_UNORM_S8_UINT;
            break;
        case AHARDWAREBUFFER_FORMAT_D32_FLOAT:
            isDepthFormat = true;
            format = VK_FORMAT_D32_SFLOAT;
            break;
        case AHARDWAREBUFFER_FORMAT_D32_FLOAT_S8_UINT:
            isDepthFormat = true;
            format = VK_FORMAT_D32_SFLOAT_S8_UINT;
            break;
        case AHARDWAREBUFFER_FORMAT_S8_UINT:
            isDepthFormat = true;
            format = VK_FORMAT_S8_UINT;
            break;
        default:
            format = VK_FORMAT_UNDEFINED;
    }

    format = transformVkFormat(format, sRGB);

    // The following only concern usage flags derived from Table 14.
    usage = 0;
    if (desc.usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE) {
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    }
    if (desc.usage & AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER) {
        if (isDepthFormat) {
            usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        } else {
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
    }
    if (desc.usage & AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER) {
        usage = VK_IMAGE_USAGE_STORAGE_BIT;
    }

    return { format, usage };
}

VulkanPlatform::ImageData allocateExternalImage(AHardwareBuffer* buffer, VkDevice device,
        VulkanPlatform::ExternalImageMetadata const& metadata) {
    VulkanPlatform::ImageData data;

    // if external format we need to specifiy it in the allocation
    const bool useExternalFormat = metadata.format == VK_FORMAT_UNDEFINED;

    const VkExternalFormatANDROID externalFormat = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID,
        .pNext = nullptr,
        .externalFormat = metadata
                .externalFormat,// pass down the format (external means we don't have it VK defined)
    };
    const VkExternalMemoryImageCreateInfo externalCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .pNext = useExternalFormat ? &externalFormat : nullptr,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
    };

    VkImageCreateInfo imageInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.pNext = &externalCreateInfo;
    imageInfo.format = metadata.format;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {
        metadata.width,
        metadata.height,
        1u,
    };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = metadata.layers;
    imageInfo.usage = metadata.usage;
    // In the unprotected case add R/W capabilities
    if (metadata.isProtected == false)
        imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkResult result = vkCreateImage(device, &imageInfo, VKALLOC, &data.first);
    FILAMENT_CHECK_POSTCONDITION(result == VK_SUCCESS)
            << "vkCreateImage failed with error=" << static_cast<int32_t>(result);

    // Allocate the memory
    VkImportAndroidHardwareBufferInfoANDROID androidHardwareBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
        .pNext = nullptr,
        .buffer = buffer,
    };
    VkMemoryDedicatedAllocateInfo memoryDedicatedAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .pNext = &androidHardwareBufferInfo,
        .image = data.first,
        .buffer = VK_NULL_HANDLE,
    };
    VkMemoryAllocateInfo allocInfo = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &memoryDedicatedAllocateInfo,
        .allocationSize = metadata.allocationSize,
        .memoryTypeIndex = metadata.memoryTypeBits};
    result = vkAllocateMemory(device, &allocInfo, VKALLOC, &data.second);
    FILAMENT_CHECK_POSTCONDITION(result == VK_SUCCESS)
            << "vkAllocateMemory failed with error=" << static_cast<int32_t>(result);

    return data;
}

} // namespace

namespace fvkandroid {

ExternalImageVulkanAndroid::~ExternalImageVulkanAndroid() = default;

Platform::ExternalImageHandle createExternalImage(AHardwareBuffer const* buffer,
        bool sRGB) noexcept {
    if (__builtin_available(android 26, *)) {
        AHardwareBuffer_Desc hardwareBufferDescription = {};
        AHardwareBuffer_describe(buffer, &hardwareBufferDescription);

        auto* const p = new (std::nothrow) ExternalImageVulkanAndroid;
        p->aHardwareBuffer = const_cast<AHardwareBuffer*>(buffer);
        p->sRGB = sRGB;
        p->height = hardwareBufferDescription.height;
        p->width = hardwareBufferDescription.width;
        TextureFormat textureFormat = mapToFilamentFormat(hardwareBufferDescription.format, sRGB);
        p->format = textureFormat;
        p->usage = mapToFilamentUsage(hardwareBufferDescription.usage, textureFormat);
        return Platform::ExternalImageHandle{ p };
    }

    return Platform::ExternalImageHandle{};
}

} // namespace fvkandroid

VulkanPlatform::ExternalImageMetadata VulkanPlatform::getExternalImageMetadataImpl(
        ExternalImageHandleRef externalImage, VkDevice device) {
    auto const* fvkExternalImage =
            static_cast<fvkandroid::ExternalImageVulkanAndroid const*>(externalImage.get());

    ExternalImageMetadata metadata;
    AHardwareBuffer* buffer = fvkExternalImage->aHardwareBuffer;
    if (__builtin_available(android 26, *)) {
        AHardwareBuffer_Desc bufferDesc;
        AHardwareBuffer_describe(buffer, &bufferDesc);
        metadata.width = bufferDesc.width;
        metadata.height = bufferDesc.height;
        metadata.layers = bufferDesc.layers;
        metadata.isProtected = isProtectedFromUsage(bufferDesc.usage);
        std::tie(metadata.format, metadata.usage) =
                getVKFormatAndUsage(bufferDesc, fvkExternalImage->sRGB);
    }
    // In the unprotected case add R/W capabilities
    if (metadata.isProtected == false) {
        metadata.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    VkAndroidHardwareBufferFormatPropertiesANDROID formatInfo = {
        .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID,
        .pNext = nullptr,
    };
    VkAndroidHardwareBufferPropertiesANDROID properties = {
        .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,
        .pNext = &formatInfo,
    };
    VkResult result = vkGetAndroidHardwareBufferPropertiesANDROID(device, buffer, &properties);
    FILAMENT_CHECK_POSTCONDITION(result == VK_SUCCESS)
        << "vkGetAndroidHardwareBufferProperties failed with error="
        << static_cast<int32_t>(result);
                            
    VkFormat bufferPropertiesFormat = transformVkFormat(formatInfo.format, fvkExternalImage->sRGB);
    FILAMENT_CHECK_POSTCONDITION(metadata.format == bufferPropertiesFormat)
            << "mismatched image format( " << metadata.format << ") and queried format("
            << bufferPropertiesFormat << ") for external image (AHB)";
    metadata.externalFormat = formatInfo.externalFormat;
    metadata.allocationSize = properties.allocationSize;
    metadata.memoryTypeBits = properties.memoryTypeBits;
    return metadata;
}

VulkanPlatform::ImageData VulkanPlatform::createExternalImageDataImpl(
        ExternalImageHandleRef externalImage, VkDevice device,
        const ExternalImageMetadata& metadata) {
    auto const* fvkExternalImage =
            static_cast<fvkandroid::ExternalImageVulkanAndroid const*>(externalImage.get());
    ImageData data = allocateExternalImage(fvkExternalImage->aHardwareBuffer, device, metadata);
    VkResult result = vkBindImageMemory(device, data.first, data.second, 0);
    FILAMENT_CHECK_POSTCONDITION(result == VK_SUCCESS)
            << "vkBindImageMemory error=" << static_cast<int32_t>(result);
    return data;
}

VulkanPlatform::ExtensionSet VulkanPlatform::getSwapchainInstanceExtensions() {
    return {
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
    };
}

VulkanPlatform::SurfaceBundle VulkanPlatform::createVkSurfaceKHR(void* nativeWindow,
    VkInstance instance, uint64_t flags) noexcept {
    VkSurfaceKHR surface;
    VkExtent2D extent;

    VkAndroidSurfaceCreateInfoKHR const createInfo{
        .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
        .window = (ANativeWindow*) nativeWindow,
    };
    VkResult const result =
            vkCreateAndroidSurfaceKHR(instance, &createInfo, VKALLOC, (VkSurfaceKHR*) &surface);
    FILAMENT_CHECK_POSTCONDITION(result == VK_SUCCESS)
            << "vkCreateAndroidSurfaceKHR with error=" << static_cast<int32_t>(result);
    return {surface, extent};
}
}
