/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2019 Yermalayeu Ihar.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#include "Simd/SimdMemory.h"
#include "Simd/SimdStore.h"
#include "Simd/SimdExtract.h"
#include "Simd/SimdSynet.h"
#include "Simd/SimdPow.h"
#include "Simd/SimdExp.h"
#include "Simd/SimdBase.h"
#include "Simd/SimdSse1.h"
#include "Simd/SimdAvx1.h"
#include "Simd/SimdAvx2.h"
#include "Simd/SimdAvx512f.h"
#include "Simd/SimdArray.h"

namespace Simd
{
#ifdef SIMD_AVX512F_ENABLE    
    namespace Avx512f
    {
        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward0(const float * src, const float * bias, const float * scale, __m512 sign, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 _bias = Load<align, mask>(bias + offset, tail);
            __m512 x = _mm512_add_ps((Load<align, mask>(src + offset, tail)), _bias);
            __m512 _scale = Load<align, mask>(scale + offset, tail);
            Store<align, mask>(dst + offset, _mm512_add_ps(_mm512_mul_ps(_mm512_sub_ps(x, _mm512_andnot_ps(sign, x)), _scale), _mm512_max_ps(_mm512_setzero_ps(), x)), tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward0(const float * src, __m512 bias, __m512 scale, __m512 sign, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 x = _mm512_add_ps((Load<align, mask>(src + offset, tail)), bias);
            Store<align, mask>(dst + offset, _mm512_add_ps(_mm512_mul_ps(_mm512_sub_ps(x, _mm512_andnot_ps(sign, x)), scale), _mm512_max_ps(_mm512_setzero_ps(), x)), tail);
        }

        template <bool align> void SynetFusedLayerForward0Nchw(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(spatial) && Aligned(dst));

            size_t aligned = AlignLo(spatial, QF);
            size_t partial = AlignLo(spatial, F);
            __m512 sign = _mm512_set1_ps(-0.0f);
            __mmask16 tail = TailMask16(spatial - partial);
            for (size_t c = 0; c < channels; ++c)
            {
                size_t s = 0;
                __m512 _bias = _mm512_set1_ps(bias[c]);
                __m512 _scale = _mm512_set1_ps(scale[c]);
                for (; s < aligned; s += QF)
                {
                    SynetFusedLayerForward0<align, false>(src, _bias, _scale, sign, dst, s + F * 0);
                    SynetFusedLayerForward0<align, false>(src, _bias, _scale, sign, dst, s + F * 1);
                    SynetFusedLayerForward0<align, false>(src, _bias, _scale, sign, dst, s + F * 2);
                    SynetFusedLayerForward0<align, false>(src, _bias, _scale, sign, dst, s + F * 3);
                }
                for (; s < partial; s += F)
                    SynetFusedLayerForward0<align, false>(src, _bias, _scale, sign, dst, s);
                if(s < spatial)
                    SynetFusedLayerForward0<align, true>(src, _bias, _scale, sign, dst, s, tail);
                src += spatial;
                dst += spatial;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward0Nchw(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(spatial) && Aligned(dst))
                SynetFusedLayerForward0Nchw<true>(src, bias, scale, channels, spatial, dst);
            else
                SynetFusedLayerForward0Nchw<false>(src, bias, scale, channels, spatial, dst);
        }

        template <bool align> void SynetFusedLayerForward0Nhwc(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(bias) && Aligned(scale) && Aligned(channels) && Aligned(dst));

            size_t aligned = AlignLo(channels, QF);
            size_t partial = AlignLo(channels, F);
            __m512 sign = _mm512_set1_ps(-0.0f);
            __mmask16 tail = TailMask16(channels - partial);
            for (size_t s = 0; s < spatial; ++s)
            {
                size_t c = 0;
                for (; c < aligned; c += QF)
                {
                    SynetFusedLayerForward0<align, false>(src, bias, scale, sign, dst, c + F * 0);
                    SynetFusedLayerForward0<align, false>(src, bias, scale, sign, dst, c + F * 1);
                    SynetFusedLayerForward0<align, false>(src, bias, scale, sign, dst, c + F * 2);
                    SynetFusedLayerForward0<align, false>(src, bias, scale, sign, dst, c + F * 3);
                }
                for (; c < partial; c += F)
                    SynetFusedLayerForward0<align, false>(src, bias, scale, sign, dst, c);
                if (c < channels)
                    SynetFusedLayerForward0<align, true>(src, bias, scale, sign, dst, c, tail);
                src += channels;
                dst += channels;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward0Nhwc(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(bias) && Aligned(scale) && Aligned(channels) && Aligned(dst))
                SynetFusedLayerForward0Nhwc<true>(src, bias, scale, channels, spatial, dst);
            else
                SynetFusedLayerForward0Nhwc<false>(src, bias, scale, channels, spatial, dst);
        }

        template <bool align> void SynetFusedLayerForward0Nchw16c(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(dst));

            size_t spatialF = spatial * F;
            size_t spatial4F = AlignLo(spatial, 4)*F;
            __m512 sign = _mm512_set1_ps(-0.0f);
            for (size_t c = 0; c < channels; c += F)
            {
                __m512 _bias = Load<false>(bias + c);
                __m512 _scale = Load<false>(scale + c);
                size_t s = 0;
                for (; s < spatial4F; s += 4 * F)
                {
                    SynetFusedLayerForward0<align, false>(src, _bias, _scale, sign, dst, s + F * 0);
                    SynetFusedLayerForward0<align, false>(src, _bias, _scale, sign, dst, s + F * 1);
                    SynetFusedLayerForward0<align, false>(src, _bias, _scale, sign, dst, s + F * 2);
                    SynetFusedLayerForward0<align, false>(src, _bias, _scale, sign, dst, s + F * 3);
                }
                for (; s < spatialF; s += F)
                    SynetFusedLayerForward0<align, false>(src, _bias, _scale, sign, dst, s);
                src += spatialF;
                dst += spatialF;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward0Nchw16c(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(dst))
                SynetFusedLayerForward0Nchw16c<true>(src, bias, scale, channels, spatial, dst);
            else
                SynetFusedLayerForward0Nchw16c<false>(src, bias, scale, channels, spatial, dst);
        }

        void SynetFusedLayerForward0(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst, SimdTensorFormatType format)
        {
            if (Base::NchwCompatible(channels, spatial, format))
                SynetFusedLayerForward0Nchw(src, bias, scale, channels, spatial, dst);
            else if (Base::NhwcCompatible(channels, spatial, format))
                SynetFusedLayerForward0Nhwc(src, bias, scale, channels, spatial, dst);
            else if (format == SimdTensorFormatNchw4c)
                Sse::SynetFusedLayerForward0(src, bias, scale, channels, spatial, dst, format);
            else if (format == SimdTensorFormatNchw8c)
                Avx::SynetFusedLayerForward0(src, bias, scale, channels, spatial, dst, format);
            else if (format == SimdTensorFormatNchw16c)
                SynetFusedLayerForward0Nchw16c(src, bias, scale, channels, spatial, dst);
            else
                Base::SynetFusedLayerForward0(src, bias, scale, channels, spatial, dst, format);
        }

        //---------------------------------------------------------------------

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward1(const float * src, const float * bias0, const float * scale1, const float * bias1, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 _bias0 = Load<align, mask>(bias0 + offset, tail);
            __m512 x = _mm512_add_ps((Load<align, mask>(src + offset, tail)), _bias0);
            __m512 _scale1 = Load<align, mask>(scale1 + offset, tail);
            __m512 _bias1 = Load<align, mask>(bias1 + offset, tail);
            Store<align, mask>(dst + offset, _mm512_add_ps(_mm512_fmadd_ps(_mm512_max_ps(_mm512_setzero_ps(), _mm512_sub_ps(_mm512_setzero_ps(), x)), _scale1, _bias1), _mm512_max_ps(_mm512_setzero_ps(), x)), tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward1(const float * src, __m512 bias0, __m512 scale1, __m512 bias1, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 x = _mm512_add_ps((Load<align, mask>(src + offset, tail)), bias0);
            Store<align, mask>(dst + offset, _mm512_add_ps(_mm512_fmadd_ps(_mm512_max_ps(_mm512_setzero_ps(), _mm512_sub_ps(_mm512_setzero_ps(), x)), scale1, bias1), _mm512_max_ps(_mm512_setzero_ps(), x)), tail);
        }

        template <bool align> void SynetFusedLayerForward1Nchw(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(spatial) && Aligned(dst));

            size_t aligned = AlignLo(spatial, QF);
            size_t partial = AlignLo(spatial, F);
            __mmask16 tail = TailMask16(spatial - partial);
            for (size_t c = 0; c < channels; ++c)
            {
                size_t s = 0;
                __m512 _bias0 = _mm512_set1_ps(bias0[c]);
                __m512 _scale1 = _mm512_set1_ps(scale1[c]);
                __m512 _bias1 = _mm512_set1_ps(bias1[c]);
                for (; s < aligned; s += QF)
                {
                    SynetFusedLayerForward1<align, false>(src, _bias0, _scale1, _bias1, dst, s + F * 0);
                    SynetFusedLayerForward1<align, false>(src, _bias0, _scale1, _bias1, dst, s + F * 1);
                    SynetFusedLayerForward1<align, false>(src, _bias0, _scale1, _bias1, dst, s + F * 2);
                    SynetFusedLayerForward1<align, false>(src, _bias0, _scale1, _bias1, dst, s + F * 3);
                }
                for (; s < partial; s += F)
                    SynetFusedLayerForward1<align, false>(src, _bias0, _scale1, _bias1, dst, s);
                if (s < spatial)
                    SynetFusedLayerForward1<align, true>(src, _bias0, _scale1, _bias1, dst, s, tail);
                src += spatial;
                dst += spatial;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward1Nchw(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(spatial) && Aligned(dst))
                SynetFusedLayerForward1Nchw<true>(src, bias0, scale1, bias1, channels, spatial, dst);
            else
                SynetFusedLayerForward1Nchw<false>(src, bias0, scale1, bias1, channels, spatial, dst);
        }

        template <bool align> void SynetFusedLayerForward1Nhwc(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(bias0) && Aligned(scale1) && Aligned(bias1) && Aligned(channels) && Aligned(dst));

            size_t aligned = AlignLo(channels, QF);
            size_t partial = AlignLo(channels, F);
            __mmask16 tail = TailMask16(channels - partial);
            for (size_t s = 0; s < spatial; ++s)
            {
                size_t c = 0;
                for (; c < aligned; c += QF)
                {
                    SynetFusedLayerForward1<align, false>(src, bias0, scale1, bias1, dst, c + F * 0);
                    SynetFusedLayerForward1<align, false>(src, bias0, scale1, bias1, dst, c + F * 1);
                    SynetFusedLayerForward1<align, false>(src, bias0, scale1, bias1, dst, c + F * 2);
                    SynetFusedLayerForward1<align, false>(src, bias0, scale1, bias1, dst, c + F * 3);
                }
                for (; c < partial; c += F)
                    SynetFusedLayerForward1<align, false>(src, bias0, scale1, bias1, dst, c);
                if (c < channels)
                    SynetFusedLayerForward1<align, true>(src, bias0, scale1, bias1, dst, c, tail);
                src += channels;
                dst += channels;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward1Nhwc(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(bias0) && Aligned(scale1) && Aligned(bias1) && Aligned(channels) && Aligned(dst))
                SynetFusedLayerForward1Nhwc<true>(src, bias0, scale1, bias1, channels, spatial, dst);
            else
                SynetFusedLayerForward1Nhwc<false>(src, bias0, scale1, bias1, channels, spatial, dst);
        }

        template <bool align> void SynetFusedLayerForward1Nchw16c(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(dst));

            size_t spatialF = spatial * F;
            size_t spatial4F = AlignLo(spatial, 4)*F;
            for (size_t c = 0; c < channels; c += F)
            {
                __m512 _bias0 = Load<false>(bias0 + c);
                __m512 _scale1 = Load<false>(scale1 + c);
                __m512 _bias1 = Load<false>(bias1 + c);
                size_t s = 0;
                for (; s < spatial4F; s += 4 * F)
                {
                    SynetFusedLayerForward1<align, false>(src, _bias0, _scale1, _bias1, dst, s + F * 0);
                    SynetFusedLayerForward1<align, false>(src, _bias0, _scale1, _bias1, dst, s + F * 1);
                    SynetFusedLayerForward1<align, false>(src, _bias0, _scale1, _bias1, dst, s + F * 2);
                    SynetFusedLayerForward1<align, false>(src, _bias0, _scale1, _bias1, dst, s + F * 3);
                }
                for (; s < spatialF; s += F)
                    SynetFusedLayerForward1<align, false>(src, _bias0, _scale1, _bias1, dst, s);
                src += spatialF;
                dst += spatialF;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward1Nchw16c(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(dst))
                SynetFusedLayerForward1Nchw16c<true>(src, bias0, scale1, bias1, channels, spatial, dst);
            else
                SynetFusedLayerForward1Nchw16c<false>(src, bias0, scale1, bias1, channels, spatial, dst);
        }

        void SynetFusedLayerForward1(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst, SimdTensorFormatType format)
        {
            if (Base::NchwCompatible(channels, spatial, format))
                SynetFusedLayerForward1Nchw(src, bias0, scale1, bias1, channels, spatial, dst);
            else if (Base::NhwcCompatible(channels, spatial, format))
                SynetFusedLayerForward1Nhwc(src, bias0, scale1, bias1, channels, spatial, dst);
            else if (format == SimdTensorFormatNchw4c)
                Sse::SynetFusedLayerForward1(src, bias0, scale1, bias1, channels, spatial, dst, format);
            else if (format == SimdTensorFormatNchw8c)
                Avx::SynetFusedLayerForward1(src, bias0, scale1, bias1, channels, spatial, dst, format);
            else if (format == SimdTensorFormatNchw16c)
                SynetFusedLayerForward1Nchw16c(src, bias0, scale1, bias1, channels, spatial, dst);
            else
                Base::SynetFusedLayerForward1(src, bias0, scale1, bias1, channels, spatial, dst, format);
        }

        //---------------------------------------------------------------------

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward2(const float * src, const float * scale, const float * bias, __m512 slope, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 _src = Load<align, mask>(src + offset, tail);
            __m512 _scale = Load<align, mask>(scale + offset, tail);
            __m512 _bias = Load<align, mask>(bias + offset, tail);
            __m512 x = _mm512_fmadd_ps(_src, _scale, _bias);
            __m512 _dst = _mm512_add_ps(_mm512_max_ps(_mm512_setzero_ps(), x), _mm512_mul_ps(_mm512_min_ps(_mm512_setzero_ps(), x), slope));
            Store<align, mask>(dst + offset, _dst, tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward2(const float * src, __m512 scale, __m512 bias, __m512 slope, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 _src = Load<align, mask>(src + offset, tail);
            __m512 x = _mm512_fmadd_ps(_src, scale, bias);
            __m512 _dst = _mm512_add_ps(_mm512_max_ps(_mm512_setzero_ps(), x), _mm512_mul_ps(_mm512_min_ps(_mm512_setzero_ps(), x), slope));
            Store<align, mask>(dst + offset, _dst, tail);
        }

        template <bool align> void SynetFusedLayerForward2Nchw(const float * src, const float * scale, const float * bias, size_t channels, size_t spatial, const float * slope, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(spatial) && Aligned(dst));

            __m512 _slope = _mm512_set1_ps(slope[0]);
            size_t aligned = AlignLo(spatial, QF);
            size_t partial = AlignLo(spatial, F);
            __mmask16 tail = TailMask16(spatial - partial);
            for (size_t c = 0; c < channels; ++c)
            {
                size_t s = 0;
                __m512 _scale = _mm512_set1_ps(scale[c]);
                __m512 _bias = _mm512_set1_ps(bias[c]);
                for (; s < aligned; s += QF)
                {
                    SynetFusedLayerForward2<align, false>(src, _scale, _bias, _slope, dst, s + F * 0);
                    SynetFusedLayerForward2<align, false>(src, _scale, _bias, _slope, dst, s + F * 1);
                    SynetFusedLayerForward2<align, false>(src, _scale, _bias, _slope, dst, s + F * 2);
                    SynetFusedLayerForward2<align, false>(src, _scale, _bias, _slope, dst, s + F * 3);
                }
                for (; s < partial; s += F)
                    SynetFusedLayerForward2<align, false>(src, _scale, _bias, _slope, dst, s);
                if (s < spatial)
                    SynetFusedLayerForward2<align, true>(src, _scale, _bias, _slope, dst, s, tail);
                src += spatial;
                dst += spatial;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward2Nchw(const float * src, const float * scale, const float * bias, size_t channels, size_t spatial, const float * slope, float * dst)
        {
            if (Aligned(src) && Aligned(spatial) && Aligned(dst))
                SynetFusedLayerForward2Nchw<true>(src, scale, bias, channels, spatial, slope, dst);
            else
                SynetFusedLayerForward2Nchw<false>(src, scale, bias, channels, spatial, slope, dst);
        }

        template <bool align> void SynetFusedLayerForward2Nhwc(const float * src, const float * scale, const float * bias, size_t channels, size_t spatial, const float * slope, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(scale) && Aligned(bias) && Aligned(channels) && Aligned(dst));

            __m512 _slope = _mm512_set1_ps(slope[0]);
            size_t aligned = AlignLo(channels, QF);
            size_t partial = AlignLo(channels, F);
            __mmask16 tail = TailMask16(channels - partial);
            for (size_t s = 0; s < spatial; ++s)
            {
                size_t c = 0;
                for (; c < aligned; c += QF)
                {
                    SynetFusedLayerForward2<align, false>(src, scale, bias, _slope, dst, c + F * 0);
                    SynetFusedLayerForward2<align, false>(src, scale, bias, _slope, dst, c + F * 1);
                    SynetFusedLayerForward2<align, false>(src, scale, bias, _slope, dst, c + F * 2);
                    SynetFusedLayerForward2<align, false>(src, scale, bias, _slope, dst, c + F * 3);
                }
                for (; c < partial; c += F)
                    SynetFusedLayerForward2<align, false>(src, scale, bias, _slope, dst, c);
                if (c < channels)
                    SynetFusedLayerForward2<align, true>(src, scale, bias, _slope, dst, c, tail);
                src += channels;
                dst += channels;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward2Nhwc(const float * src, const float * scale, const float * bias, size_t channels, size_t spatial, const float * slope, float * dst)
        {
            if (Aligned(src) && Aligned(scale) && Aligned(bias) && Aligned(channels) && Aligned(dst))
                SynetFusedLayerForward2Nhwc<true>(src, scale, bias, channels, spatial, slope, dst);
            else
                SynetFusedLayerForward2Nhwc<false>(src, scale, bias, channels, spatial, slope, dst);
        }

        template <bool align> void SynetFusedLayerForward2Nchw16c(const float * src, const float * scale, const float * bias, size_t channels, size_t spatial, const float * slope, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(dst));

            __m512 _slope = _mm512_set1_ps(slope[0]);
            size_t spatialF = spatial * F;
            size_t spatial4F = AlignLo(spatial, 4)*F;
            for (size_t c = 0; c < channels; c += F)
            {
                __m512 _scale = Load<false>(scale + c);
                __m512 _bias = Load<false>(bias + c);
                size_t s = 0;
                for (; s < spatial4F; s += 4 * F)
                {
                    SynetFusedLayerForward2<align, false>(src, _scale, _bias, _slope, dst, s + F * 0);
                    SynetFusedLayerForward2<align, false>(src, _scale, _bias, _slope, dst, s + F * 1);
                    SynetFusedLayerForward2<align, false>(src, _scale, _bias, _slope, dst, s + F * 2);
                    SynetFusedLayerForward2<align, false>(src, _scale, _bias, _slope, dst, s + F * 3);
                }
                for (; s < spatialF; s += F)
                    SynetFusedLayerForward2<align, false>(src, _scale, _bias, _slope, dst, s);
                src += spatialF;
                dst += spatialF;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward2Nchw16c(const float * src, const float * scale, const float * bias, size_t channels, size_t spatial, const float * slope, float * dst)
        {
            if (Aligned(src) && Aligned(dst))
                SynetFusedLayerForward2Nchw16c<true>(src, scale, bias, channels, spatial, slope, dst);
            else
                SynetFusedLayerForward2Nchw16c<false>(src, scale, bias, channels, spatial, slope, dst);
        }

        void SynetFusedLayerForward2(const float * src, const float * scale, const float * bias, size_t channels, size_t spatial, const float * slope, float * dst, SimdTensorFormatType format)
        {
            if (Base::NchwCompatible(channels, spatial, format))
                SynetFusedLayerForward2Nchw(src, scale, bias, channels, spatial, slope, dst);
            else if (Base::NhwcCompatible(channels, spatial, format))
                SynetFusedLayerForward2Nhwc(src, scale, bias, channels, spatial, slope, dst);
            else if (format == SimdTensorFormatNchw4c)
                Sse::SynetFusedLayerForward2(src, scale, bias, channels, spatial, slope, dst, format);
            else if (format == SimdTensorFormatNchw8c)
                Avx::SynetFusedLayerForward2(src, scale, bias, channels, spatial, slope, dst, format);
            else if (format == SimdTensorFormatNchw16c)
                SynetFusedLayerForward2Nchw16c(src, scale, bias, channels, spatial, slope, dst);
            else
                Base::SynetFusedLayerForward2(src, scale, bias, channels, spatial, slope, dst, format);
        }

        //---------------------------------------------------------------------

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward3(const float * src, const float * bias, const float * scale, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 _bias = Load<align, mask>(bias + offset, tail);
            __m512 x = _mm512_add_ps((Load<align, mask>(src + offset, tail)), _bias);
            __m512 _scale = Load<align, mask>(scale + offset, tail);
            __m512 pos = _mm512_max_ps(_mm512_setzero_ps(), x);
            __m512 neg = _mm512_min_ps(_mm512_setzero_ps(), x);
            Store<align, mask>(dst + offset, _mm512_add_ps(pos, _mm512_mul_ps(_scale, neg)), tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward3(const float * src, __m512 bias, __m512 scale, float * dst, size_t offset, __mmask16 tail = -1)
        {
            __m512 x = _mm512_add_ps((Load<align, mask>(src + offset, tail)), bias);
            __m512 pos = _mm512_max_ps(_mm512_setzero_ps(), x);
            __m512 neg = _mm512_min_ps(_mm512_setzero_ps(), x);
            Store<align, mask>(dst + offset, _mm512_add_ps(pos, _mm512_mul_ps(scale, neg)), tail);
        }

        template <bool align> void SynetFusedLayerForward3Nchw(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(spatial) && Aligned(dst));

            size_t aligned = AlignLo(spatial, QF);
            size_t partial = AlignLo(spatial, F);
            __mmask16 tail = TailMask16(spatial - partial);
            for (size_t c = 0; c < channels; ++c)
            {
                size_t s = 0;
                __m512 _bias = _mm512_set1_ps(bias[c]);
                __m512 _scale = _mm512_set1_ps(scale[c]);
                for (; s < aligned; s += QF)
                {
                    SynetFusedLayerForward3<align, false>(src, _bias, _scale, dst, s + F * 0);
                    SynetFusedLayerForward3<align, false>(src, _bias, _scale, dst, s + F * 1);
                    SynetFusedLayerForward3<align, false>(src, _bias, _scale, dst, s + F * 2);
                    SynetFusedLayerForward3<align, false>(src, _bias, _scale, dst, s + F * 3);
                }
                for (; s < partial; s += F)
                    SynetFusedLayerForward3<align, false>(src, _bias, _scale, dst, s);
                if (s < spatial)
                    SynetFusedLayerForward3<align, true>(src, _bias, _scale, dst, s, tail);
                src += spatial;
                dst += spatial;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward3Nchw(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(spatial) && Aligned(dst))
                SynetFusedLayerForward3Nchw<true>(src, bias, scale, channels, spatial, dst);
            else
                SynetFusedLayerForward3Nchw<false>(src, bias, scale, channels, spatial, dst);
        }

        template <bool align> void SynetFusedLayerForward3Nhwc(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(bias) && Aligned(scale) && Aligned(channels) && Aligned(dst));

            size_t aligned = AlignLo(channels, QF);
            size_t partial = AlignLo(channels, F);
            __mmask16 tail = TailMask16(channels - partial);
            for (size_t s = 0; s < spatial; ++s)
            {
                size_t c = 0;
                for (; c < aligned; c += QF)
                {
                    SynetFusedLayerForward3<align, false>(src, bias, scale, dst, c + F * 0);
                    SynetFusedLayerForward3<align, false>(src, bias, scale, dst, c + F * 1);
                    SynetFusedLayerForward3<align, false>(src, bias, scale, dst, c + F * 2);
                    SynetFusedLayerForward3<align, false>(src, bias, scale, dst, c + F * 3);
                }
                for (; c < partial; c += F)
                    SynetFusedLayerForward3<align, false>(src, bias, scale, dst, c);
                if (c < channels)
                    SynetFusedLayerForward3<align, true>(src, bias, scale, dst, c, tail);
                src += channels;
                dst += channels;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward3Nhwc(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(bias) && Aligned(scale) && Aligned(channels) && Aligned(dst))
                SynetFusedLayerForward3Nhwc<true>(src, bias, scale, channels, spatial, dst);
            else
                SynetFusedLayerForward3Nhwc<false>(src, bias, scale, channels, spatial, dst);
        }

        template <bool align> void SynetFusedLayerForward3Nchw16c(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(dst));

            size_t spatialF = spatial * F;
            size_t spatial4F = AlignLo(spatial, 4)*F;
            for (size_t c = 0; c < channels; c += F)
            {
                __m512 _bias = Load<false>(bias + c);
                __m512 _scale = Load<false>(scale + c);
                size_t s = 0;
                for (; s < spatial4F; s += 4 * F)
                {
                    SynetFusedLayerForward3<align, false>(src, _bias, _scale, dst, s + F * 0);
                    SynetFusedLayerForward3<align, false>(src, _bias, _scale, dst, s + F * 1);
                    SynetFusedLayerForward3<align, false>(src, _bias, _scale, dst, s + F * 2);
                    SynetFusedLayerForward3<align, false>(src, _bias, _scale, dst, s + F * 3);
                }
                for (; s < spatialF; s += F)
                    SynetFusedLayerForward3<align, false>(src, _bias, _scale, dst, s);
                src += spatialF;
                dst += spatialF;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward3Nchw16c(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(dst))
                SynetFusedLayerForward3Nchw16c<true>(src, bias, scale, channels, spatial, dst);
            else
                SynetFusedLayerForward3Nchw16c<false>(src, bias, scale, channels, spatial, dst);
        }

        void SynetFusedLayerForward3(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst, SimdTensorFormatType format)
        {
            if (Base::NchwCompatible(channels, spatial, format))
                SynetFusedLayerForward3Nchw(src, bias, scale, channels, spatial, dst);
            else if (Base::NhwcCompatible(channels, spatial, format))
                SynetFusedLayerForward3Nhwc(src, bias, scale, channels, spatial, dst);
            else if (format == SimdTensorFormatNchw4c)
                Sse::SynetFusedLayerForward3(src, bias, scale, channels, spatial, dst, format);
            else if (format == SimdTensorFormatNchw8c)
                Avx::SynetFusedLayerForward3(src, bias, scale, channels, spatial, dst, format);
            else if (format == SimdTensorFormatNchw16c)
                SynetFusedLayerForward3Nchw16c(src, bias, scale, channels, spatial, dst);
            else
                Base::SynetFusedLayerForward3(src, bias, scale, channels, spatial, dst, format);
        }

        //---------------------------------------------------------------------

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward4(const float * src, const float * bias0, __m512 scale1, __m512 bias1, float * dst0, float * dst1, size_t offset, __mmask16 tail = -1)
        {
            __m512 x = _mm512_add_ps((Load<align, mask>(src + offset, tail)), (Load<align, mask>(bias0 + offset, tail)));
            Store<align, mask>(dst0 + offset, _mm512_max_ps(_mm512_setzero_ps(), x), tail);
            Store<align, mask>(dst1 + offset, _mm512_max_ps(_mm512_setzero_ps(), _mm512_fmadd_ps(x, scale1, bias1)), tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward4(const float * src, __m512 bias0, __m512 scale1, __m512 bias1, float * dst0, float * dst1, size_t offset, __mmask16 tail = -1)
        {
            __m512 x = _mm512_add_ps((Load<align, mask>(src + offset, tail)), bias0);
            Store<align, mask>(dst0 + offset, _mm512_max_ps(_mm512_setzero_ps(), x), tail);
            Store<align, mask>(dst1 + offset, _mm512_max_ps(_mm512_setzero_ps(), _mm512_fmadd_ps(x, scale1, bias1)), tail);
        }

        template <bool align> void SynetFusedLayerForward4Nchw(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst0)
        {
            if (align)
                assert(Aligned(src) && Aligned(spatial) && Aligned(dst));

            __m512 _bias1 = _mm512_set1_ps(bias1[0]);
            __m512 _scale1 = _mm512_set1_ps(scale1[0]);
            size_t aligned = AlignLo(spatial, QF);
            size_t partial = AlignLo(spatial, F);
            __mmask16 tail = TailMask16(spatial - partial);
            float * dst1 = dst0 + channels * spatial;
            for (size_t c = 0; c < channels; ++c)
            {
                size_t s = 0;
                __m512 _bias0 = _mm512_set1_ps(bias0[c]);
                for (; s < aligned; s += QF)
                {
                    SynetFusedLayerForward4<align, false>(src, _bias0, _scale1, _bias1, dst0, dst1, s + F * 0);
                    SynetFusedLayerForward4<align, false>(src, _bias0, _scale1, _bias1, dst0, dst1, s + F * 1);
                    SynetFusedLayerForward4<align, false>(src, _bias0, _scale1, _bias1, dst0, dst1, s + F * 2);
                    SynetFusedLayerForward4<align, false>(src, _bias0, _scale1, _bias1, dst0, dst1, s + F * 3);
                }
                for (; s < partial; s += F)
                    SynetFusedLayerForward4<align, false>(src, _bias0, _scale1, _bias1, dst0, dst1, s);
                if (s < spatial)
                    SynetFusedLayerForward4<align, true>(src, _bias0, _scale1, _bias1, dst0, dst1, s, tail);
                src += spatial;
                dst0 += spatial;
                dst1 += spatial;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward4Nchw(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(spatial) && Aligned(dst))
                SynetFusedLayerForward4Nchw<true>(src, bias0, scale1, bias1, channels, spatial, dst);
            else
                SynetFusedLayerForward4Nchw<false>(src, bias0, scale1, bias1, channels, spatial, dst);
        }

        template <bool align> void SynetFusedLayerForward4Nhwc(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst0)
        {
            if (align)
                assert(Aligned(src) && Aligned(bias0) && Aligned(channels) && Aligned(dst));

            __m512 _bias1 = _mm512_set1_ps(bias1[0]);
            __m512 _scale1 = _mm512_set1_ps(scale1[0]);
            size_t aligned = AlignLo(channels, QF);
            size_t partial = AlignLo(channels, F);
            __mmask16 tail = TailMask16(channels - partial);
            float * dst1 = dst0 + channels;
            for (size_t s = 0; s < spatial; ++s)
            {
                size_t c = 0;
                for (; c < aligned; c += QF)
                {
                    SynetFusedLayerForward4<align, false>(src, bias0, _scale1, _bias1, dst0, dst1, c + F * 0);
                    SynetFusedLayerForward4<align, false>(src, bias0, _scale1, _bias1, dst0, dst1, c + F * 1);
                    SynetFusedLayerForward4<align, false>(src, bias0, _scale1, _bias1, dst0, dst1, c + F * 2);
                    SynetFusedLayerForward4<align, false>(src, bias0, _scale1, _bias1, dst0, dst1, c + F * 3);
                }
                for (; c < partial; c += F)
                    SynetFusedLayerForward4<align, false>(src, bias0, _scale1, _bias1, dst0, dst1, c);
                if (c < channels)
                    SynetFusedLayerForward4<align, true>(src, bias0, _scale1, _bias1, dst0, dst1, c, tail);
                src += channels;
                dst0 += 2 * channels;
                dst1 += 2 * channels;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward4Nhwc(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(bias0) && Aligned(channels) && Aligned(dst))
                SynetFusedLayerForward4Nhwc<true>(src, bias0, scale1, bias1, channels, spatial, dst);
            else
                SynetFusedLayerForward4Nhwc<false>(src, bias0, scale1, bias1, channels, spatial, dst);
        }

        template <bool align> void SynetFusedLayerForward4Nchw16cA(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst0)
        {
            if (align)
                assert(Aligned(src) && Aligned(dst));

            __m512 _bias1 = _mm512_set1_ps(bias1[0]);
            __m512 _scale1 = _mm512_set1_ps(scale1[0]);
            size_t spatialF = spatial * F;
            size_t spatial4F = AlignLo(spatial, 4) * F;
            float * dst1 = dst0 + channels * spatial;
            for (size_t c = 0; c < channels; c += F)
            {
                __m512 _bias0 = Load<false>(bias0 + c);
                size_t s = 0;
                for (; s < spatial4F; s += 4 * F)
                {
                    SynetFusedLayerForward4<align, false>(src, _bias0, _scale1, _bias1, dst0, dst1, s + F * 0);
                    SynetFusedLayerForward4<align, false>(src, _bias0, _scale1, _bias1, dst0, dst1, s + F * 1);
                    SynetFusedLayerForward4<align, false>(src, _bias0, _scale1, _bias1, dst0, dst1, s + F * 2);
                    SynetFusedLayerForward4<align, false>(src, _bias0, _scale1, _bias1, dst0, dst1, s + F * 3);
                }
                for (; s < spatialF; s += F)
                    SynetFusedLayerForward4<align, false>(src, _bias0, _scale1, _bias1, dst0, dst1, s);
                src += spatialF;
                dst0 += spatialF;
                dst1 += spatialF;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward4Nchw16cA(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst)
        {
            assert(Aligned(channels));
            if (Aligned(src) && Aligned(dst))
                SynetFusedLayerForward4Nchw16cA<true>(src, bias0, scale1, bias1, channels, spatial, dst);
            else
                SynetFusedLayerForward4Nchw16cA<false>(src, bias0, scale1, bias1, channels, spatial, dst);
        }

        void SynetFusedLayerForward4(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst, SimdTensorFormatType format)
        {
            if (Base::NchwCompatible(channels, spatial, format))
                SynetFusedLayerForward4Nchw(src, bias0, scale1, bias1, channels, spatial, dst);
            else if (Base::NhwcCompatible(channels, spatial, format))
                SynetFusedLayerForward4Nhwc(src, bias0, scale1, bias1, channels, spatial, dst);
            else if (format == SimdTensorFormatNchw4c)
                Sse::SynetFusedLayerForward4(src, bias0, scale1, bias1, channels, spatial, dst, format);
            else if (format == SimdTensorFormatNchw8c)
                Avx::SynetFusedLayerForward4(src, bias0, scale1, bias1, channels, spatial, dst, format);
            else if (format == SimdTensorFormatNchw16c && Aligned(channels))
                SynetFusedLayerForward4Nchw16cA(src, bias0, scale1, bias1, channels, spatial, dst);
            else
                Base::SynetFusedLayerForward4(src, bias0, scale1, bias1, channels, spatial, dst, format);
        }

        //---------------------------------------------------------------------

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward8(const float * src0, const float * src1, const float * src2, float * dst, size_t offset, __mmask16 tail = -1)
        {
            Store<align, mask>(dst + offset, _mm512_add_ps((Load<align, mask>(src0 + offset, tail)), 
                _mm512_mul_ps((Load<align, mask>(src1 + offset, tail)), (Load<align, mask>(src2 + offset, tail)))), tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward8(const float * src0, const float * src1, const __m512 & src2, float * dst, size_t offset, __mmask16 tail = -1)
        {
            Store<align, mask>(dst + offset, _mm512_add_ps((Load<align, mask>(src0 + offset, tail)), 
                _mm512_mul_ps((Load<align, mask>(src1 + offset, tail)), src2)), tail);
        }

        template <bool align> void SynetFusedLayerForward8Nchw(const float * src0, const float * src1, const float * src2, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src0) && Aligned(src1) && Aligned(spatial) && Aligned(dst));

            size_t aligned = AlignLo(spatial, QF);
            size_t partial = AlignLo(spatial, F);
            __mmask16 tail = TailMask16(spatial - partial);
            for (size_t c = 0; c < channels; ++c)
            {
                size_t s = 0;
                __m512 _src2 = _mm512_set1_ps(src2[c]);
                for (; s < aligned; s += QF)
                {
                    SynetFusedLayerForward8<align, false>(src0, src1, _src2, dst, s + F * 0);
                    SynetFusedLayerForward8<align, false>(src0, src1, _src2, dst, s + F * 1);
                    SynetFusedLayerForward8<align, false>(src0, src1, _src2, dst, s + F * 2);
                    SynetFusedLayerForward8<align, false>(src0, src1, _src2, dst, s + F * 3);
                }
                for (; s < partial; s += F)
                    SynetFusedLayerForward8<align, false>(src0, src1, _src2, dst, s);
                if (s < spatial)
                    SynetFusedLayerForward8<align, true>(src0, src1, _src2, dst, s, tail);
                src0 += spatial;
                src1 += spatial;
                dst += spatial;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward8Nchw(const float * src0, const float * src1, const float * src2, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src0) && Aligned(src1) && Aligned(spatial) && Aligned(dst))
                SynetFusedLayerForward8Nchw<true>(src0, src1, src2, channels, spatial, dst);
            else
                SynetFusedLayerForward8Nchw<false>(src0, src1, src2, channels, spatial, dst);
        }

        template <bool align> void SynetFusedLayerForward8Nhwc(const float * src0, const float * src1, const float * src2, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src0) && Aligned(src1) && Aligned(src2) && Aligned(channels) && Aligned(dst));

            size_t aligned = AlignLo(channels, QF);
            size_t partial = AlignLo(channels, F);
            __mmask16 tail = TailMask16(channels - partial);
            for (size_t s = 0; s < spatial; ++s)
            {
                size_t c = 0;
                for (; c < aligned; c += QF)
                {
                    SynetFusedLayerForward8<align, false>(src0, src1, src2, dst, c + F * 0);
                    SynetFusedLayerForward8<align, false>(src0, src1, src2, dst, c + F * 1);
                    SynetFusedLayerForward8<align, false>(src0, src1, src2, dst, c + F * 2);
                    SynetFusedLayerForward8<align, false>(src0, src1, src2, dst, c + F * 3);
                }
                for (; c < partial; c += F)
                    SynetFusedLayerForward8<align, false>(src0, src1, src2, dst, c);
                if (c < channels)
                    SynetFusedLayerForward8<align, true>(src0, src1, src2, dst, c, tail);
                src0 += channels;
                src1 += channels;
                dst += channels;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward8Nhwc(const float * src0, const float * src1, const float * src2, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src0) && Aligned(src1) && Aligned(src2) && Aligned(channels) && Aligned(dst))
                SynetFusedLayerForward8Nhwc<true>(src0, src1, src2, channels, spatial, dst);
            else
                SynetFusedLayerForward8Nhwc<false>(src0, src1, src2, channels, spatial, dst);
        }

        template <bool align> void SynetFusedLayerForward8Nchw16c(const float * src0, const float * src1, const float * src2, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src0) && Aligned(src1) && Aligned(dst));

            size_t spatialF = spatial * F;
            size_t spatial4F = AlignLo(spatial, 4)*F;
            for (size_t c = 0; c < channels; c += F)
            {
                __m512 _src2 = Load<false>(src2 + c);
                size_t s = 0;
                for (; s < spatial4F; s += 4 * F)
                {
                    SynetFusedLayerForward8<align, false>(src0, src1, _src2, dst, s + F * 0);
                    SynetFusedLayerForward8<align, false>(src0, src1, _src2, dst, s + F * 1);
                    SynetFusedLayerForward8<align, false>(src0, src1, _src2, dst, s + F * 2);
                    SynetFusedLayerForward8<align, false>(src0, src1, _src2, dst, s + F * 3);
                }
                for (; s < spatialF; s += F)
                    SynetFusedLayerForward8<align, false>(src0, src1, _src2, dst, s);
                src0 += spatialF;
                src1 += spatialF;
                dst += spatialF;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward8Nchw16c(const float * src0, const float * src1, const float * src2, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src0) && Aligned(src1) && Aligned(dst))
                SynetFusedLayerForward8Nchw16c<true>(src0, src1, src2, channels, spatial, dst);
            else
                SynetFusedLayerForward8Nchw16c<false>(src0, src1, src2, channels, spatial, dst);
        }

        void SynetFusedLayerForward8(const float * src0, const float * src1, const float * src2, size_t channels, size_t spatial, float * dst, SimdTensorFormatType format)
        {
            if (Base::NchwCompatible(channels, spatial, format))
                SynetFusedLayerForward8Nchw(src0, src1, src2, channels, spatial, dst);
            else if (Base::NhwcCompatible(channels, spatial, format))
                SynetFusedLayerForward8Nhwc(src0, src1, src2, channels, spatial, dst);
            else if (format == SimdTensorFormatNchw4c)
                Sse::SynetFusedLayerForward8(src0, src1, src2, channels, spatial, dst, format);
            else if (format == SimdTensorFormatNchw8c)
                Avx::SynetFusedLayerForward8(src0, src1, src2, channels, spatial, dst, format);
            else if (format == SimdTensorFormatNchw16c)
                SynetFusedLayerForward8Nchw16c(src0, src1, src2, channels, spatial, dst);
            else
                Base::SynetFusedLayerForward8(src0, src1, src2, channels, spatial, dst, format);
        }

        //---------------------------------------------------------------------

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward9(const float * src, const float * scale, const float * bias, float * dst0, float * dst1, size_t offset, __mmask16 tail = -1)
        {
            __m512 _src = Load<align, mask>(src + offset, tail);
            __m512 _scale = Load<align, mask>(scale + offset, tail);
            __m512 _bias = Load<align, mask>(bias + offset, tail);
            Store<align, mask>(dst0 + offset, _mm512_max_ps(_mm512_setzero_ps(), _mm512_fmadd_ps(_src, _scale, _bias)), tail);
            Store<align, mask>(dst1 + offset, _src, tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward9(const float * src, const float * scale, const float * bias, float * dst0, size_t offset, __mmask16 tail = -1)
        {
            __m512 _src = Load<align, mask>(src + offset, tail);
            __m512 _scale = Load<align, mask>(scale + offset, tail);
            __m512 _bias = Load<align, mask>(bias + offset, tail);
            Store<align, mask>(dst0 + offset, _mm512_max_ps(_mm512_setzero_ps(), _mm512_fmadd_ps(_src, _scale, _bias)), tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward9(const float * src, const __m512 & scale, const __m512 & bias, float * dst0, float * dst1, size_t offset, __mmask16 tail = -1)
        {
            __m512 _src = Load<align, mask>(src + offset, tail);
            Store<align, mask>(dst0 + offset, _mm512_max_ps(_mm512_setzero_ps(), _mm512_fmadd_ps(_src, scale, bias)), tail);
            Store<align, mask>(dst1 + offset, _src, tail);
        }

        template <bool align, bool mask> SIMD_INLINE void SynetFusedLayerForward9(const float * src, const __m512 & scale, const __m512 & bias, float * dst0, size_t offset, __mmask16 tail = -1)
        {
            __m512 _src = Load<align, mask>(src + offset, tail);
            Store<align, mask>(dst0 + offset, _mm512_max_ps(_mm512_setzero_ps(), _mm512_fmadd_ps(_src, scale, bias)), tail);
        }

        template<bool align> void SynetFusedLayerForward9(const float * src0, const float * src1, const float * scale0, const float * bias0, size_t count0, size_t count1, size_t size, float * dst0, float * dst1, SimdBool trans)
        {
            if (align)
                assert((trans || size == 1 ? Aligned(count0) && Aligned(count1) && Aligned(scale0) && Aligned(bias0) : Aligned(size)) && Aligned(src0) && Aligned(src1) && Aligned(dst0) && Aligned(dst1));
            const float * scale1 = scale0 + count0;
            const float * bias1 = bias0 + count0;
            if (trans || size == 1)
            {
                size_t aligned0 = AlignLo(count0, QF);
                size_t partial0 = AlignLo(count0, F);
                __mmask16 tail0 = TailMask16(count0 - partial0);
                size_t aligned1 = AlignLo(count1, QF);
                size_t partial1 = AlignLo(count1, F);
                __mmask16 tail1 = TailMask16(count1 - partial1);
                if (dst1)
                {
                    for (size_t j = 0; j < size; ++j)
                    {
                        size_t i = 0;
                        for (; i < aligned0; i += QF)
                        {
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, dst1, i + 0 * F);
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, dst1, i + 1 * F);
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, dst1, i + 2 * F);
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, dst1, i + 3 * F);
                        }
                        for (; i < partial0; i += F)
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, dst1, i);
                        if(i < count0)
                            SynetFusedLayerForward9<align, true>(src0, scale0, bias0, dst0, dst1, i, tail0);
                        src0 += count0;
                        dst0 += count0;
                        dst1 += count0;
                        i = 0;
                        for (; i < aligned1; i += QF)
                        {
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, dst1, i + 0 * F);
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, dst1, i + 1 * F);
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, dst1, i + 2 * F);
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, dst1, i + 3 * F);
                        }
                        for (; i < partial1; i += F)
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, dst1, i);
                        if (i < count1)
                            SynetFusedLayerForward9<align, true>(src1, scale1, bias1, dst0, dst1, i, tail1);
                        src1 += count1;
                        dst0 += count1;
                        dst1 += count1;
                    }
                }
                else
                {
                    for (size_t j = 0; j < size; ++j)
                    {
                        size_t i = 0;
                        for (; i < aligned0; i += QF)
                        {
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, i + 0 * F);
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, i + 1 * F);
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, i + 2 * F);
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, i + 3 * F);
                        }
                        for (; i < partial0; i += F)
                            SynetFusedLayerForward9<align, false>(src0, scale0, bias0, dst0, i);
                        if (i < count0)
                            SynetFusedLayerForward9<align, true>(src0, scale0, bias0, dst0, i, tail0);
                        src0 += count0;
                        dst0 += count0;
                        i = 0;
                        for (; i < aligned1; i += QF)
                        {
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, i + 0 * F);
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, i + 1 * F);
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, i + 2 * F);
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, i + 3 * F);
                        }
                        for (; i < partial1; i += F)
                            SynetFusedLayerForward9<align, false>(src1, scale1, bias1, dst0, i);
                        if (i < count1)
                            SynetFusedLayerForward9<align, true>(src1, scale1, bias1, dst0, i, tail1);
                        src1 += count1;
                        dst0 += count1;
                    }
                }
            }
            else
            {
                size_t aligned = AlignLo(size, QF);
                size_t partial = AlignLo(size, F);
                __mmask16 tail = TailMask16(size - partial);
                if (dst1)
                {
                    for (size_t i = 0; i < count0; ++i, src0 += size, dst0 += size, dst1 += size)
                    {
                        size_t j = 0;
                        __m512 _scale0 = _mm512_set1_ps(scale0[i]);
                        __m512 _bias0 = _mm512_set1_ps(bias0[i]);
                        for (; j < aligned; j += QF)
                        {
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, dst1, j + 0 * F);
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, dst1, j + 1 * F);
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, dst1, j + 2 * F);
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, dst1, j + 3 * F);
                        }
                        for (; j < partial; j += F)
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, dst1, j);
                        if (j < size)
                            SynetFusedLayerForward9<align, true>(src0, _scale0, _bias0, dst0, dst1, j, tail);
                    }
                    for (size_t i = 0; i < count1; ++i, src1 += size, dst0 += size, dst1 += size)
                    {
                        size_t j = 0;
                        __m512 _scale1 = _mm512_set1_ps(scale1[i]);
                        __m512 _bias1 = _mm512_set1_ps(bias1[i]);
                        for (; j < aligned; j += QF)
                        {
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, dst1, j + 0 * F);
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, dst1, j + 1 * F);
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, dst1, j + 2 * F);
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, dst1, j + 3 * F);
                        }
                        for (; j < partial; j += F)
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, dst1, j);
                        if (j < size)
                            SynetFusedLayerForward9<align, true>(src1, _scale1, _bias1, dst0, dst1, j, tail);
                    }
                }
                else
                {
                    for (size_t i = 0; i < count0; ++i, src0 += size, dst0 += size)
                    {
                        size_t j = 0;
                        __m512 _scale0 = _mm512_set1_ps(scale0[i]);
                        __m512 _bias0 = _mm512_set1_ps(bias0[i]);
                        for (; j < aligned; j += QF)
                        {
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, j + 0 * F);
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, j + 1 * F);
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, j + 2 * F);
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, j + 3 * F);
                        }
                        for (; j < partial; j += F)
                            SynetFusedLayerForward9<align, false>(src0, _scale0, _bias0, dst0, j);
                        if (j < size)
                            SynetFusedLayerForward9<align, true>(src0, _scale0, _bias0, dst0, j, tail);
                    }
                    for (size_t i = 0; i < count1; ++i, src1 += size, dst0 += size)
                    {
                        size_t j = 0;
                        __m512 _scale1 = _mm512_set1_ps(scale1[i]);
                        __m512 _bias1 = _mm512_set1_ps(bias1[i]);
                        for (; j < aligned; j += QF)
                        {
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, j + 0 * F);
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, j + 1 * F);
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, j + 2 * F);
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, j + 3 * F);
                        }
                        for (; j < partial; j += F)
                            SynetFusedLayerForward9<align, false>(src1, _scale1, _bias1, dst0, j);
                        if (j < size)
                            SynetFusedLayerForward9<align, true>(src1, _scale1, _bias1, dst0, j, tail);
                    }
                }
            }
        }

        void SynetFusedLayerForward9(const float * src0, const float * src1, const float * scale0, const float * bias0, size_t count0, size_t count1, size_t size, float * dst0, float * dst1, SimdBool trans)
        {
            if ((trans || size == 1 ? Aligned(count0) && Aligned(count1) && Aligned(scale0) && Aligned(bias0) : Aligned(size)) && Aligned(src0) && Aligned(src1) && Aligned(dst0) && Aligned(dst1))
                SynetFusedLayerForward9<true>(src0, src1, scale0, bias0, count0, count1, size, dst0, dst1, trans);
            else
                SynetFusedLayerForward9<false>(src0, src1, scale0, bias0, count0, count1, size, dst0, dst1, trans);
        }
    }
#endif// SIMD_AVX512F_ENABLE
}