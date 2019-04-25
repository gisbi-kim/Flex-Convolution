/* Copyright 2017 ComputerGraphics Tuebingen. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
// Authors: Fabian Groh, Patrick Wieschollek, Hendrik P.A. Lensch

#if GOOGLE_CUDA

#define EIGEN_USE_GPU

#include <cub/cub.cuh>
#include <limits>

#include "flex_pool_op.h"
#include "tensorflow/core/util/cuda_kernel_helper.h"

namespace {

#include "flex_pool_kernel_gpu_impl.cuh"



}  // namespace

namespace tensorflow {
namespace functor {

template <typename Dtype>
struct FlexPoolFunctor<GPUDevice, Dtype> {
  void operator()(::tensorflow::OpKernelContext* ctx, const Tensor& features_,
                  const Tensor& neighborhood_, Tensor* output_,
                  Tensor* argmax_) {
    // get dimensions
    const int B = neighborhood_.dim_size(0);
    const int K = neighborhood_.dim_size(1);
    const int N = neighborhood_.dim_size(2);
    const int D = features_.dim_size(1);

    const int threads = 32;
    dim3 block(threads, threads, 1);
    dim3 grid(up2(N, threads), up2(D, threads), B);

    forward<Dtype><<<grid, block>>>(
        B, N, K, D, features_.flat<Dtype>().data(),
        neighborhood_.flat<int>().data(), output_->flat<Dtype>().data(),
        argmax_->flat<int>().data(), std::numeric_limits<Dtype>::lowest());

    if (!ctx->eigen_gpu_device().ok()) {
      ctx->SetStatus(
          tensorflow::errors::Internal("CUDA: FlexPoolFunctor Error!"));
    }
  }
};

template struct FlexPoolFunctor<GPUDevice, float>;
template struct FlexPoolFunctor<GPUDevice, double>;

template <typename Dtype>
struct FlexPoolGrad<GPUDevice, Dtype> {
  void operator()(::tensorflow::OpKernelContext* ctx, const Tensor& features_,
                  const Tensor& neighborhood_, const Tensor& topdiff_,
                  const Tensor& argmax_, Tensor* grad_features_) {
    // get dimensions
    const int B = neighborhood_.dim_size(0);
    const int K = neighborhood_.dim_size(1);
    const int N = neighborhood_.dim_size(2);
    const int D = features_.dim_size(1);

    const int threads = 32;
    dim3 block(threads, threads, 1);
    dim3 grid(up2(N, threads), up2(D, threads), B);

    cudaMemset(grad_features_->flat<Dtype>().data(), 0,
               grad_features_->NumElements() * sizeof(Dtype));

    backward<Dtype><<<grid, block>>>(
        B, N, K, D, features_.flat<Dtype>().data(),
        neighborhood_.flat<int>().data(), topdiff_.flat<Dtype>().data(),
        argmax_.flat<int>().data(), grad_features_->flat<Dtype>().data());

    if (!ctx->eigen_gpu_device().ok()) {
      ctx->SetStatus(tensorflow::errors::Internal("CUDA: FlexPoolGrad Error!"));
    }
  }
};

template struct FlexPoolGrad<GPUDevice, float>;
template struct FlexPoolGrad<GPUDevice, double>;

}  // namespace functor
}  // namespace tensorflow

#endif  // GOOGLE_CUDA
