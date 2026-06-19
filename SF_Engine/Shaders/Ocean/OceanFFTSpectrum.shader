Shader "Ocean/OceanFFTSpectrum"
{
    ResourceLayout
    {
        [0,0] uniformbuf SpectrumUBO
        [0,1] storagebuf SpectrumParams      // StructuredBuffer<SpectrumParameters>
        [0,2] storageimg InitialSpectrumTex  // RWTexture2DArray<rgba32f>, 4 layers
        [0,3] storageimg SpectrumTex         // RWTexture2DArray<rgba32f>, 8 layers (disp+slope per cascade)
        [0,4] storageimg DisplacementTex     // RWTexture2DArray<rgba32f>, 4 layers
        [0,5] storageimg SlopeTex            // RWTexture2DArray<rg32f>, 4 layers
        [0,6] storageimg FourierTarget       // RWTexture2DArray<rgba32f>, 8 layers (FFT scratch)
    }

    ComputeShader
    {
        #pragma entry CS_InitializeSpectrum
        #Section Includes
        #import "OceanFFTCommon.si"

        layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

        struct SpectrumParameters
        {
            float scale;
            float angle;
            float spreadBlend;
            float swell;
            float alpha;
            float peakOmega;
            float gamma;
            float shortWavesFade;
        };

        layout(set = 0, binding = 1, std430) readonly buffer SpectrumParamsBuf
        {
            SpectrumParameters spectrums[]; // [8] -- 2 per cascade x 4 cascades
        };

        layout(set = 0, binding = 2, rgba32f) uniform image2DArray InitialSpectrumTex;

        float Dispersion(float kMag)
        {
            return sqrt(spectrum.gravity * kMag * tanh(min(kMag * spectrum.depth, 20.0)));
        }

        float DispersionDerivative(float kMag)
        {
            float th = tanh(min(kMag * spectrum.depth, 20.0));
            float ch = cosh(kMag * spectrum.depth);
            return spectrum.gravity * (spectrum.depth * kMag / (ch * ch) + th) / Dispersion(kMag) / 2.0;
        }

        float NormalizationFactor(float s)
        {
            float s2 = s * s, s3 = s2 * s, s4 = s3 * s;
            if (s < 5.0)
                return -0.000564 * s4 + 0.00776 * s3 - 0.044 * s2 + 0.192 * s + 0.163;
            return -4.80e-08 * s4 + 1.07e-05 * s3 - 9.53e-04 * s2 + 5.90e-02 * s + 3.93e-01;
        }

        float DonelanBannerBeta(float x)
        {
            if (x < 0.95) return 2.61 * pow(abs(x), 1.3);
            if (x < 1.6)  return 2.28 * pow(abs(x), -1.3);
            float p = -0.4 + 0.8393 * exp(-0.567 * log(x * x));
            return pow(10.0, p);
        }

        float DonelanBanner(float theta, float omega, float peakOmega)
        {
            float beta = DonelanBannerBeta(omega / peakOmega);
            float sech = 1.0 / cosh(beta * theta);
            return beta / (2.0 * tanh(beta * PI)) * sech * sech;
        }

        float Cosine2s(float theta, float s)
        {
            return NormalizationFactor(s) * pow(abs(cos(0.5 * theta)), 2.0 * s);
        }

        float SpreadPower(float omega, float peakOmega)
        {
            if (omega > peakOmega)
                return 9.77 * pow(abs(omega / peakOmega), -2.5);
            return 6.97 * pow(abs(omega / peakOmega), 5.0);
        }

        float DirectionSpectrum(float theta, float omega, SpectrumParameters s)
        {
            float spread = SpreadPower(omega, s.peakOmega)
                + 16.0 * tanh(min(omega / s.peakOmega, 20.0)) * s.swell * s.swell;
            return mix(2.0 / PI * cos(theta) * cos(theta), Cosine2s(theta - s.angle, spread), s.spreadBlend);
        }

        float TMACorrection(float omega)
        {
            float omegaH = omega * sqrt(spectrum.depth / spectrum.gravity);
            if (omegaH <= 1.0) return 0.5 * omegaH * omegaH;
            if (omegaH < 2.0)  return 1.0 - 0.5 * (2.0 - omegaH) * (2.0 - omegaH);
            return 1.0;
        }

        float JONSWAP(float omega, SpectrumParameters s)
        {
            float sigma = (omega <= s.peakOmega) ? 0.07 : 0.09;
            float r = exp(-(omega - s.peakOmega) * (omega - s.peakOmega)
                          / (2.0 * sigma * sigma * s.peakOmega * s.peakOmega));

            float invOmega = 1.0 / omega;
            float peakOverOmega = s.peakOmega / omega;
            return s.scale * TMACorrection(omega) * s.alpha * spectrum.gravity * spectrum.gravity
                 * invOmega * invOmega * invOmega * invOmega * invOmega
                 * exp(-1.25 * peakOverOmega * peakOverOmega * peakOverOmega * peakOverOmega)
                 * pow(abs(s.gamma), r);
        }

        float ShortWavesFade(float kLength, SpectrumParameters s)
        {
            return exp(-s.shortWavesFade * s.shortWavesFade * kLength * kLength);
        }

        void main()
        {
            uvec2 id = gl_GlobalInvocationID.xy;
            uint seed = id.x + spectrum.N * id.y + spectrum.N;
            seed += uint(spectrum.seed);

            float lengthScales[4] = float[4](
                spectrum.lengthScale0, spectrum.lengthScale1,
                spectrum.lengthScale2, spectrum.lengthScale3);

            for (int i = 0; i < 4; ++i)
            {
                float halfN = float(spectrum.N) * 0.5;
                float deltaK = 2.0 * PI / lengthScales[i];
                vec2 K = (vec2(id) - halfN) * deltaK;
                float kLength = length(K);

                seed += uint(i) + uint(hash(seed) * 10.0);
                vec4 u = vec4(hash(seed), hash(seed * 2u), hash(seed * 3u), hash(seed * 4u));
                vec2 gauss1 = UniformToGaussian(u.x, u.y);
                vec2 gauss2 = UniformToGaussian(u.z, u.w);

                vec4 result = vec4(0.0);
                if (spectrum.lowCutoff <= kLength && kLength <= spectrum.highCutoff)
                {
                    float kAngle = atan(K.y, K.x);
                    float omega = Dispersion(kLength);
                    float dOmegadk = DispersionDerivative(kLength);

                    float spec = JONSWAP(omega, spectrums[i * 2])
                                * DirectionSpectrum(kAngle, omega, spectrums[i * 2])
                                * ShortWavesFade(kLength, spectrums[i * 2]);

                    if (spectrums[i * 2 + 1].scale > 0.0)
                        spec += JONSWAP(omega, spectrums[i * 2 + 1])
                              * DirectionSpectrum(kAngle, omega, spectrums[i * 2 + 1])
                              * ShortWavesFade(kLength, spectrums[i * 2 + 1]);

                    vec2 hVal = vec2(gauss2.x, gauss1.y)
                        * sqrt(2.0 * spec * abs(dOmegadk) / kLength * deltaK * deltaK);
                    result = vec4(hVal, 0.0, 0.0);
                }

                imageStore(InitialSpectrumTex, ivec3(id, i), result);
            }
        }
    }

    ComputeShader
    {
        #pragma entry CS_PackSpectrumConjugate
        #Section Includes
        #import "OceanFFTCommon.si"

        layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
        layout(set = 0, binding = 2, rgba32f) uniform image2DArray InitialSpectrumTex;

        void main()
        {
            uvec2 id = gl_GlobalInvocationID.xy;
            uint N = spectrum.N;

            for (int i = 0; i < 4; ++i)
            {
                vec2 h0 = imageLoad(InitialSpectrumTex, ivec3(id, i)).rg;
                ivec2 conjCoord = ivec2((N - id.x) % N, (N - id.y) % N);
                vec2 h0conj = imageLoad(InitialSpectrumTex, ivec3(conjCoord, i)).rg;

                imageStore(InitialSpectrumTex, ivec3(id, i), vec4(h0, h0conj.x, -h0conj.y));
            }
        }
    }

    ComputeShader
    {
        #pragma entry CS_UpdateSpectrumForFFT
        #Section Includes
        #import "OceanFFTCommon.si"

        layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
        layout(set = 0, binding = 2, rgba32f) uniform readonly image2DArray InitialSpectrumTex;
        layout(set = 0, binding = 3, rgba32f) uniform image2DArray SpectrumTex;

        void main()
        {
            uvec2 id = gl_GlobalInvocationID.xy;
            float lengthScales[4] = float[4](
                spectrum.lengthScale0, spectrum.lengthScale1,
                spectrum.lengthScale2, spectrum.lengthScale3);

            for (int i = 0; i < 4; ++i)
            {
                vec4 initialSignal = imageLoad(InitialSpectrumTex, ivec3(id, i));
                vec2 h0 = initialSignal.xy;
                vec2 h0conj = initialSignal.zw;

                float halfN = float(spectrum.N) * 0.5;
                vec2 K = (vec2(id) - halfN) * 2.0 * PI / lengthScales[i];
                float kMag = length(K);
                float kMagRcp = kMag < 0.0001 ? 1.0 : (1.0 / kMag);

                float w0 = 2.0 * PI / spectrum.repeatTime;
                float dispersion = floor(sqrt(spectrum.gravity * kMag) / w0) * w0 * spectrum.frameTime;

                vec2 exponent = EulerFormula(dispersion);

                vec2 htilde = ComplexMult(h0, exponent) + ComplexMult(h0conj, vec2(exponent.x, -exponent.y));
                vec2 ih = vec2(-htilde.y, htilde.x);

                vec2 dispX = ih * K.x * kMagRcp;
                vec2 dispY = htilde;
                vec2 dispZ = ih * K.y * kMagRcp;

                vec2 dispX_dx = -htilde * K.x * K.x * kMagRcp;
                vec2 dispY_dx = ih * K.x;
                vec2 dispZ_dx = -htilde * K.x * K.y * kMagRcp;

                vec2 dispY_dz = ih * K.y;
                vec2 dispZ_dz = -htilde * K.y * K.y * kMagRcp;

                vec2 htildeDisplacementX = vec2(dispX.x - dispZ.y,    dispX.y + dispZ.x);
                vec2 htildeDisplacementZ = vec2(dispY.x - dispZ_dx.y, dispY.y + dispZ_dx.x);

                vec2 htildeSlopeX = vec2(dispY_dx.x - dispY_dz.y, dispY_dx.y + dispY_dz.x);
                vec2 htildeSlopeZ = vec2(dispX_dx.x - dispZ_dz.y, dispX_dx.y + dispZ_dz.x);

                imageStore(SpectrumTex, ivec3(id, i * 2),     vec4(htildeDisplacementX, htildeDisplacementZ));
                imageStore(SpectrumTex, ivec3(id, i * 2 + 1), vec4(htildeSlopeX, htildeSlopeZ));
            }
        }
    }

    ComputeShader
    {
        #pragma entry CS_HorizontalFFT
        #Section Includes
        #import "OceanFFTCommon.si"
        #import "OceanFFTButterfly.si"

        layout(local_size_x = 1024, local_size_y = 1, local_size_z = 1) in;
        layout(set = 0, binding = 6, rgba32f) uniform image2DArray FourierTarget;

        void main()
        {
            uvec2 id = gl_GlobalInvocationID.xy;
            for (int i = 0; i < 8; ++i)
            {
                vec4 v = imageLoad(FourierTarget, ivec3(id, i));
                v = FFT(int(gl_LocalInvocationID.x), v);
                imageStore(FourierTarget, ivec3(id, i), v);
            }
        }
    }

    ComputeShader
    {
        #pragma entry CS_VerticalFFT
        #Section Includes
        #import "OceanFFTCommon.si"
        #import "OceanFFTButterfly.si"

        layout(local_size_x = 1024, local_size_y = 1, local_size_z = 1) in;
        layout(set = 0, binding = 6, rgba32f) uniform image2DArray FourierTarget;

        void main()
        {
            uvec2 id = gl_GlobalInvocationID.xy;
            for (int i = 0; i < 8; ++i)
            {
                vec4 v = imageLoad(FourierTarget, ivec3(id.yx, i));
                v = FFT(int(gl_LocalInvocationID.x), v);
                imageStore(FourierTarget, ivec3(id.yx, i), v);
            }
        }
    }

    ComputeShader
    {
        #pragma entry CS_AssembleMaps
        #Section Includes
        #import "OceanFFTCommon.si"

        layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
        layout(set = 0, binding = 3, rgba32f) uniform readonly image2DArray SpectrumTex;
        layout(set = 0, binding = 4, rgba32f) uniform image2DArray DisplacementTex;
        layout(set = 0, binding = 5, rg32f)   uniform image2DArray SlopeTex;

        vec4 Permute(vec4 data, ivec3 id)
        {
            return data * (1.0 - 2.0 * float((id.x + id.y) % 2));
        }

        void main()
        {
            ivec2 id = ivec2(gl_GlobalInvocationID.xy);

            for (int i = 0; i < 4; ++i)
            {
                vec4 htildeDisplacement = Permute(imageLoad(SpectrumTex, ivec3(id, i * 2)), ivec3(id, i));
                vec4 htildeSlope        = Permute(imageLoad(SpectrumTex, ivec3(id, i * 2 + 1)), ivec3(id, i));

                vec2 dxdz   = htildeDisplacement.rg;
                vec2 dydxz  = htildeDisplacement.ba;
                vec2 dyxdyz = htildeSlope.rg;
                vec2 dxxdzz = htildeSlope.ba;

                vec2 lambda = spectrum.lambda;

                float jacobian = (1.0 + lambda.x * dxxdzz.x) * (1.0 + lambda.y * dxxdzz.y)
                               - lambda.x * lambda.y * dydxz.y * dydxz.y;

                vec3 displacement = vec3(lambda.x * dxdz.x, dydxz.x, lambda.y * dxdz.y);

                vec2 slopes = dyxdyz.xy / (1.0 + abs(dxxdzz * lambda));

                float foam = imageLoad(DisplacementTex, ivec3(id, i)).a;
                foam *= exp(-spectrum.foamDecayRate);
                foam = clamp(foam, 0.0, 1.0);

                float biasedJacobian = max(0.0, -(jacobian - spectrum.foamBias));
                if (biasedJacobian > spectrum.foamThreshold)
                    foam += spectrum.foamAdd * biasedJacobian;

                imageStore(DisplacementTex, ivec3(id, i), vec4(displacement, foam));
                imageStore(SlopeTex, ivec3(id, i), vec4(slopes, 0.0, 0.0));
            }
        }
    }
}