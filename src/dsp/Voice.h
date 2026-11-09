#pragma once
// Voz de Saturno 8: dos osciladores, mezclador, filtro paso alto, filtro paso bajo
// de 12 o 24 dB, dos envolventes ADSR y VCA. Diseño propio inspirado en la
// arquitectura de los polifónicos analógicos de 8 voces de principios de los 80.
// Sin dependencias de JUCE, para poder probarlo y renderizarlo fuera del plugin.

#include "DspUtils.h"
#include <array>

namespace s8
{
enum class Wave1 { Triangle, Saw, Pulse, Square };
enum class Wave2 { Sine, Saw, Pulse, Noise };
enum class LfoWave { Sine, Triangle, Saw, Square, Random };
enum class ModDest { Vco1, Vco2, Both };
enum class PwmSrc { Lfo, Env1 };

// Matriz de modulación: 4 ranuras por capa, fuente -> destino con cantidad bipolar.
enum class ModSrc { None, Lfo, Env1, Env2, Velocity, Key, Wheel, Aftertouch, Bend, Random, Count };
enum class ModDst { None, Pitch, Pitch1, Pitch2, PulseWidth, CrossMod, Vco2Level, Hpf, Cutoff, Resonance, Level, Pan, Count };
constexpr int kModSlots = 4;
struct ModSlot { ModSrc src = ModSrc::None; ModDst dst = ModDst::None; float amount = 0; };   // amount -1..1

// Parámetros de una capa (los comparten todas sus voces). Rangos en unidades reales.
struct LayerParams
{
    // Osciladores
    int   vco1Range = 1;        // 0 = 16', 1 = 8', 2 = 4', 3 = 2'
    Wave1 vco1Wave = Wave1::Saw;
    float crossMod = 0.0f;      // 0..1: FM exponencial de VCO-2 sobre VCO-1
    int   vco2Range = 1;        // 0 = 16', 1 = 8', 2 = 4', 3 = 2'
    float vco2Fine = 0.0f;      // semitonos -12..+12
    Wave2 vco2Wave = Wave2::Saw;
    bool  vco2Low = false;      // VCO-2 a baja frecuencia (sin seguir el teclado)
    bool  sync = false;         // VCO-2 se reinicia con cada ciclo de VCO-1

    // Modulador de los osciladores
    float vcoLfo = 0.0f;        // 0..1 (hasta ±1 octava)
    float vcoEnv = 0.0f;        // 0..1 (ENV-1, hasta +2 octavas)
    ModDest vcoModDest = ModDest::Both;
    float pulseWidth = 0.0f;    // 0..1 (0 = cuadrada, 1 = pulso muy estrecho)
    float pwmAmount = 0.0f;     // 0..1
    PwmSrc pwmSrc = PwmSrc::Lfo;

    // Mezclador
    float vco1Level = 1.0f;     // 0..1
    float vco2Level = 0.0f;     // 0..1

    // Filtros
    float hpf = 0.0f;           // 0..1 (20 Hz .. 2 kHz)
    float cutoff = 0.7f;        // 0..1 (20 Hz .. 20 kHz, exponencial)
    float resonance = 0.0f;     // 0..1
    bool  slope24 = true;
    float vcfEnv = 0.0f;        // 0..1 (hasta 8 octavas)
    bool  vcfEnv2 = false;      // false = ENV-1, true = ENV-2
    float vcfLfo = 0.0f;        // 0..1 (hasta ±3 octavas)
    float keyFollow = 0.5f;     // 0..1

    // VCA
    float level = 0.8f;         // 0..1
    float vcaLfo = 0.0f;        // 0..1 (trémolo)

    // Envolventes (segundos y 0..1)
    float a1 = 0.005f, d1 = 0.4f, s1 = 0.0f, r1 = 0.3f;
    float key1 = 0.0f, vel1 = 0.0f;
    bool  env1Invert = false;
    float a2 = 0.005f, d2 = 0.3f, s2 = 1.0f, r2 = 0.3f;
    float key2 = 0.0f, vel2 = 0.5f;

    // LFO (uno por capa, como en los polifónicos de la época)
    float lfoRate = 4.0f;       // Hz
    float lfoDelay = 0.0f;      // s (fundido de entrada desde la primera nota)
    LfoWave lfoWave = LfoWave::Triangle;

    std::array<ModSlot, kModSlots> matrix {};
};

// ADSR analógica: ataque hacia un objetivo por encima de 1, caídas exponenciales.
struct Adsr
{
    enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
    float value = 0;

    void gateOn (bool retrigger) { if (retrigger || stage == Idle || stage == Release) stage = Attack; }
    void gateOff() { if (stage != Idle) stage = Release; }
    void kill() { stage = Idle; value = 0; }
    bool active() const { return stage != Idle; }

    // timeScale < 1 acorta los tiempos (seguimiento de teclado).
    float process (float a, float d, float s, float r, float timeScale, float sr)
    {
        auto coef = [sr, timeScale] (float t) { return 1.0f - std::exp (-1.0f / (std::max (t * timeScale, 0.0005f) * sr)); };
        switch (stage)
        {
            case Attack:
                value += (1.25f - value) * coef (a * 0.6f);
                if (value >= 1.0f) { value = 1.0f; stage = Decay; }
                break;
            case Decay:
                value += (s - value) * coef (d * 0.3f);
                if (std::abs (value - s) < 1.0e-4f) stage = Sustain;
                break;
            case Sustain: value = s; break;
            case Release:
                value -= value * coef (r * 0.3f);
                if (value < 1.0e-4f) { value = 0; stage = Idle; }   // -80 dB
                break;
            case Idle: break;
        }
        return value;
    }
};

// LFO de capa. Salida -1..1 y fundido de entrada (delay).
struct Lfo
{
    float phase = 0, out = 0, fade = 1, held = 0;
    Noise rnd { 0x5a7u };
    void restartFade() { fade = 0; }
    float process (const LayerParams& p, float sr)
    {
        phase += p.lfoRate / sr;
        if (phase >= 1.0f) { phase -= 1.0f; held = rnd.next(); }
        switch (p.lfoWave)
        {
            case LfoWave::Sine:     out = std::sin (kTwoPi * phase); break;
            case LfoWave::Triangle: out = 1.0f - 4.0f * std::abs (phase - 0.5f); break;
            case LfoWave::Saw:      out = 1.0f - 2.0f * phase; break;
            case LfoWave::Square:   out = phase < 0.5f ? 1.0f : -1.0f; break;
            case LfoWave::Random:   out = held; break;
        }
        if (fade < 1.0f) fade = p.lfoDelay <= 0.0f ? 1.0f : std::min (1.0f, fade + 1.0f / (p.lfoDelay * sr));
        return out * fade;
    }
};

// Controles de interpretación de la capa: bender, rueda de modulación, portamento y deriva.
struct PlayParams
{
    float bendVco1 = 0, bendVco2 = 0;   // semitonos ya escalados por el bender
    float bendVcf = 0;                  // octavas
    float wheelVco = 0;                 // semitonos de LFO añadidos por la rueda
    float wheelVcf = 0;                 // octavas de LFO añadidas por la rueda
    float glide = 0;                    // s (0 = sin portamento)
    float driftVco = 0, driftVcf = 0, driftVca = 0, envSlop = 0;   // 0..1
    float tuneSt = 0;                   // afinación general en semitonos
    float wheel = 0, aftertouch = 0, bend = 0;   // valores en bruto para la matriz (0..1, 0..1, -1..1)
};

// Valores que cambian por voz además de la nota (unísono, desafinación).
struct VoiceOffsets
{
    float detuneSt = 0;   // semitonos
    float pan = 0;        // -1..1
    float gain = 1;       // fundido entre capas en modo división
};

class Voice
{
public:
    void setSampleRate (float newSr) { sr = newSr; }
    void setSeed (uint32_t seed)
    {
        noise = Noise (seed);
        drift = Noise (seed ^ 0xa5a5a5a5u);
        // Tolerancia fija de cada voz (componentes que no son idénticos).
        for (auto& t : tol) t = drift.next();
    }

    // fromPitch >= 0: la voz arranca en ese tono y se desliza hasta la nota (portamento).
    void start (int midiNote, float velocity, bool retrigger, uint64_t stamp, float fromPitch = -1)
    {
        const bool wasActive = active();
        if (! active())
        {
            ph1 = 0; ph2 = 0;
            lp12.reset(); ladder.reset(); hp.reset();
        }
        note = midiNote;
        target = (float) midiNote;
        if (fromPitch >= 0) pitch = fromPitch;
        else if (! wasActive || retrigger) pitch = target;
        vel = velocity;
        randomValue = drift.next();
        slopA = 1.0f + 0.3f * drift.next(); slopD = 1.0f + 0.3f * drift.next(); slopR = 1.0f + 0.3f * drift.next();
        gate = true;
        age = stamp;
        env1.gateOn (retrigger);
        env2.gateOn (retrigger);
    }
    // Cambio de nota sin redisparar (legato en modo solo).
    void glideTo (int midiNote) { note = midiNote; target = (float) midiNote; }
    void release (uint64_t stamp) { if (gate) { gate = false; age = stamp; env1.gateOff(); env2.gateOff(); } }
    void kill() { gate = false; env1.kill(); env2.kill(); }

    bool active() const { return env2.active(); }
    bool held() const { return gate; }
    int  currentNote() const { return note; }
    uint64_t stamp() const { return age; }

    VoiceOffsets offs;

    // lfo: salida del LFO de la capa (-1..1). Suma el resultado a l y r.
    float currentPitch() const { return pitch; }

    void process (const LayerParams& p, const PlayParams& pp, float lfo, float& l, float& r)
    {
        if (! active()) return;

        // Portamento: el tono se acerca a la nota de forma exponencial.
        if (pp.glide > 0.0005f) pitch += (target - pitch) * (1.0f - std::exp (-1.0f / (pp.glide * 0.35f * sr)));
        else pitch = target;

        // Deriva analógica: un paseo aleatorio lento por voz, más la tolerancia fija de la voz.
        if (--driftCount <= 0)
        {
            driftCount = (int) (sr * (0.4f + 0.6f * std::abs (drift.next())));
            for (auto& t : driftTarget) t = drift.next();
        }
        const float dc = 1.0f / (0.8f * sr);
        for (size_t i = 0; i < driftNow.size(); ++i) driftNow[i] += (driftTarget[i] - driftNow[i]) * dc;
        auto dr = [this] (size_t i) { return 0.6f * driftNow[i] + 0.4f * tol[i]; };
        const float slop = pp.envSlop;

        const float keyFromC4 = (pitch - 60.0f) / 12.0f;
        const float e1 = env1.process (p.a1 * (1 + slop * (slopA - 1)), p.d1 * (1 + slop * (slopD - 1)), p.s1, p.r1 * (1 + slop * (slopR - 1)),
                                       std::exp2 (-keyFromC4 * p.key1 * 1.5f), sr)
                         * (1.0f - p.vel1 + p.vel1 * vel);
        const float e2 = env2.process (p.a2 * (1 + slop * (slopA - 1)), p.d2 * (1 + slop * (slopD - 1)), p.s2, p.r2 * (1 + slop * (slopR - 1)),
                                       std::exp2 (-keyFromC4 * p.key2 * 1.5f), sr)
                         * (1.0f - p.vel2 + p.vel2 * vel);
        const float e1m = p.env1Invert ? -e1 : e1;

        // Matriz de modulación
        std::array<float, (size_t) ModDst::Count> md {};
        for (auto& slot : p.matrix)
        {
            if (slot.src == ModSrc::None || slot.dst == ModDst::None || slot.amount == 0.0f) continue;
            float v = 0;
            switch (slot.src)
            {
                case ModSrc::Lfo: v = lfo; break;
                case ModSrc::Env1: v = e1; break;
                case ModSrc::Env2: v = e2; break;
                case ModSrc::Velocity: v = vel; break;
                case ModSrc::Key: v = (pitch - 60.0f) / 36.0f; break;
                case ModSrc::Wheel: v = pp.wheel; break;
                case ModSrc::Aftertouch: v = pp.aftertouch; break;
                case ModSrc::Bend: v = pp.bend; break;
                case ModSrc::Random: v = randomValue; break;
                case ModSrc::None: case ModSrc::Count: break;
            }
            md[(size_t) slot.dst] += v * slot.amount;
        }
        auto M = [&md] (ModDst d) { return md[(size_t) d]; };

        // Tono de los osciladores
        const float base = pitch + offs.detuneSt + pp.tuneSt;
        const float modSt = (p.vcoLfo * 12.0f + pp.wheelVco) * lfo + p.vcoEnv * 24.0f * e1m;
        const float tune1 = pp.bendVco1 + pp.driftVco * 0.2f * dr (0) + 24.0f * (M (ModDst::Pitch) + M (ModDst::Pitch1));   // deriva hasta ±20 cents
        const float tune2 = pp.bendVco2 + pp.driftVco * 0.2f * dr (1) + 24.0f * (M (ModDst::Pitch) + M (ModDst::Pitch2));
        const bool m1 = p.vcoModDest != ModDest::Vco2, m2 = p.vcoModDest != ModDest::Vco1;
        const float oct1 = 12.0f * (float) (1 - p.vco1Range);
        const float oct2 = 12.0f * (float) (1 - p.vco2Range);
        float f2 = p.vco2Low ? 0.3f * std::exp2 ((p.vco2Fine + 12.0f * (float) p.vco2Range) / 12.0f)
                             : noteToHz (base - oct2 + p.vco2Fine + tune2 + (m2 ? modSt : 0.0f));
        f2 = std::min (f2, sr * 0.45f);

        // Ancho de pulso
        const float pwmSrc = p.pwmSrc == PwmSrc::Lfo ? 0.5f + 0.5f * lfo : e1;
        const float width = std::clamp (0.5f - 0.45f * std::clamp (p.pulseWidth + p.pwmAmount * pwmSrc + M (ModDst::PulseWidth), 0.0f, 1.0f), 0.05f, 0.5f);

        // VCO-2 primero (es el modulador de la modulación cruzada)
        const float dt2 = f2 / sr;
        const float o2 = osc2 (p.vco2Wave, dt2, width);

        const float xmAmt = std::clamp (p.crossMod + M (ModDst::CrossMod), 0.0f, 1.0f);
        const float xm = xmAmt * xmAmt * 4.0f * o2;   // octavas
        float f1 = noteToHz (base - oct1 + tune1 + (m1 ? modSt : 0.0f)) * std::exp2 (xm);
        f1 = std::min (f1, sr * 0.45f);
        const float dt1 = f1 / sr;
        const bool wrapped = ph1 + dt1 >= 1.0f;
        const float o1 = osc1 (p.vco1Wave, dt1, width);
        if (p.sync && wrapped && ! p.vco2Low)
            ph2 = (ph1 / dt1) * dt2;   // VCO-2 esclavo de VCO-1

        // Un soplo de ruido de fondo, como en un circuito real: permite que el filtro autooscile.
        float x = 0.6f * (o1 * p.vco1Level + o2 * std::clamp (p.vco2Level + M (ModDst::Vco2Level), 0.0f, 1.0f)) + 1.0e-4f * noise.next();

        // Filtro paso alto (sin resonancia)
        const float hpfAmt = std::clamp (p.hpf + M (ModDst::Hpf), 0.0f, 1.0f);
        if (hpfAmt > 0.001f) x = hp.process (x, tptG (20.0f * std::exp2 (hpfAmt * 6.64f), sr));

        // Filtro paso bajo
        const float envSrc = p.vcfEnv2 ? e2 : e1m;
        const float cutOct = p.cutoff * 9.97f + p.vcfEnv * 8.0f * envSrc + (p.vcfLfo * 3.0f + pp.wheelVcf) * lfo
                           + p.keyFollow * keyFromC4 + pp.bendVcf + pp.driftVcf * 0.25f * dr (2) + 5.0f * M (ModDst::Cutoff);
        const float reso = std::clamp (p.resonance + M (ModDst::Resonance), 0.0f, 1.0f);
        const float g = tptG (20.0f * std::exp2 (cutOct), sr);
        float y;
        if (p.slope24)
        {
            const float k = 4.1f * reso;
            y = ladder.process (x * 0.9f, g, k) * (1.0f + 0.6f * k);
        }
        else
            y = lp12.lowpass (x, g, 2.0f * (1.0f - 0.985f * reso));

        // VCA
        const float amp = 0.5f * e2 * p.level * (1.0f + pp.driftVca * 0.12f * dr (3)) * (1.0f - 0.5f * p.vcaLfo * (1.0f - lfo))
                        * std::clamp (1.0f + M (ModDst::Level), 0.0f, 2.0f);
        y *= amp * offs.gain;
        const float pan = std::clamp (offs.pan + M (ModDst::Pan), -1.0f, 1.0f);
        l += y * std::sqrt (0.5f * (1.0f - pan));
        r += y * std::sqrt (0.5f * (1.0f + pan));
    }

private:
    float osc1 (Wave1 w, float dt, float width)
    {
        float v = 0, t = ph1;
        switch (w)
        {
            case Wave1::Saw: v = 2.0f * t - 1.0f - polyBlep (t, dt); break;
            case Wave1::Triangle:
                v = 1.0f - 4.0f * std::abs (t - 0.5f);
                v += 4.0f * dt * (polyBlamp (t, dt) - polyBlamp (std::fmod (t + 0.5f, 1.0f), dt));
                v = -v;
                break;
            case Wave1::Square: width = 0.5f; [[fallthrough]];
            case Wave1::Pulse: v = pulse (t, dt, width); break;
        }
        ph1 += dt; if (ph1 >= 1.0f) ph1 -= 1.0f;
        return v;
    }
    float osc2 (Wave2 w, float dt, float width)
    {
        float v = 0, t = ph2;
        switch (w)
        {
            case Wave2::Sine: v = std::sin (kTwoPi * t); break;
            case Wave2::Saw: v = 2.0f * t - 1.0f - polyBlep (t, dt); break;
            case Wave2::Pulse: v = pulse (t, dt, width); break;
            case Wave2::Noise: v = noise.next(); break;
        }
        ph2 += dt; if (ph2 >= 1.0f) ph2 -= 1.0f;
        return v;
    }
    static float pulse (float t, float dt, float w)
    {
        float v = t < w ? 1.0f : -1.0f;
        v += polyBlep (t, dt);
        v -= polyBlep (std::fmod (t + 1.0f - w, 1.0f), dt);
        return v;
    }

    float sr = 48000;
    int note = 60;
    float pitch = 60, target = 60, vel = 1;
    float slopA = 1, slopD = 1, slopR = 1;
    int driftCount = 0;
    std::array<float, 4> tol {}, driftNow {}, driftTarget {};
    Noise drift;
    bool gate = false;
    uint64_t age = 0;
    float randomValue = 0;
    float ph1 = 0, ph2 = 0;
    Adsr env1, env2;
    Svf lp12;
    Ladder ladder;
    OnePoleHp hp;
    Noise noise;
};
} // namespace s8
