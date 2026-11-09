#pragma once
// Utilidades DSP independientes de JUCE (se pueden probar fuera del plugin).
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace s8
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

inline float semitonesToRatio (float st) { return std::exp2 (st / 12.0f); }
inline float noteToHz (float note) { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }

// Ruido blanco rápido (xorshift32), salida en [-1, 1].
struct Noise
{
    uint32_t s = 0x12345678u;
    explicit Noise (uint32_t seed = 0x12345678u) : s (seed ? seed : 1u) {}
    float next()
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
};

// Corrección PolyBLEP para discontinuidades de salto (sierra, pulso).
inline float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

// Corrección PolyBLAMP para discontinuidades de pendiente (triángulo).
inline float polyBlamp (float t, float dt)
{
    if (t < dt) { t = t / dt - 1.0f; return -1.0f / 3.0f * t * t * t; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt + 1.0f; return 1.0f / 3.0f * t * t * t; }
    return 0.0f;
}

// Filtro de estado variable TPT (Zavalishin), 12 dB/oct. Estable al modular.
struct Svf
{
    float ic1 = 0, ic2 = 0;
    void reset() { ic1 = ic2 = 0; }
    // g = tan(pi fc / sr); k = 1/Q. Devuelve el paso bajo.
    float lowpass (float x, float g, float k)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }
};

// Paso bajo de 4 polos con realimentación sin retardo (estilo escalera de transistores),
// 24 dB/oct. k = 0..4 (autooscila cerca de 4). La saturación de entrada da el carácter.
struct Ladder
{
    float s[4] {};
    void reset() { for (auto& v : s) v = 0; }
    float process (float x, float g, float k)
    {
        const float G = g / (1.0f + g);
        const float sig = (s[0] * G * G * G + s[1] * G * G + s[2] * G + s[3]) / (1.0f + g);
        const float G4 = G * G * G * G;
        float u = (x - k * sig) / (1.0f + k * G4);
        u = std::tanh (u);
        for (int i = 0; i < 4; ++i)
        {
            const float v = (u - s[i]) * G;
            const float y = v + s[i];
            s[i] = y + v;
            u = y;
        }
        return u;
    }
};

// Paso alto de 1 polo (TPT), 6 dB/oct, sin resonancia.
struct OnePoleHp
{
    float s = 0;
    void reset() { s = 0; }
    float process (float x, float g)
    {
        const float G = g / (1.0f + g);
        const float v = (x - s) * G;
        const float lp = v + s;
        s = lp + v;
        return x - lp;
    }
};

// tan(pi f / sr) acotado, para los filtros TPT.
inline float tptG (float hz, float sr)
{
    hz = std::clamp (hz, 8.0f, sr * 0.45f);
    return std::tan (kPi * hz / sr);
}
} // namespace s8
