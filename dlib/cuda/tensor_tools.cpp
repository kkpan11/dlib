// Copyright (C) 2015  Davis E. King (davis@dlib.net)
// License: Boost Software License   See LICENSE.txt for the full license.
#ifndef DLIB_TeNSOR_TOOLS_CPP_
#define DLIB_TeNSOR_TOOLS_CPP_

#include "tensor_tools.h"
#include "../string.h"
#include <atomic>

namespace dlib
{
    namespace
    {
        std::atomic<bool>& dnn_prefer_fastest_algo (
        )
        {
            static std::atomic<bool> var(true);
            return var;
        }
    }

    bool dnn_prefer_fastest_algorithms (
    )
    {
        return dnn_prefer_fastest_algo();
    }

    void set_dnn_prefer_fastest_algorithms(
    )
    {
        dnn_prefer_fastest_algo() = true;
    }

    void set_dnn_prefer_smallest_algorithms(
    )
    {
        dnn_prefer_fastest_algo() = false;
    }
}

namespace dlib { namespace tt
{

// ----------------------------------------------------------------------------------------

    void inverse_norms (
        resizable_tensor& invnorms,
        const tensor& data,
        const double eps
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::inverse_norms(invnorms, data, eps);
#else
        invnorms = reciprocal(sqrt(sum_cols(squared(mat(data))) + eps));
#endif
    }

    void dot_prods (
        resizable_tensor& out,
        const tensor& lhs,
        const tensor& rhs
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::dot_prods(out, lhs, rhs);
#else
        out = sum_cols(pointwise_multiply(mat(lhs), mat(rhs))); 
#endif
    }

    void dot_prods (
        bool add_to,
        tensor& out,
        const tensor& lhs,
        const tensor& rhs
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::dot_prods(add_to, out, lhs, rhs);
#else
        if (add_to)
            out += sum_cols(pointwise_multiply(mat(lhs), mat(rhs))); 
        else
            out = sum_cols(pointwise_multiply(mat(lhs), mat(rhs))); 
#endif
    }

    void scale_columns (
        tensor& out,
        const tensor& m,
        const tensor& v
    )
    {
        DLIB_CASSERT(have_same_dimensions(out,m));
        DLIB_CASSERT(is_vector(v));
        if (m.size() == 0 && v.size() == 0)
            return;
        DLIB_CASSERT(m.size() != 0);
        DLIB_CASSERT(m.size()/m.num_samples() == v.size());

#ifdef DLIB_USE_CUDA
        cuda::scale_columns(out, m, v);
#else
        out = scale_columns(mat(m), mat(v));
#endif
    }

    void scale_rows (
        tensor& out,
        const tensor& m,
        const tensor& v
    )
    {
        DLIB_CASSERT(have_same_dimensions(out,m));
        DLIB_CASSERT(is_vector(v));
        if (m.size() == 0 && v.size() == 0)
            return;
        DLIB_CASSERT(m.size() != 0);
        DLIB_CASSERT(m.num_samples() == static_cast<long long>(v.size()));

#ifdef DLIB_USE_CUDA
        cuda::scale_rows(out, m, v);
#else
        out = scale_rows(mat(m), mat(v));
#endif
    }

    void scale_rows2 (
        float beta, 
        tensor& out,
        const tensor& m1,
        const tensor& m2,
        const tensor& v1,
        const tensor& v2
    )
    {
        DLIB_CASSERT(have_same_dimensions(out,m1));
        DLIB_CASSERT(have_same_dimensions(out,m2));
        DLIB_CASSERT(have_same_dimensions(v1,v2));
        DLIB_CASSERT(is_vector(mat(v1))); 
        DLIB_CASSERT(static_cast<long long>(v1.size()) == m1.num_samples());

#ifdef DLIB_USE_CUDA
        cuda::scale_rows2(beta, out, m1, m2, v1, v2);
#else
        if (beta == 0)
            out = scale_rows(mat(m1) - scale_rows(mat(m2),mat(v1)), mat(v2));
        else
            out = beta*mat(out) + scale_rows(mat(m1) - scale_rows(mat(m2),mat(v1)), mat(v2));
#endif
    }

// ----------------------------------------------------------------------------------------

    void exp (
        tensor& dest,
        const tensor& src
    )
    {
        DLIB_CASSERT(dest.size() == src.size());

#ifdef DLIB_USE_CUDA
        cuda::exp(dest,src);
#else
        dest = exp(mat(src));
#endif
    }

// ----------------------------------------------------------------------------------------

    void log (
        tensor& dest,
        const tensor& src
    )
    {
        DLIB_CASSERT(dest.size() == src.size());

#ifdef DLIB_USE_CUDA
        cuda::log(dest,src);
#else
        dest = log(mat(src));
#endif
    }

// ----------------------------------------------------------------------------------------

    void log10 (
        tensor& dest,
        const tensor& src
    )
    {
        DLIB_CASSERT(dest.size() == src.size());

#ifdef DLIB_USE_CUDA
        cuda::log10(dest,src);
#else
        dest = log10(mat(src));
#endif
    }

// ----------------------------------------------------------------------------------------

    void gemm (
        float beta,
        tensor& dest,
        float alpha,
        const tensor& lhs,
        bool trans_lhs,
        const tensor& rhs,
        bool trans_rhs,
        operation_mode mode
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::gemm(beta, dest, alpha, lhs, trans_lhs, rhs, trans_rhs, mode);
#else
        if (mode == operation_mode::CHANNEL_WISE)
        {
            if (beta != 0)
            {
                if (trans_lhs && trans_rhs)
                    dest = alpha * trans(mat(lhs)) * trans(mat(rhs)) + beta * mat(dest);
                else if (!trans_lhs && trans_rhs)
                    dest = alpha * mat(lhs) * trans(mat(rhs)) + beta * mat(dest);
                else if (trans_lhs && !trans_rhs)
                    dest = alpha * trans(mat(lhs)) * mat(rhs) + beta * mat(dest);
                else
                    dest = alpha * mat(lhs) * mat(rhs) + beta * mat(dest);
            }
            else
            {
                if (trans_lhs && trans_rhs)
                    dest = alpha * trans(mat(lhs)) * trans(mat(rhs));
                else if (!trans_lhs && trans_rhs)
                    dest = alpha * mat(lhs) * trans(mat(rhs));
                else if (trans_lhs && !trans_rhs)
                    dest = alpha * trans(mat(lhs)) * mat(rhs);
                else
                    dest = alpha * mat(lhs) * mat(rhs);
            }
        }
        else if (mode == operation_mode::PLANE_WISE)
        {
            auto is_matrix = [](const auto& tensor) {
                return ((tensor.num_samples() * tensor.k() == 1 && tensor.nr() * tensor.nc() > 1) ||
                    (tensor.num_samples() * tensor.k() > 1 && tensor.nr() * tensor.nc() == 1));
                };

            long num_samples = std::min({ lhs.num_samples(), rhs.num_samples(), dest.num_samples() });
            long num_channels = std::min({ lhs.k(), rhs.k(), dest.k() });
            const bool lhs_is_matrix = is_matrix(lhs), rhs_is_matrix = is_matrix(rhs), dest_is_matrix = is_matrix(dest);

            if (lhs_is_matrix && rhs_is_matrix && dest_is_matrix) {
                num_samples = num_channels = 1;
            }

            long lhs_rows = (lhs_is_matrix && lhs.num_samples() > 1) ? lhs.num_samples() : lhs.nr();
            long lhs_cols = (lhs_is_matrix && lhs.k() > 1) ? lhs.k() : lhs.nc();
            long rhs_rows = (rhs_is_matrix && rhs.num_samples() > 1) ? rhs.num_samples() : rhs.nr();
            long rhs_cols = (rhs_is_matrix && rhs.k() > 1) ? rhs.k() : rhs.nc();
            long dest_rows = (dest_is_matrix && dest.num_samples() > 1) ? dest.num_samples() : dest.nr();
            long dest_cols = (dest_is_matrix && dest.k() > 1) ? dest.k() : dest.nc();

            const size_t lhs_plane_size = lhs_rows * lhs_cols;
            const size_t rhs_plane_size = rhs_rows * rhs_cols;
            const size_t dest_plane_size = dest_rows * dest_cols;

            for (long b = 0; b < num_samples; ++b)
            {
                for (long c = 0; c < num_channels; ++c)
                {
                    auto lhs_slice = lhs_is_matrix ? alias_tensor(lhs_rows, lhs_cols)(lhs, 0) :
                        alias_tensor(lhs_rows, lhs_cols)(lhs, (b * num_channels + c) * lhs_plane_size);
                    auto rhs_slice = rhs_is_matrix ? alias_tensor(rhs_rows, rhs_cols)(rhs, 0) :
                        alias_tensor(rhs_rows, rhs_cols)(rhs, (b * num_channels + c) * rhs_plane_size);
                    auto dest_slice = dest_is_matrix ? alias_tensor(dest_rows, dest_cols)(dest, 0) :
                        alias_tensor(dest_rows, dest_cols)(dest, (b * num_channels + c) * dest_plane_size);

                    if (beta != 0)
                    {
                        if (trans_lhs && trans_rhs)
                            dest_slice = alpha * trans(mat(lhs_slice)) * trans(mat(rhs_slice)) + beta * mat(dest_slice);
                        else if (!trans_lhs && trans_rhs)
                            dest_slice = alpha * mat(lhs_slice) * trans(mat(rhs_slice)) + beta * mat(dest_slice);
                        else if (trans_lhs && !trans_rhs)
                            dest_slice = alpha * trans(mat(lhs_slice)) * mat(rhs_slice) + beta * mat(dest_slice);
                        else
                            dest_slice = alpha * mat(lhs_slice) * mat(rhs_slice) + beta * mat(dest_slice);
                    }
                    else
                    {
                        if (trans_lhs && trans_rhs)
                            dest_slice = alpha * trans(mat(lhs_slice)) * trans(mat(rhs_slice));
                        else if (!trans_lhs && trans_rhs)
                            dest_slice = alpha * mat(lhs_slice) * trans(mat(rhs_slice));
                        else if (trans_lhs && !trans_rhs)
                            dest_slice = alpha * trans(mat(lhs_slice)) * mat(rhs_slice);
                        else
                            dest_slice = alpha * mat(lhs_slice) * mat(rhs_slice);
                    }
                }
            }
        }
#endif
    }

// ----------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------

    tensor_rand::
    tensor_rand(
        unsigned long long seed
    ) 
#ifdef DLIB_USE_CUDA
    :rnd(seed){}
#else
    {rnd.set_seed(cast_to_string(seed)); }
#endif

    void tensor_rand::
    fill_gaussian (
        tensor& data,
        float mean,
        float stddev
    )
    {
        DLIB_CASSERT(data.size()%2 == 0);
#ifdef DLIB_USE_CUDA
        rnd.fill_gaussian(data, mean, stddev);
#else
        for (auto& x : data) 
            x = rnd.get_random_gaussian()*stddev + mean;
#endif
    }

    void tensor_rand::
    fill_uniform (
        tensor& data
    )
    {
#ifdef DLIB_USE_CUDA
        rnd.fill_uniform(data);
#else
        for (auto& x : data) 
            x = rnd.get_random_float();
#endif
    }

// ----------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------

    void multiply (
        bool add_to,
        tensor& dest,
        const tensor& src1,
        const tensor& src2
    )
    {
        DLIB_CASSERT(dest.k() == src1.k() && src1.k() == src2.k() &&
            dest.nr() == src1.nr() && src1.nr() == src2.nr() &&
            dest.nc() == src1.nc() && src1.nc() == src2.nc() );
        const long MD = std::max(std::max(dest.num_samples(),src1.num_samples()),src2.num_samples());
        DLIB_CASSERT((dest.num_samples()==1 || dest.num_samples()==MD) &&
                    (src1.num_samples()==1 || src1.num_samples()==MD) &&
                    (src2.num_samples()==1 || src2.num_samples()==MD) );
#ifdef DLIB_USE_CUDA
        cuda::multiply(add_to, dest, src1, src2);
#else
        cpu::multiply(add_to, dest, src1, src2);
#endif

    }

    void scale_channels (
        bool add_to,
        tensor& dest,
        const tensor& src,
        const tensor& scales
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::scale_channels(add_to, dest, src, scales);
#else
        cpu::scale_channels(add_to, dest, src, scales);
#endif
    }

    void multiply_conv (
        bool add_to,
        tensor& dest,
        const tensor& src1,
        const tensor& src2
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::multiply_conv(add_to, dest, src1, src2);
#else
        cpu::multiply_conv(add_to, dest, src1, src2);
#endif
    }

    void multiply_zero_padded (
        bool add_to,
        tensor& dest,
        const tensor& src1,
        const tensor& src2
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::multiply_zero_padded(add_to, dest, src1, src2);
#else
        cpu::multiply_zero_padded(add_to, dest, src1, src2);
#endif
    }

// ----------------------------------------------------------------------------------------

    void affine_transform(
        tensor& dest,
        const tensor& src,
        const float A,
        const float B
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::affine_transform(dest,src,A,B);
#else
        cpu::affine_transform(dest,src,A,B);
#endif
    }

    void affine_transform(
        tensor& dest,
        const tensor& src,
        const float A
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::affine_transform(dest,src,A);
#else
        cpu::affine_transform(dest,src,A,0);
#endif
    }

    void affine_transform(
        tensor& dest,
        const tensor& src1,
        const tensor& src2,
        const float A,
        const float B,
        const float C
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::affine_transform(dest,src1,src2,A,B,C);
#else
        cpu::affine_transform(dest,src1,src2,A,B,C);
#endif
    }

    void affine_transform(
        tensor& dest,
        const tensor& src1,
        const tensor& src2,
        const float A,
        const float B
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::affine_transform(dest,src1,src2,A,B);
#else
        cpu::affine_transform(dest,src1,src2,A,B,0);
#endif
    }

    void affine_transform(
        tensor& dest,
        const tensor& src1,
        const tensor& src2,
        const tensor& src3,
        const float A,
        const float B,
        const float C,
        const float D
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::affine_transform(dest,src1,src2,src3,A,B,C,D);
#else
        cpu::affine_transform(dest,src1,src2,src3,A,B,C,D);
#endif
    }

    void affine_transform_range(
        size_t begin,
        size_t end,
        tensor& dest,
        const tensor& src1,
        const tensor& src2,
        const tensor& src3,
        const float A,
        const float B,
        const float C
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::affine_transform_range(begin, end, dest,src1,src2,src3,A,B,C);
#else
        cpu::affine_transform_range(begin, end, dest,src1,src2,src3,A,B,C);
#endif
    }

    void affine_transform(
        const rectangle& rect,
        tensor& dest, 
        const tensor& src1, 
        const tensor& src2, 
        const tensor& src3, 
        float A, 
        float B,
        float C
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::affine_transform(rect, dest,src1,src2,src3,A,B,C);
#else
        cpu::affine_transform(rect, dest,src1,src2,src3,A,B,C);
#endif
    }

    void affine_transform(
        tensor& dest,
        const tensor& src1,
        const tensor& src2,
        const tensor& src3,
        const float A,
        const float B,
        const float C
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::affine_transform_range(0,dest.size(),dest,src1,src2,src3,A,B,C);
#else
        cpu::affine_transform_range(0,dest.size(),dest,src1,src2,src3,A,B,C);
#endif
    }

// ----------------------------------------------------------------------------------------

    void affine_transform(
        tensor& dest,
        const tensor& src,
        const tensor& A,
        const tensor& B
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::affine_transform(dest,src,A,B);
#else
        cpu::affine_transform(dest,src,A,B);
#endif
    }

// ----------------------------------------------------------------------------------------

    void affine_transform_conv(
        tensor& dest,
        const tensor& src,
        const tensor& A,
        const tensor& B
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::affine_transform_conv(dest,src,A,B);
#else
        cpu::affine_transform_conv(dest,src,A,B);
#endif
    }

// ----------------------------------------------------------------------------------------

    void compute_adam_update (
        size_t begin,
        size_t end,
        tensor& s,
        tensor& m,
        tensor& v,
        const float t,
        const float learning_rate,
        const float weight_decay,
        const float momentum1,
        const float momentum2,
        const tensor& params,
        const tensor& params_grad
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::compute_adam_update(begin, end, s, m, v, t, learning_rate, weight_decay, momentum1,
            momentum2, params, params_grad);
#else
        cpu::compute_adam_update(begin, end, s, m, v, t, learning_rate, weight_decay, momentum1,
            momentum2, params, params_grad);
#endif
    }

// ----------------------------------------------------------------------------------------

    void batch_normalize_inference (
        const double eps,
        resizable_tensor& dest,
        const tensor& src,
        const tensor& gamma, 
        const tensor& beta,
        const tensor& running_means,
        const tensor& running_variances
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::batch_normalize_inference(eps,dest,src,gamma,beta,running_means,running_variances);
#else
        cpu::batch_normalize_inference(eps,dest,src,gamma,beta,running_means,running_variances);
#endif
    }

    void batch_normalize (
        const double eps,
        resizable_tensor& dest,
        resizable_tensor& means,
        resizable_tensor& vars,
        const double averaging_factor,
        resizable_tensor& running_means,
        resizable_tensor& running_variances,
        const tensor& src,
        const tensor& gamma, 
        const tensor& beta 
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::batch_normalize(eps,dest,means,vars,averaging_factor,running_means,running_variances,src,gamma,beta);
#else
        cpu::batch_normalize(eps,dest,means,vars,averaging_factor,running_means,running_variances,src,gamma,beta);
#endif
    }

    void batch_normalize_gradient (
        const double eps,
            const tensor& gradient_input,
            const tensor& means,
            const tensor& invstds,
            const tensor& src,
            const tensor& gamma,
            tensor& src_grad,
            tensor& gamma_grad, 
            tensor& beta_grad 
    )
    {
             
#ifdef DLIB_USE_CUDA
        cuda::batch_normalize_gradient(eps,gradient_input, means, invstds, src, gamma, src_grad, gamma_grad, beta_grad);
#else
        cpu::batch_normalize_gradient(eps,gradient_input, means, invstds, src, gamma, src_grad, gamma_grad, beta_grad);
#endif
    }

// ----------------------------------------------------------------------------------------

    void batch_normalize_conv_inference (
        const double eps,
        resizable_tensor& dest,
        const tensor& src,
        const tensor& gamma, 
        const tensor& beta,
        const tensor& running_means,
        const tensor& running_variances
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::batch_normalize_conv_inference(eps,dest,src,gamma,beta,running_means,running_variances);
#else
        cpu::batch_normalize_conv_inference(eps,dest,src,gamma,beta,running_means,running_variances);
#endif
    }

    void batch_normalize_conv (
        const double eps,
        resizable_tensor& dest,
        resizable_tensor& means,
        resizable_tensor& vars,
        const double averaging_factor,
        resizable_tensor& running_means,
        resizable_tensor& running_variances,
        const tensor& src,
        const tensor& gamma, 
        const tensor& beta 
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::batch_normalize_conv(eps,dest,means,vars,averaging_factor,running_means,running_variances,src,gamma,beta);
#else
        cpu::batch_normalize_conv(eps,dest,means,vars,averaging_factor,running_means,running_variances,src,gamma,beta);
#endif
    }

    void batch_normalize_conv_gradient (
        const double eps,
        const tensor& gradient_input,
        const tensor& means,
        const tensor& invstds,
        const tensor& src,
        const tensor& gamma,
        tensor& src_grad,
        tensor& gamma_grad, 
        tensor& beta_grad 
    )
    {
             
#ifdef DLIB_USE_CUDA
        cuda::batch_normalize_conv_gradient(eps,gradient_input, means, invstds, src, gamma, src_grad, gamma_grad, beta_grad);
#else
        cpu::batch_normalize_conv_gradient(eps,gradient_input, means, invstds, src, gamma, src_grad, gamma_grad, beta_grad);
#endif
    }

// ----------------------------------------------------------------------------------------

    void layer_normalize (
        const double eps,
        resizable_tensor& dest,
        resizable_tensor& means,
        resizable_tensor& vars,
        const tensor& src,
        const tensor& gamma,
        const tensor& beta
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::layer_normalize(eps, dest, means, vars, src, gamma, beta);
#else
        cpu::layer_normalize(eps, dest, means, vars, src, gamma, beta);
#endif
    }

    void layer_normalize_gradient (
        const double eps,
            const tensor& gradient_input,
            const tensor& means,
            const tensor& invstds,
            const tensor& src,
            const tensor& gamma,
            tensor& src_grad,
            tensor& gamma_grad,
            tensor& beta_grad,
            resizable_tensor& dmeans,
            resizable_tensor& dvars
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::layer_normalize_gradient(eps, gradient_input, means, invstds, src, gamma, src_grad, gamma_grad, beta_grad, dmeans, dvars);
#else
        cpu::layer_normalize_gradient(eps, gradient_input, means, invstds, src, gamma, src_grad, gamma_grad, beta_grad, dmeans, dvars);
#endif
    }

// ----------------------------------------------------------------------------------------

    void rms_normalize(
        const double eps,
        resizable_tensor& dest,
        resizable_tensor& scale,
        const tensor& src,
        const tensor& gamma
    )
    {            
#ifdef DLIB_USE_CUDA
        cuda::rms_normalize(eps, dest, scale, src, gamma);
#else
        cpu::rms_normalize(eps, dest, scale, src, gamma);
#endif
    }

    void rms_normalize_gradient(
        const tensor& gradient_input,
        const tensor& scale,
        const tensor& src,
        const tensor& gamma,
        tensor& src_grad,
        tensor& gamma_grad,
        resizable_tensor& dscale
    )
    {            
#ifdef DLIB_USE_CUDA
        cuda::rms_normalize_gradient(gradient_input, scale, src, gamma, src_grad, gamma_grad, dscale);
#else
        cpu::rms_normalize_gradient(gradient_input, scale, src, gamma, src_grad, gamma_grad, dscale);
#endif
    }

// ----------------------------------------------------------------------------------------

    void threshold (
        tensor& data,
        float thresh
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::threshold(data,thresh);
#else
        cpu::threshold(data,thresh);
#endif
    }

    void dot (
        const tensor& a,
        const tensor& b,
        tensor& result,
        size_t idx
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::dot(a,b,result,idx);
#else
        cpu::dot(a,b,result,idx);
#endif
    }

// ----------------------------------------------------------------------------------------

    void add(
        float beta,
        tensor& dest,
        float alpha,
        const tensor& src
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::add(beta,dest,alpha,src);
#else
        cpu::add(beta,dest,alpha,src);
#endif
    }

// ----------------------------------------------------------------------------------------

    void add (
        tensor& dest,
        const tensor& src1,
        const tensor& src2
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::add(dest, src1, src2);
#else
        cpu::add(dest, src1, src2);
#endif
    }

// ----------------------------------------------------------------------------------------

    void assign_conv_bias_gradient (
        tensor& grad,
        const tensor& gradient_input
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::assign_conv_bias_gradient(grad,gradient_input);
#else
        cpu::assign_conv_bias_gradient(grad,gradient_input);
#endif
    }

// ----------------------------------------------------------------------------------------

    void assign_bias_gradient (
        tensor& grad,
        const tensor& gradient_input
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::assign_bias_gradient(grad,gradient_input);
#else
        cpu::assign_bias_gradient(grad,gradient_input);
#endif
    }

// ----------------------------------------------------------------------------------------

    void softmax(
        tensor& dest,
        const tensor& src,
        operation_mode mode
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::softmax(dest, src, mode);
#else
        cpu::softmax(dest, src, mode);
#endif
    }

    void softmax_gradient(
        tensor& grad,
        const tensor& dest,
        const tensor& gradient_input,
        operation_mode mode
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::softmax_gradient(grad, dest, gradient_input, mode);
#else
        cpu::softmax_gradient(grad, dest, gradient_input, mode);
#endif
    }

// ----------------------------------------------------------------------------------------

    void softmax_all (
        tensor& dest,
        const tensor& src
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::softmax_all(dest,src);
#else
        cpu::softmax_all(dest,src);
#endif
    }

    void softmax_all_gradient (
        tensor& grad,
        const tensor& dest,
        const tensor& gradient_input
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::softmax_all_gradient(grad, dest, gradient_input);
#else
        cpu::softmax_all_gradient(grad, dest, gradient_input);
#endif
    }

// ----------------------------------------------------------------------------------------

    void sigmoid (
        tensor& dest,
        const tensor& src
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::sigmoid(dest,src);
#else
        cpu::sigmoid(dest,src);
#endif
    }

    void sigmoid_gradient (
        tensor& grad,
        const tensor& dest,
        const tensor& gradient_input
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::sigmoid_gradient(grad, dest, gradient_input);
#else
        cpu::sigmoid_gradient(grad, dest, gradient_input);
#endif
    }

// ----------------------------------------------------------------------------------------

    void mish (
        tensor& dest,
        const tensor& src
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::mish(dest,src);
#else
        cpu::mish(dest,src);
#endif
    }

    void mish_gradient (
        tensor& grad,
        const tensor& src,
        const tensor& gradient_input
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::mish_gradient(grad, src, gradient_input);
#else
        cpu::mish_gradient(grad, src, gradient_input);
#endif
    }

// ----------------------------------------------------------------------------------------

    void relu (
        tensor& dest,
        const tensor& src
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::relu(dest,src);
#else
        cpu::relu(dest,src);
#endif
    }

    void relu_gradient (
        tensor& grad,
        const tensor& dest,
        const tensor& gradient_input
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::relu_gradient(grad, dest, gradient_input);
#else
        cpu::relu_gradient(grad, dest, gradient_input);
#endif
    }

// ----------------------------------------------------------------------------------------

    void prelu (
        tensor& dest,
        const tensor& src,
        const tensor& param
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::prelu(dest, src, param);
#else
        cpu::prelu(dest, src, param);
#endif
    }

    void prelu_gradient (
        tensor& grad,
        const tensor& src,
        const tensor& gradient_input,
        const tensor& param,
        tensor& params_grad 
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::prelu_gradient(grad, src, gradient_input, param, params_grad);
#else
        cpu::prelu_gradient(grad, src, gradient_input, param, params_grad);
#endif
    }

// ----------------------------------------------------------------------------------------

    void leaky_relu (
        tensor& dest,
        const tensor& src,
        const float alpha
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::leaky_relu(dest, src, alpha);
#else
        cpu::leaky_relu(dest, src, alpha);
#endif
    }

    void leaky_relu_gradient (
        tensor& grad,
        const tensor& dest,
        const tensor& gradient_input,
        const float alpha
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::leaky_relu_gradient(grad, dest, gradient_input, alpha);
#else
        cpu::leaky_relu_gradient(grad, dest, gradient_input, alpha);
#endif
    }

// ----------------------------------------------------------------------------------------

    void tanh (
        tensor& dest,
        const tensor& src
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::tanh(dest,src);
#else
        cpu::tanh(dest,src);
#endif
    }

    void tanh_gradient (
        tensor& grad,
        const tensor& dest,
        const tensor& gradient_input
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::tanh_gradient(grad, dest, gradient_input);
#else
        cpu::tanh_gradient(grad, dest, gradient_input);
#endif
    }

// ----------------------------------------------------------------------------------------

    void clipped_relu (
        tensor& dest,
        const tensor& src,
        const float ceiling
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::clipped_relu(dest, src, ceiling);
#else
        cpu::clipped_relu(dest, src, ceiling);
#endif
    }

    void clipped_relu_gradient (
        tensor& grad,
        const tensor& dest,
        const tensor& gradient_input,
        const float ceiling
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::clipped_relu_gradient(grad, dest, gradient_input, ceiling);
#else
        cpu::clipped_relu_gradient(grad, dest, gradient_input, ceiling);
#endif
    }

// ----------------------------------------------------------------------------------------

    void elu (
        tensor& dest,
        const tensor& src,
        const float alpha
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::elu(dest, src, alpha);
#else
        cpu::elu(dest, src, alpha);
#endif
    }

    void elu_gradient (
        tensor& grad,
        const tensor& dest,
        const tensor& gradient_input,
        const float alpha
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::elu_gradient(grad, dest, gradient_input, alpha);
#else
        cpu::elu_gradient(grad, dest, gradient_input, alpha);
#endif
    }

// ----------------------------------------------------------------------------------------

    void gelu (
        tensor& dest,
        const tensor& src
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::gelu(dest,src);
#else
        cpu::gelu(dest,src);
#endif
    }

    void gelu_gradient (
        tensor& grad,
        const tensor& src,
        const tensor& gradient_input
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::gelu_gradient(grad, src, gradient_input);
#else
        cpu::gelu_gradient(grad, src, gradient_input);
#endif
    }

// ----------------------------------------------------------------------------------------

    void smelu (
        tensor& dest,
        const tensor& src,
        const float beta
    )
    {
        DLIB_CASSERT(beta > 0);
#ifdef DLIB_USE_CUDA
        cuda::smelu(dest, src, beta);
#else
        cpu::smelu(dest, src, beta);
#endif
    }

    void smelu_gradient (
        tensor& grad,
        const tensor& dest,
        const tensor& gradient_input,
        const float beta
    )
    {
        DLIB_CASSERT(beta > 0);
#ifdef DLIB_USE_CUDA
        cuda::smelu_gradient(grad, dest, gradient_input, beta);
#else
        cpu::smelu_gradient(grad, dest, gradient_input, beta);
#endif
    }

// ----------------------------------------------------------------------------------------

    void silu (
        tensor& dest,
        const tensor& src
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::silu(dest,src);
#else
        cpu::silu(dest,src);
#endif
    }

    void silu_gradient (
        tensor& grad,
        const tensor& src,
        const tensor& gradient_input
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::silu_gradient(grad, src, gradient_input);
#else
        cpu::silu_gradient(grad, src, gradient_input);
#endif
    }

// ----------------------------------------------------------------------------------------

    void resize_bilinear (
        tensor& dest,
        long dest_row_stride,
        long dest_channel_stride,
        const tensor& src,
        long src_row_stride,
        long src_channel_stride
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::resize_bilinear(dest,dest_row_stride,dest_channel_stride, src,src_row_stride,src_channel_stride);
#else
        cpu::resize_bilinear(dest,dest_row_stride,dest_channel_stride, src,src_row_stride,src_channel_stride);
#endif
    }

    void resize_bilinear_gradient (
        tensor& grad,
        long grad_row_stride,
        long grad_channel_stride,
        const tensor& gradient_input,
        long gradient_input_row_stride,
        long gradient_input_channel_stride
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::resize_bilinear_gradient(grad,grad_row_stride,grad_channel_stride,  gradient_input,gradient_input_row_stride,gradient_input_channel_stride);
#else
        cpu::resize_bilinear_gradient(grad,grad_row_stride,grad_channel_stride,  gradient_input,gradient_input_row_stride,gradient_input_channel_stride);
#endif
    }

// ------------------------------------------------------------------------------------

    void reorg (
        bool add_to,
        tensor& dest,
        const int row_stride,
        const int col_stride,
        const tensor& src
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::reorg(add_to, dest, row_stride, col_stride, src);
#else
        cpu::reorg(add_to, dest, row_stride, col_stride, src);
#endif
    }

    void reorg_gradient (
        bool add_to,
        tensor& grad,
        const int row_stride,
        const int col_stride,
        const tensor& gradient_input
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::reorg_gradient(add_to, grad, row_stride, col_stride, gradient_input);
#else
        cpu::reorg_gradient(add_to, grad, row_stride, col_stride, gradient_input);
#endif
    }

// ------------------------------------------------------------------------------------

    void copy_tensor(
            bool add_to,
            tensor& dest,
            size_t dest_k_offset,
            const tensor& src,
            size_t src_k_offset,
            size_t count_k
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::copy_tensor(add_to, dest, dest_k_offset, src, src_k_offset, count_k);
#else
        cpu::copy_tensor(add_to, dest, dest_k_offset, src, src_k_offset, count_k);
#endif
    }

// ----------------------------------------------------------------------------------------

    void copy_tensor(
        bool add_to,
        tensor& dest,
        size_t dk, size_t dnr, size_t dnc,
        const tensor& src,
        size_t sk, size_t snr, size_t snc,
        size_t k, size_t nr, size_t nc
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::copy_tensor(add_to, dest, dk, dnr, dnc , src, sk, snr, snc, k, nr, nc);
#else
        cpu::copy_tensor(add_to, dest, dk, dnr, dnc, src, sk, snr, snc, k, nr, nc);
#endif
    }

// ----------------------------------------------------------------------------------------

    void inv::
    operator() (
        const tensor& m,
        resizable_tensor& out
    )
    {
#ifdef DLIB_USE_CUDA
        finv(m,out);
#else
        out = dlib::inv(mat(m));
#endif
    }

// ----------------------------------------------------------------------------------------

    void transpose(
        bool add_to,
        tensor& dest,
        const tensor& src
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::transpose(add_to, dest, src);
#else
        cpu::transpose(add_to, dest, src);
#endif
    }

// ----------------------------------------------------------------------------------------

    void embeddings(
        resizable_tensor& dest,
        const tensor& src,
        const tensor& embs
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::embeddings(dest, src, embs);
#else
        cpu::embeddings(dest, src, embs);
#endif
    }

    void embeddings_gradient(
        const tensor& prev,
        const tensor& gradient_input,
        tensor& grads,
        const tensor& freqs,
        float learning_rate,
        bool scale
    )
    {
#ifdef DLIB_USE_CUDA
        cuda::embeddings_gradient(prev, gradient_input, grads, freqs, learning_rate, scale);
#else
        cpu::embeddings_gradient(prev, gradient_input, grads, freqs, learning_rate, scale);
#endif
    }

// ----------------------------------------------------------------------------------------

}}

#endif // DLIB_TeNSOR_TOOLS_CPP_
