#include "vec.h"

#include <cassert>

// precomputed gelu table for f16 (128 KB)
wsp_ggml_fp16_t wsp_ggml_table_gelu_f16[1 << 16];

// precomputed quick gelu table for f16 (128 KB)
wsp_ggml_fp16_t wsp_ggml_table_gelu_quick_f16[1 << 16];

void wsp_ggml_vec_dot_f32(int n, float * WSP_GGML_RESTRICT s, size_t bs, const float * WSP_GGML_RESTRICT x, size_t bx, const float * WSP_GGML_RESTRICT y, size_t by, int nrc) {
   assert(nrc == 1);
   WSP_GGML_UNUSED(nrc);
   WSP_GGML_UNUSED(bx);
   WSP_GGML_UNUSED(by);
   WSP_GGML_UNUSED(bs);

#if defined(WSP_GGML_SIMD)
    float sumf = 0.0f;

    #if defined(__ARM_FEATURE_SVE)
        const int sve_register_length = wsp_ggml_cpu_get_sve_cnt() * 8;
        const int wsp_ggml_f32_epr = sve_register_length / 32;//8;//svcntw(); // SVE128:4, SVE256:8, SVE512:16
        const int wsp_ggml_f32_step = 8 * wsp_ggml_f32_epr; // choose 8 SVE registers

        const int np = (n & ~(wsp_ggml_f32_step - 1));
        svfloat32_t sum1 = svdup_n_f32(0.0f);
        svfloat32_t sum2 = svdup_n_f32(0.0f);
        svfloat32_t sum3 = svdup_n_f32(0.0f);
        svfloat32_t sum4 = svdup_n_f32(0.0f);
        svfloat32_t sum5 = svdup_n_f32(0.0f);
        svfloat32_t sum6 = svdup_n_f32(0.0f);
        svfloat32_t sum7 = svdup_n_f32(0.0f);
        svfloat32_t sum8 = svdup_n_f32(0.0f);
        svfloat32_t ax1,ax2,ax3,ax4,ax5,ax6,ax7,ax8;
        svfloat32_t ay1,ay2,ay3,ay4,ay5,ay6,ay7,ay8;
        for (int i = 0; i < np; i += wsp_ggml_f32_step) {
            ax1 = WSP_GGML_F32_VEC_LOAD(x + i);
            ay1 = WSP_GGML_F32_VEC_LOAD(y + i);
            sum1 = WSP_GGML_F32_VEC_FMA(ax1, ay1, sum1);

            ax2 = WSP_GGML_F32_VEC_LOAD(x + i + 1*wsp_ggml_f32_epr);
            ay2 = WSP_GGML_F32_VEC_LOAD(y + i + 1*wsp_ggml_f32_epr);
            sum2 = WSP_GGML_F32_VEC_FMA(ax2, ay2, sum2);

            ax3 = WSP_GGML_F32_VEC_LOAD(x + i + 2*wsp_ggml_f32_epr);
            ay3 = WSP_GGML_F32_VEC_LOAD(y + i + 2*wsp_ggml_f32_epr);
            sum3 = WSP_GGML_F32_VEC_FMA(ax3, ay3, sum3);

            ax4 = WSP_GGML_F32_VEC_LOAD(x + i + 3*wsp_ggml_f32_epr);
            ay4 = WSP_GGML_F32_VEC_LOAD(y + i + 3*wsp_ggml_f32_epr);
            sum4 = WSP_GGML_F32_VEC_FMA(ax4, ay4, sum4);

            ax5 = WSP_GGML_F32_VEC_LOAD(x + i + 4*wsp_ggml_f32_epr);
            ay5 = WSP_GGML_F32_VEC_LOAD(y + i + 4*wsp_ggml_f32_epr);
            sum5 = WSP_GGML_F32_VEC_FMA(ax5, ay5, sum5);

            ax6 = WSP_GGML_F32_VEC_LOAD(x + i + 5*wsp_ggml_f32_epr);
            ay6 = WSP_GGML_F32_VEC_LOAD(y + i + 5*wsp_ggml_f32_epr);
            sum6 = WSP_GGML_F32_VEC_FMA(ax6, ay6, sum6);

            ax7 = WSP_GGML_F32_VEC_LOAD(x + i + 6*wsp_ggml_f32_epr);
            ay7 = WSP_GGML_F32_VEC_LOAD(y + i + 6*wsp_ggml_f32_epr);
            sum7 = WSP_GGML_F32_VEC_FMA(ax7, ay7, sum7);

            ax8 = WSP_GGML_F32_VEC_LOAD(x + i + 7*wsp_ggml_f32_epr);
            ay8 = WSP_GGML_F32_VEC_LOAD(y + i + 7*wsp_ggml_f32_epr);
            sum8 = WSP_GGML_F32_VEC_FMA(ax8, ay8, sum8);
        }
        // leftovers
        // Since 8 unrolls are done in above loop, leftovers lie in range [0, wsp_ggml_f32_step] which is handled in below loop
        const int np2 = (n & ~(wsp_ggml_f32_epr - 1));
        for (int i = np; i < np2; i += wsp_ggml_f32_epr) {
            ax1 = WSP_GGML_F32_VEC_LOAD(x + i);
            ay1 = WSP_GGML_F32_VEC_LOAD(y + i);
            sum1 = WSP_GGML_F32_VEC_FMA(ax1, ay1, sum1);
        }
        // maximum number of leftover elements will be less that wsp_ggml_f32_epr. Apply predicated svmad on available elements only
        if (np2 < n) {
            svbool_t pg = svwhilelt_b32(np2, n);
            ax1 = svld1_f32(pg, x + np2);
            ay1 = svld1_f32(pg, y + np2);
            sum1 = svmad_f32_m(pg, ax1, ay1, sum1);
        }
        // reduce sum1,sum2 to sum1
        WSP_GGML_F32_VEC_REDUCE(sumf, sum1, sum2, sum3, sum4, sum5, sum6, sum7, sum8);
    #else
        const int np = (n & ~(WSP_GGML_F32_STEP - 1));

        WSP_GGML_F32_VEC sum[WSP_GGML_F32_ARR] = { WSP_GGML_F32_VEC_ZERO };

        WSP_GGML_F32_VEC ax[WSP_GGML_F32_ARR];
        WSP_GGML_F32_VEC ay[WSP_GGML_F32_ARR];

        for (int i = 0; i < np; i += WSP_GGML_F32_STEP) {
            for (int j = 0; j < WSP_GGML_F32_ARR; j++) {
                ax[j] = WSP_GGML_F32_VEC_LOAD(x + i + j*WSP_GGML_F32_EPR);
                ay[j] = WSP_GGML_F32_VEC_LOAD(y + i + j*WSP_GGML_F32_EPR);

                sum[j] = WSP_GGML_F32_VEC_FMA(sum[j], ax[j], ay[j]);
            }
        }

        // reduce sum0..sum3 to sum0
        WSP_GGML_F32_VEC_REDUCE(sumf, sum);

        // leftovers
        for (int i = np; i < n; ++i) {
            sumf += x[i]*y[i];
        }
    #endif
#else
    // scalar
    wsp_ggml_float sumf = 0.0;
    for (int i = 0; i < n; ++i) {
        sumf += (wsp_ggml_float)(x[i]*y[i]);
    }
#endif

    *s = sumf;
}

void wsp_ggml_vec_dot_bf16(int n, float * WSP_GGML_RESTRICT s, size_t bs, wsp_ggml_bf16_t * WSP_GGML_RESTRICT x, size_t bx, wsp_ggml_bf16_t * WSP_GGML_RESTRICT y, size_t by, int nrc) {
    assert(nrc == 1);
    WSP_GGML_UNUSED(nrc);
    WSP_GGML_UNUSED(bx);
    WSP_GGML_UNUSED(by);
    WSP_GGML_UNUSED(bs);
    int i = 0;
    wsp_ggml_float sumf = 0;

#if defined(__AVX512BF16__)
    __m512 c1 = _mm512_setzero_ps();
    __m512 c2 = _mm512_setzero_ps();
    for (; i + 64 <= n; i += 64) {
        c1 = _mm512_dpbf16_ps(c1, m512bh(_mm512_loadu_si512((x + i))),
                             m512bh(_mm512_loadu_si512((y + i))));
        c2 = _mm512_dpbf16_ps(c2, m512bh(_mm512_loadu_si512((x + i + 32))),
                             m512bh(_mm512_loadu_si512((y + i + 32))));
    }
    sumf += (wsp_ggml_float)_mm512_reduce_add_ps(c1);
    sumf += (wsp_ggml_float)_mm512_reduce_add_ps(c2);

#elif defined(__AVX512F__)
#define LOAD(p) _mm512_castsi512_ps(_mm512_slli_epi32(_mm512_cvtepu16_epi32(_mm256_loadu_si256((const __m256i *)(p))), 16))
    __m512 c1 = _mm512_setzero_ps();
    __m512 c2 = _mm512_setzero_ps();
    for (; i + 32 <= n; i += 32) {
        c1 = _mm512_add_ps(_mm512_mul_ps(LOAD(x + i), LOAD(y + i)), c1);
        c2 = _mm512_add_ps(_mm512_mul_ps(LOAD(x + i + 16), LOAD(y + i + 16)), c2);
    }
    sumf += (wsp_ggml_float)_mm512_reduce_add_ps(c1);
    sumf += (wsp_ggml_float)_mm512_reduce_add_ps(c2);

#undef LOAD
#elif defined(__AVX2__) || defined(__AVX__)
#if defined(__AVX2__)
#define LOAD(p) _mm256_castsi256_ps(_mm256_slli_epi32(_mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i *)(p))), 16))
#else
#define LOAD(p) _mm256_castsi256_ps(_mm256_insertf128_si256(_mm256_castsi128_si256(_mm_slli_epi32(_mm_cvtepu16_epi32(_mm_loadu_si128((const __m128i *)(p))), 16)), (_mm_slli_epi32(_mm_cvtepu16_epi32(_mm_bsrli_si128(_mm_loadu_si128((const __m128i *)(p)), 8)), 16)), 1))
#endif
    __m256 c1 = _mm256_setzero_ps();
    __m256 c2 = _mm256_setzero_ps();
    __m256 c3 = _mm256_setzero_ps();
    __m256 c4 = _mm256_setzero_ps();
    for (; i + 32 <= n; i += 32) {
        c1 = _mm256_add_ps(_mm256_mul_ps(LOAD(x + i), LOAD(y + i)), c1);
        c2 = _mm256_add_ps(_mm256_mul_ps(LOAD(x + i + 8), LOAD(y + i + 8)), c2);
        c3 = _mm256_add_ps(_mm256_mul_ps(LOAD(x + i + 16), LOAD(y + i + 16)), c3);
        c4 = _mm256_add_ps(_mm256_mul_ps(LOAD(x + i + 24), LOAD(y + i + 24)), c4);
    }
    __m128 g;
    c1 = _mm256_add_ps(_mm256_add_ps(c1, c3),
                       _mm256_add_ps(c2, c4));
    g = _mm_add_ps(_mm256_extractf128_ps(c1, 1),
                   _mm256_castps256_ps128(c1));
    g = _mm_add_ps(g, _mm_movehl_ps(g, g));
    g = _mm_add_ss(g, _mm_movehdup_ps(g));
    sumf += (wsp_ggml_float)_mm_cvtss_f32(g);

#undef LOAD
#endif

    for (; i < n; ++i) {
        sumf += (wsp_ggml_float)(WSP_GGML_BF16_TO_FP32(x[i]) *
                             WSP_GGML_BF16_TO_FP32(y[i]));
    }
    *s = sumf;
}

void wsp_ggml_vec_dot_f16(int n, float * WSP_GGML_RESTRICT s, size_t bs, wsp_ggml_fp16_t * WSP_GGML_RESTRICT x, size_t bx, wsp_ggml_fp16_t * WSP_GGML_RESTRICT y, size_t by, int nrc) {
    assert(nrc == 1);
    WSP_GGML_UNUSED(nrc);
    WSP_GGML_UNUSED(bx);
    WSP_GGML_UNUSED(by);
    WSP_GGML_UNUSED(bs);

    wsp_ggml_float sumf = 0.0;

#if defined(WSP_GGML_SIMD)
    const int np = (n & ~(WSP_GGML_F16_STEP - 1));

    WSP_GGML_F16_VEC sum[WSP_GGML_F16_ARR] = { WSP_GGML_F16_VEC_ZERO };

    WSP_GGML_F16_VEC ax[WSP_GGML_F16_ARR];
    WSP_GGML_F16_VEC ay[WSP_GGML_F16_ARR];

    for (int i = 0; i < np; i += WSP_GGML_F16_STEP) {
        for (int j = 0; j < WSP_GGML_F16_ARR; j++) {
            ax[j] = WSP_GGML_F16_VEC_LOAD(x + i + j*WSP_GGML_F16_EPR, j);
            ay[j] = WSP_GGML_F16_VEC_LOAD(y + i + j*WSP_GGML_F16_EPR, j);

            sum[j] = WSP_GGML_F16_VEC_FMA(sum[j], ax[j], ay[j]);
        }
    }

    // reduce sum0..sum3 to sum0
    WSP_GGML_F16_VEC_REDUCE(sumf, sum);

    // leftovers
    for (int i = np; i < n; ++i) {
        sumf += (wsp_ggml_float)(WSP_GGML_CPU_FP16_TO_FP32(x[i])*WSP_GGML_CPU_FP16_TO_FP32(y[i]));
    }
#else
    for (int i = 0; i < n; ++i) {
        sumf += (wsp_ggml_float)(WSP_GGML_CPU_FP16_TO_FP32(x[i])*WSP_GGML_CPU_FP16_TO_FP32(y[i]));
    }
#endif

    *s = sumf;
}

void wsp_ggml_vec_silu_f32(const int n, float * y, const float * x) {
    int i = 0;
#if defined(__AVX512F__) && defined(__AVX512DQ__)
    for (; i + 15 < n; i += 16) {
        _mm512_storeu_ps(y + i, wsp_ggml_v_silu(_mm512_loadu_ps(x + i)));
    }
#elif defined(__AVX2__) && defined(__FMA__)
    for (; i + 7 < n; i += 8) {
        _mm256_storeu_ps(y + i, wsp_ggml_v_silu(_mm256_loadu_ps(x + i)));
    }
#elif defined(__SSE2__)
    for (; i + 3 < n; i += 4) {
        _mm_storeu_ps(y + i, wsp_ggml_v_silu(_mm_loadu_ps(x + i)));
    }
#elif defined(__ARM_NEON) && defined(__aarch64__)
    for (; i + 3 < n; i += 4) {
        vst1q_f32(y + i, wsp_ggml_v_silu(vld1q_f32(x + i)));
    }
#endif
    for (; i < n; ++i) {
        y[i] = wsp_ggml_silu_f32(x[i]);
    }
}

void wsp_ggml_vec_swiglu_f32(const int n, float * y, const float * x, const float * g) {
    int i = 0;
#if defined(__AVX512F__) && defined(__AVX512DQ__)
    for (; i + 15 < n; i += 16) {
        _mm512_storeu_ps(y + i, _mm512_mul_ps(wsp_ggml_v_silu(_mm512_loadu_ps(x + i)), _mm512_loadu_ps(g + i)));
    }
#elif defined(__AVX2__) && defined(__FMA__)
    for (; i + 7 < n; i += 8) {
        _mm256_storeu_ps(y + i, _mm256_mul_ps(wsp_ggml_v_silu(_mm256_loadu_ps(x + i)), _mm256_loadu_ps(g + i)));
    }
#elif defined(__SSE2__)
    for (; i + 3 < n; i += 4) {
        _mm_storeu_ps(y + i, _mm_mul_ps(wsp_ggml_v_silu(_mm_loadu_ps(x + i)), _mm_loadu_ps(g + i)));
    }
#elif defined(__ARM_NEON) && defined(__aarch64__)
    for (; i + 3 < n; i += 4) {
        vst1q_f32(y + i, vmulq_f32(wsp_ggml_v_silu(vld1q_f32(x + i)), vld1q_f32(g + i)));
    }
#endif
    for (; i < n; ++i) {
        y[i] = wsp_ggml_silu_f32(x[i]) * g[i];
    }
}

wsp_ggml_float wsp_ggml_vec_soft_max_f32(const int n, float * y, const float * x, float max) {
    int i = 0;
    wsp_ggml_float sum = 0;
#if defined(__AVX512F__) && defined(__AVX512DQ__)
    for (; i + 15 < n; i += 16) {
        __m512 val = wsp_ggml_v_expf(_mm512_sub_ps(_mm512_loadu_ps(x + i),
                                               _mm512_set1_ps(max)));
        _mm512_storeu_ps(y + i, val);
        sum += (wsp_ggml_float)_mm512_reduce_add_ps(val);
    }
#elif defined(__AVX2__) && defined(__FMA__)
    for (; i + 7 < n; i += 8) {
        __m256 val = wsp_ggml_v_expf(_mm256_sub_ps(_mm256_loadu_ps(x + i),
                                               _mm256_set1_ps(max)));
        _mm256_storeu_ps(y + i, val);
        __m128 val2 = _mm_add_ps(_mm256_extractf128_ps(val, 1),
                                 _mm256_castps256_ps128(val));
        val2 = _mm_add_ps(val2, _mm_movehl_ps(val2, val2));
        val2 = _mm_add_ss(val2, _mm_movehdup_ps(val2));
        sum += (wsp_ggml_float)_mm_cvtss_f32(val2);
    }
#elif defined(__SSE2__)
    for (; i + 3 < n; i += 4) {
        __m128 val = wsp_ggml_v_expf(_mm_sub_ps(_mm_loadu_ps(x + i),
                                            _mm_set1_ps(max)));
        _mm_storeu_ps(y + i, val);
#if defined(__AVX__) || defined(__AVX2__) || defined(__AVX512F__)
        val = _mm_add_ps(val, _mm_movehl_ps(val, val));
        val = _mm_add_ss(val, _mm_movehdup_ps(val));
#else
        __m128 tmp = _mm_shuffle_ps(val, val, _MM_SHUFFLE(2, 3, 0, 1));
        val = _mm_add_ps(val, tmp);
        tmp = _mm_movehl_ps(tmp, val);
        val = _mm_add_ss(val, tmp);
#endif
        sum += (wsp_ggml_float)_mm_cvtss_f32(val);
    }
#elif defined(__ARM_NEON) && defined(__aarch64__)
    for (; i + 3 < n; i += 4) {
        float32x4_t val = wsp_ggml_v_expf(vsubq_f32(vld1q_f32(x + i),
                                                vdupq_n_f32(max)));
        vst1q_f32(y + i, val);
        sum += (wsp_ggml_float)vaddvq_f32(val);
    }
#endif
    for (; i < n; ++i) {
        float val = expf(x[i] - max);
        sum += (wsp_ggml_float)val;
        y[i] = val;
    }
    return sum;
}

wsp_ggml_float wsp_ggml_vec_log_soft_max_f32(const int n, float * y, const float * x, float max) {
    // log(soft_max) = log(soft_max_i / soft_max_sum) = log(soft_max_i) - log(soft_max_sum) = (logit_i - max) - log(soft_max_i)

    int i = 0;
    wsp_ggml_float sum = 0;
    for (; i < n; ++i) {
        float val = x[i] - max;
        y[i] = val;
        sum += (wsp_ggml_float)expf(val);
    }
    return sum = (wsp_ggml_float)logf(sum);
}
