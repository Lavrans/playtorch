/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <jsi/jsi.h>

#include <ATen/Functions.h>
#include <torch/script.h>
#include <string>

#include "../torch/utils/helpers.h"
#include "TransformsHostObject.h"

// Namespace alias for torch to avoid namespace conflicts with torchlive::torch
namespace torch_ = torch;

namespace torchlive {
namespace vision {
namespace transforms {

using namespace facebook;

static inline std::array<int, 2> getImageSize(torch_::Tensor& tensor) {
  auto sizes = tensor.sizes();
  auto length = sizes.size();
  std::array<int, 2> size;
  size[0] = sizes[length - 1];
  size[1] = sizes[length - 2];
  return size;
}

// TransformsHostObject Method Name
static const std::string CENTER_CROP = "centerCrop";

// TransformsHostObject Property Names
// empty

// TransformsHostObject Properties
// empty
static const std::vector<std::string> PROPERTIES = {};

// TransformsHostObject Methods
// empty
const std::vector<std::string> METHODS = {};

TransformsHostObject::TransformsHostObject(jsi::Runtime& runtime)
    : centerCrop_(createCenterCrop(runtime)) {}

std::vector<jsi::PropNameID> TransformsHostObject::getPropertyNames(
    jsi::Runtime& rt) {
  std::vector<jsi::PropNameID> result;
  for (std::string property : PROPERTIES) {
    result.push_back(jsi::PropNameID::forUtf8(rt, property));
  }
  for (std::string method : METHODS) {
    result.push_back(jsi::PropNameID::forUtf8(rt, method));
  }
  return result;
}

jsi::Value TransformsHostObject::get(
    jsi::Runtime& runtime,
    const jsi::PropNameID& propName) {
  auto name = propName.utf8(runtime);

  if (name == CENTER_CROP) {
    return jsi::Value(runtime, centerCrop_);
  }

  return jsi::Value::undefined();
}

/**
 * Crops the given torch Tensor at the center, it is expected to have […, H, W]
 * shape, where … means an arbitrary number of leading dimensions.
 *
 * TODO(T112483016) If image size is smaller than output size along any edge,
 * image is padded with 0 and then center cropped.
 *
 * Original function:
 * https://github.com/pytorch/vision/blob/main/torchvision/transforms/functional.py#L515-L553
 */
jsi::Function TransformsHostObject::createCenterCrop(jsi::Runtime& runtime) {
  auto centerCropFactoryFunc = [](jsi::Runtime& runtime,
                                  const jsi::Value& thisValue,
                                  const jsi::Value* arguments,
                                  size_t count) -> jsi::Value {
    int width = -1;
    int height = -1;
    if (count > 0) {
      width = arguments[0].asNumber();
    }
    if (count > 1) {
      height = arguments[1].asNumber();
    }

    auto centerCropFunc = [width, height](
                              jsi::Runtime& innerRuntime,
                              const jsi::Value& innerThisValue,
                              const jsi::Value* innerArguments,
                              size_t innerCount) -> jsi::Value {
      if (innerCount != 1) {
        throw jsi::JSError(innerRuntime, "Tensor required as argument");
      }

      auto tensorHostObject =
          utils::helpers::parseTensor(innerRuntime, &innerArguments[0]);
      auto tensor = tensorHostObject->tensor;

      // Get the size of the image tensor -> […, H, W]
      auto size = getImageSize(tensor);

      auto cropHeight = height;
      auto cropWidth = width;
      if (cropWidth == -1) {
        auto minSize = std::min(size[0], size[1]);
        cropHeight = minSize;
        cropWidth = minSize;
      } else if (cropHeight == -1) {
        cropHeight = cropWidth;
      }

      auto cropTop = (size[1] - cropHeight) / 2;
      auto cropLeft = (size[0] - cropWidth) / 2;

      // Crop image tensor by narrowing the tensor along the last two
      // dimensions.
      auto dims = tensor.ndimension();
      tensor = tensor.narrow(dims - 2, cropTop, cropHeight)
                   .narrow(dims - 1, cropLeft, cropWidth);

      auto centerCroppedTensorHostObject =
          std::make_shared<torchlive::torch::TensorHostObject>(
              innerRuntime, tensor);
      return jsi::Object::createFromHostObject(
          innerRuntime, centerCroppedTensorHostObject);
    };

    return jsi::Function::createFromHostFunction(
        runtime,
        jsi::PropNameID::forUtf8(
            runtime,
            "CenterCrop(" + std::to_string(width) + ", " +
                std::to_string(height) + ")"),
        1,
        centerCropFunc);
  };

  return jsi::Function::createFromHostFunction(
      runtime,
      jsi::PropNameID::forUtf8(runtime, CENTER_CROP),
      1,
      centerCropFactoryFunc);
}

} // namespace transforms
} // namespace vision
} // namespace torchlive
