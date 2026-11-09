#pragma once
// Efectos de Saturno 8: una cadena por capa (distorsión, chorus, phaser y delay)
// y una cadena global (ecualizador y reverb). Diseño propio, sin dependencias de JUCE.

#include "../dsp/DspUtils.h"
#include <array>
#include <vector>

namespace s8
{
// Línea de retardo con lectura interpolada (lineal).
struct DelayLine
{
    std::vector<float> buf;
    int w = 0;
    void prepare (int maxSamples) { buf.assign ((size_t) std::max (maxSamples, 4), 0.0f); w = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }
    // d en muestras (>= 1)
    float read (float d) const
    {
        const int n = (int) buf.size();
        d = std::clamp (d, 1.0f, (float) (n - 2));
        float rp = (float) w - d;
        if (rp < 0) rp += (float) n;
        int i0 = (int) rp;
        if (i0 >= n) { i0 -= n; rp -= (float) n; }   // redondeo de coma flotante justo en el borde
        const int i1 = i0 + 1 >= n ? 0 : i0 + 1;
        const float f = rp - (float) i0;
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * f;
    }
};

// Biquad (RBJ) para el ecualizador.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void shelf (bool high, float hz, float db, float sr)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = kTwoPi * hz / sr;
        const float cw = std::cos (w), sw = std::sin (w), alpha = sw / 2.0f * std::sqrt (2.0f), sa = 2.0f * std::sqrt (A) * alpha;
        float B0, B1, B2, A0, A1, A2;
        if (! high)
        {
            B0 = A * ((A + 1) - (A - 1) * cw + sa); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - sa);
            A0 = (A + 1) + (A - 1) * cw + sa; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - sa;
        }
        else
        {
            B0 = A * ((A + 1) + (A - 1) * cw + sa); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - sa);
            A0 = (A + 1) - (A - 1) * cw + sa; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - sa;
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// ---------------------------------------------------------------- cadena de capa
struct LayerFxParams
{
    bool driveOn = false; float drive = 0.3f, driveTone = 0.6f, driveMix = 1.0f;
    bool chorusOn = false; int chorusMode = 0; float chorusMix = 0.5f;      // 0 = I, 1 = II, 2 = I+II
    bool phaserOn = false; float phaserRate = 0.3f, phaserDepth = 0.7f, phaserFeedback = 0.4f, phaserMix = 0.5f;
    bool delayOn = false; bool delaySync = true; int delayDiv = 4; float delayMs = 350;
    float delayFeedback = 0.35f, delayDamp = 0.4f, delayMix = 0.25f; bool pingPong = false;
};

// Figuras del delay: 1/16, 1/16 puntillo, 1/8T, 1/8, 1/8 puntillo, 1/4T, 1/4, 1/4 puntillo, 1/2
inline float delayDivisionBeats (int d)
{
    static const float beats[] { 0.25f, 0.375f, 1.0f / 3.0f, 0.5f, 0.75f, 2.0f / 3.0f, 1.0f, 1.5f, 2.0f };
    return beats[std::clamp (d, 0, 8)];
}

class LayerFx
{
public:
    LayerFxParams p;

    void prepare (float newSr)
    {
        sr = newSr;
        for (auto& d : chorus) d.prepare ((int) (sr * 0.03f));
        for (auto& d : delay) d.prepare ((int) (sr * 2.1f));
        clear();
    }
    void clear()
    {
        for (auto& d : chorus) d.clear();
        for (auto& d : delay) d.clear();
        for (auto& ch : ap) for (auto& s : ch) s = 0;
        phFb[0] = phFb[1] = 0; dampL = dampR = 0; toneL = toneR = 0;
        delayTime = -1;
    }

    void process (float& l, float& r, double bpm)
    {
        if (p.driveOn) drive (l, r);
        if (p.chorusOn) chorusFx (l, r);
        if (p.phaserOn) phaser (l, r);
        if (p.delayOn) delayFx (l, r, bpm);
    }

private:
    void drive (float& l, float& r)
    {
        const float gain = 1.0f + 30.0f * p.drive * p.drive;
        const float comp = 1.0f / std::tanh (gain * 0.5f) * 0.5f;
        const float a = 1.0f - std::exp (-kTwoPi * (800.0f + 14000.0f * p.driveTone * p.driveTone) / sr);
        auto one = [&] (float x, float& tone) {
            float y = std::tanh (gain * x + 0.1f * p.drive) - std::tanh (0.1f * p.drive);   // asimetría suave
            tone += (y - tone) * a;
            return x + (tone * comp - x) * p.driveMix;
        };
        l = one (l, toneL);
        r = one (r, toneR);
    }

    // Chorus de dos líneas BBD con LFO triangular en contrafase (estilo conjunto de cuerdas).
    void chorusFx (float& l, float& r)
    {
        static const float rate[] { 0.5f, 0.83f, 9.0f }, depthMs[] { 1.9f, 2.4f, 0.3f };
        const int m = std::clamp (p.chorusMode, 0, 2);
        chPhase += rate[m] / sr;
        if (chPhase >= 1.0f) chPhase -= 1.0f;
        const float tri = 1.0f - 4.0f * std::abs (chPhase - 0.5f);
        const float centre = 0.0035f * sr, depth = depthMs[m] * 0.001f * sr;
        const float mono = 0.5f * (l + r);
        chorus[0].push (mono); chorus[1].push (mono);
        const float wl = chorus[0].read (centre + depth * tri), wr = chorus[1].read (centre - depth * tri);
        l = l * (1.0f - 0.5f * p.chorusMix) + wl * p.chorusMix;
        r = r * (1.0f - 0.5f * p.chorusMix) + wr * p.chorusMix;
    }

    // Phaser estéreo de 4 etapas paso todo.
    void phaser (float& l, float& r)
    {
        phPhase += (0.05f + 6.0f * p.phaserRate * p.phaserRate) / sr;
        if (phPhase >= 1.0f) phPhase -= 1.0f;
        float* io[2] { &l, &r };
        for (int c = 0; c < 2; ++c)
        {
            float ph = phPhase + (c == 1 ? 0.25f : 0.0f);
            if (ph >= 1.0f) ph -= 1.0f;
            const float lfo = 0.5f + 0.5f * std::sin (kTwoPi * ph);
            const float hz = 200.0f * std::exp2 (p.phaserDepth * 4.5f * lfo);
            const float t = std::tan (kPi * std::min (hz, sr * 0.45f) / sr);
            const float a = (t - 1.0f) / (t + 1.0f);
            float x = *io[c] + phFb[c] * p.phaserFeedback * 0.9f;
            for (auto& s : ap[(size_t) c])
            {
                const float y = a * x + s;
                s = x - a * y;
                x = y;
            }
            phFb[c] = x;
            *io[c] = *io[c] * (1.0f - 0.5f * p.phaserMix) + x * p.phaserMix;
        }
    }

    void delayFx (float& l, float& r, double bpm)
    {
        const float target = p.delaySync ? (float) (60.0 / std::max (bpm, 20.0)) * delayDivisionBeats (p.delayDiv) * sr
                                         : p.delayMs * 0.001f * sr;
        if (delayTime < 0) delayTime = target;
        delayTime += (std::min (target, sr * 2.0f) - delayTime) * 0.0005f;   // cambios de tiempo sin chasquidos
        const float dl = delay[0].read (delayTime), dr = delay[1].read (delayTime);
        const float damp = 1.0f - std::exp (-kTwoPi * (1500.0f + 16000.0f * (1.0f - p.delayDamp)) / sr);
        dampL += (dl - dampL) * damp;
        dampR += (dr - dampR) * damp;
        const float fb = std::min (p.delayFeedback, 0.95f);
        if (p.pingPong)
        {
            delay[0].push (0.5f * (l + r) + dampR * fb);
            delay[1].push (dampL * fb);
        }
        else
        {
            delay[0].push (l + dampL * fb);
            delay[1].push (r + dampR * fb);
        }
        l += dl * p.delayMix;
        r += dr * p.delayMix;
    }

    float sr = 48000;
    std::array<DelayLine, 2> chorus, delay;
    float chPhase = 0, phPhase = 0;
    std::array<std::array<float, 4>, 2> ap {};
    float phFb[2] {}, dampL = 0, dampR = 0, toneL = 0, toneR = 0, delayTime = -1;
};

// ---------------------------------------------------------------- cadena global
struct GlobalFxParams
{
    bool eqOn = false; float eqLow = 0, eqHigh = 0;            // dB, ±12
    bool reverbOn = false; float reverbSize = 0.6f, reverbDamp = 0.5f, reverbPredelay = 20, reverbMix = 0.2f;
};

class GlobalFx
{
public:
    GlobalFxParams p;

    void prepare (float newSr)
    {
        sr = newSr;
        // Afinaciones de peine y paso todo propias (en muestras a 48 kHz), escaladas a la frecuencia real.
        static const int combs[8] { 1213, 1291, 1399, 1471, 1553, 1621, 1693, 1777 };
        static const int alls[4] { 601, 467, 367, 241 };
        const float k = sr / 48000.0f;
        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < 8; ++i) comb[(size_t) c][(size_t) i].prepare ((int) ((float) (combs[i] + 29 * c) * k) + 1);
            for (int i = 0; i < 4; ++i) allp[(size_t) c][(size_t) i].prepare ((int) ((float) (alls[i] + 13 * c) * k) + 1);
        }
        pre.prepare ((int) (sr * 0.2f));
        lastLow = lastHigh = 1000;
        clear();
    }
    void clear()
    {
        for (auto& ch : comb) for (auto& c : ch) { c.clear(); }
        for (auto& ch : allp) for (auto& a : ch) a.clear();
        for (auto& ch : combLp) ch.fill (0);
        pre.clear();
    }

    void process (float& l, float& r)
    {
        if (p.eqOn) eq (l, r);
        if (p.reverbOn) reverb (l, r);
    }

private:
    struct Line   // retardo fijo circular
    {
        std::vector<float> b; int i = 0;
        void prepare (int n) { b.assign ((size_t) std::max (n, 2), 0.0f); i = 0; }
        void clear() { std::fill (b.begin(), b.end(), 0.0f); }
        float& at() { return b[(size_t) i]; }
        void step() { if (++i >= (int) b.size()) i = 0; }
    };

    void eq (float& l, float& r)
    {
        if (p.eqLow != lastLow) { lastLow = p.eqLow; for (auto& b : low) b.shelf (false, 140.0f, p.eqLow, sr); }
        if (p.eqHigh != lastHigh) { lastHigh = p.eqHigh; for (auto& b : high) b.shelf (true, 5000.0f, p.eqHigh, sr); }
        // Una banda a 0 dB no se procesa: plano exacto y menos CPU.
        if (p.eqLow != 0.0f) { l = low[0].process (l); r = low[1].process (r); }
        if (p.eqHigh != 0.0f) { l = high[0].process (l); r = high[1].process (r); }
    }

    void reverb (float& l, float& r)
    {
        const float feedback = 0.70f + 0.28f * p.reverbSize;
        const float damp = 0.05f + 0.6f * p.reverbDamp;
        pre.push (0.5f * (l + r));
        const float in = pre.read (std::max (1.0f, p.reverbPredelay * 0.001f * sr)) * 0.03f;
        float out[2];
        for (int c = 0; c < 2; ++c)
        {
            float acc = 0;
            for (size_t i = 0; i < 8; ++i)
            {
                auto& line = comb[(size_t) c][i];
                const float y = line.at();
                combLp[(size_t) c][i] = y * (1.0f - damp) + combLp[(size_t) c][i] * damp;
                line.at() = in + combLp[(size_t) c][i] * feedback;
                line.step();
                acc += y;
            }
            for (auto& a : allp[(size_t) c])
            {
                const float buf = a.at();
                const float y = buf - acc;
                a.at() = acc + buf * 0.5f;
                a.step();
                acc = y;
            }
            out[c] = acc;
        }
        // La señal seca baja un poco al subir la mezcla para que el total no crezca.
        const float dry = 1.0f - 0.3f * p.reverbMix;
        l = l * dry + out[0] * p.reverbMix;
        r = r * dry + out[1] * p.reverbMix;
    }

    float sr = 48000, lastLow = 1000, lastHigh = 1000;
    std::array<Biquad, 2> low, high;
    std::array<std::array<Line, 8>, 2> comb;
    std::array<std::array<Line, 4>, 2> allp;
    std::array<std::array<float, 8>, 2> combLp {};
    DelayLine pre;
};
} // namespace s8
