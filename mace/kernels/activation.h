//
// Copyright (c) 2017 XiaoMi All rights reserved.
//

#ifndef MACE_KERNELS_ACTIVATION_H_
#define MACE_KERNELS_ACTIVATION_H_

#include <algorithm>
#include <string>
#include <vector>

#include "mace/core/future.h"
#include "mace/core/runtime/opencl/cl2_header.h"
#include "mace/core/tensor.h"
#include "mace/core/types.h"

namespace mace {
namespace kernels {

enum ActivationType {
  NOOP = 0,
  RELU = 1,
  RELUX = 2,
  PRELU = 3,
  TANH = 4,
  SIGMOID = 5
};

inline ActivationType StringToActivationType(const std::string type) {
  if (type == "RELU") {
    return ActivationType::RELU;
  } else if (type == "RELUX") {
    return ActivationType::RELUX;
  } else if (type == "PRELU") {
    return ActivationType::PRELU;
  } else if (type == "TANH") {
    return ActivationType::TANH;
  } else if (type == "SIGMOID") {
    return ActivationType::SIGMOID;
  } else if (type == "NOOP") {
    return ActivationType::NOOP;
  } else {
    LOG(FATAL) << "Unknown activation type: " << type;
  }
  return ActivationType::NOOP;
}

template <typename T>
void DoActivation(const T *input_ptr,
                  T *output_ptr,
                  const index_t size,
                  const ActivationType type,
                  const float relux_max_limit) {
  MACE_CHECK(DataTypeToEnum<T>::value != DataType::DT_HALF);

  switch (type) {
    case NOOP:
      break;
    case RELU:
#pragma omp parallel for
      for (index_t i = 0; i < size; ++i) {
        output_ptr[i] = std::max(input_ptr[i], static_cast<T>(0));
      }
      break;
    case RELUX:
#pragma omp parallel for
      for (index_t i = 0; i < size; ++i) {
        output_ptr[i] = std::min(std::max(input_ptr[i], static_cast<T>(0)),
                                 static_cast<T>(relux_max_limit));
      }
      break;
    case TANH:
#pragma omp parallel for
      for (index_t i = 0; i < size; ++i) {
        T in_exp = std::exp(-2 * input_ptr[i]);
        output_ptr[i] = (1 - in_exp) / (1 + in_exp);
      }
      break;
    case SIGMOID:
#pragma omp parallel for
      for (index_t i = 0; i < size; ++i) {
        output_ptr[i] = 1 / (1 + std::exp(-input_ptr[i]));
      }
      break;
    default:
      LOG(FATAL) << "Unknown activation type: " << type;
  }
}

template <typename T>
void PReLUActivation(const T *input_ptr,
                     const index_t outer_size,
                     const index_t input_chan,
                     const index_t inner_size,
                     const T *alpha_ptr,
                     T *output_ptr) {
#pragma omp parallel for collapse(3)
  for (index_t i = 0; i < outer_size; ++i) {
    for (index_t chan_idx = 0; chan_idx < input_chan; ++chan_idx) {
      for (index_t j = 0; j < inner_size; ++j) {
        index_t idx = i * input_chan * inner_size + chan_idx * inner_size + j;
        if (input_ptr[idx] < 0) {
          output_ptr[idx] = input_ptr[idx] * alpha_ptr[chan_idx];
        } else {
          output_ptr[idx] = input_ptr[idx];
        }
      }
    }
  }
}

template <DeviceType D, typename T>
class ActivationFunctor {
 public:
  ActivationFunctor(ActivationType type, T relux_max_limit)
      : activation_(type), relux_max_limit_(relux_max_limit) {}

  void operator()(const Tensor *input,
                  const Tensor *alpha,
                  Tensor *output,
                  StatsFuture *future) {
    const T *input_ptr = input->data<T>();
    T *output_ptr = output->mutable_data<T>();
    if (activation_ == PRELU) {
      MACE_CHECK_NOTNULL(alpha);
      const T *alpha_ptr = alpha->data<T>();
      const index_t outer_size = output->dim(0) * output->dim(1)
          * output->dim(2);
      PReLUActivation(input_ptr, outer_size, input->dim(3), 1, alpha_ptr,
                      output_ptr);
    } else {
      DoActivation(input_ptr, output_ptr, output->size(), activation_,
                   relux_max_limit_);
    }
  }

 private:
  ActivationType activation_;
  T relux_max_limit_;
};

template <>
class ActivationFunctor<DeviceType::NEON, float> {
 public:
  ActivationFunctor(ActivationType type, float relux_max_limit)
    : activation_(type), relux_max_limit_(relux_max_limit) {}

  void operator()(const Tensor *input,
                  const Tensor *alpha,
                  Tensor *output,
                  StatsFuture *future);

 private:
  ActivationType activation_;
  float relux_max_limit_;
};

template <typename T>
class ActivationFunctor<DeviceType::OPENCL, T> {
 public:
  ActivationFunctor(ActivationType type, T relux_max_limit)
      : activation_(type), relux_max_limit_(static_cast<T>(relux_max_limit)) {}

  void operator()(const Tensor *input,
                  const Tensor *alpha,
                  Tensor *output,
                  StatsFuture *future);

 private:
  ActivationType activation_;
  T relux_max_limit_;
  cl::Kernel kernel_;
  uint32_t kwg_size_;
  std::string tuning_key_prefix_;
  std::vector<index_t> input_shape_;
};

}  // namespace kernels
}  // namespace mace

#endif  // MACE_KERNELS_ACTIVATION_H_
