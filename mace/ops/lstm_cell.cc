// Copyright 2018 Xiaomi, Inc.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>
#include <memory>

#include "mace/core/operator.h"
#include "mace/ops/opencl/image/lstm_cell.h"

namespace mace {
namespace ops {

template <DeviceType D, class T>
class LSTMCellOp;

template <typename T>
class LSTMCellOp<DeviceType::GPU, T> : public Operation {
 public:
  explicit LSTMCellOp(OpConstructContext *context)
      : Operation(context) {
    T forget_bias = static_cast<T>(
                     Operation::GetOptionalArg<float>("scalar_input",
                                                         0.0));
    if (context->device()->opencl_runtime()->UseImageMemory()) {
      kernel_.reset(new opencl::image::LSTMCellKernel<T>(forget_bias));
    } else {
      MACE_NOT_IMPLEMENTED;
    }
  }

  MaceStatus Run(OpContext *context) override {
    const Tensor *input = this->Input(INPUT);
    const Tensor *pre_output = this->Input(PRE_OUTPUT);
    const Tensor *weight = this->Input(WEIGHT);
    const Tensor *bias = this->Input(BIAS);
    const Tensor *pre_cell = this->Input(PRE_CELL);
    Tensor *cell = this->Output(CELL);
    Tensor *output = this->Output(OUTPUT);
    return kernel_->Compute(context, input, pre_output, weight, bias,
                            pre_cell, cell, output);
  }

 private:
  std::unique_ptr<OpenCLLSTMCellKernel> kernel_;

  MACE_OP_INPUT_TAGS(INPUT, PRE_OUTPUT, WEIGHT, BIAS, PRE_CELL);
  MACE_OP_OUTPUT_TAGS(CELL, OUTPUT);
};

void RegisterLSTMCell(OpRegistryBase *op_registry) {
  MACE_REGISTER_OP(op_registry, "LSTMCell", LSTMCellOp,
                   DeviceType::GPU, float);

  MACE_REGISTER_OP(op_registry, "LSTMCell", LSTMCellOp,
                   DeviceType::GPU, half);
}

}  // namespace ops
}  // namespace mace
