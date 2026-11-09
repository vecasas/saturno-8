#pragma once
// Saturno 8 completo: dos capas (superior e inferior), modos de teclado,
// bender, rueda de modulación, portamento, deriva analógica, arpegiador o
// secuenciador por capa y sección máster.
// Sin dependencias de JUCE.

#include "Layer.h"
#include "../seq/Player.h"

namespace s8
{
enum class KeyMode { Whole, Dual, Split, SplitFade };
enum class Target { Upper, Both, Lower };
enum class Condition { New, Studio, Road, Veteran, Service };

struct GlobalParams
{
    KeyMode keyMode = KeyMode::Whole;
    int splitPoint = 60;              // primera nota de la capa superior
    float balance = 0;                // -1 = solo inferior, +1 = solo superior

    float bendRange = 2;              // semitonos
    ModDest bendDest = ModDest::Both;
    float bendVcf = 0;                // 0..1 (hasta ±2 octavas)
    Target bendTarget = Target::Both;
    float wheelVco = 0.3f;            // 0..1 (hasta 2 semitonos de LFO)
    float wheelVcf = 0;               // 0..1 (hasta 2 octavas de LFO)

    float portamento = 0;             // s
    Target portaTarget = Target::Both;
    bool portaOn = false;

    bool driftOn = false;
    float driftVco = 0.3f, driftVcf = 0.3f, driftVca = 0.3f, envSlop = 0.3f;
    Condition condition = Condition::Studio;

    bool sustainUpper = true, sustainLower = true;
    float tune = 0;                   // semitonos (±1)
    float volume = 0.7f;
    bool limiter = true;
};

inline float conditionScale (Condition c)
{
    switch (c)
    {
        case Condition::New: return 0.25f;
        case Condition::Studio: return 0.6f;
        case Condition::Road: return 1.0f;
        case Condition::Veteran: return 1.7f;
        case Condition::Service: return 3.0f;
    }
    return 1.0f;
}

class Synth
{
public:
    Synth() = default;
    Synth (const Synth&) = delete;              // los reproductores apuntan a sus capas
    Synth& operator= (const Synth&) = delete;

    Layer upper, lower;
    Player upperPlayer { upper }, lowerPlayer { lower };
    GlobalParams g;

    // Tempo y posición: llamar al principio de cada bloque.
    // hostPlaying = el DAW está en marcha y ppq es su posición en negras.
    void setTransport (double bpm, bool hostPlaying, double ppq)
    {
        tempo = std::clamp (bpm, 20.0, 400.0);
        if (hostPlaying) beat = ppq;
        hostSync = hostPlaying;
    }
    double currentBeat() const { return beat; }

    // Aplica los parámetros del arpegiador; si cambia el modo, suelta lo que sonaba.
    void updatePlayers()
    {
        for (int i = 0; i < 2; ++i)
        {
            auto& pl = i == 0 ? upperPlayer : lowerPlayer;
            auto& L = i == 0 ? upper : lower;
            if (pl.p.mode != lastMode[i]) { pl.stop(); L.allNotesOff(); lastMode[i] = pl.p.mode; }
            pl.updateHold();
        }
    }

    void setSampleRate (float newSr)
    {
        sr = newSr;
        upper.setSampleRate (sr);
        lower.setSampleRate (sr);
        limiter.prepare (sr);
    }
    void reset() { upperPlayer.stop(); lowerPlayer.stop(); upper.reset(); lower.reset(); bend = 0; wheel = 0; }

    void noteOn (int note, float vel)
    {
        float gu = 1, gl = 1;
        const bool playU = routes (note, true, gu), playL = routes (note, false, gl);
        if (playU) { if (upperPlayer.active()) upperPlayer.keyDown (note, vel, gu); else upper.noteOn (note, vel, gu); }
        if (playL) { if (lowerPlayer.active()) lowerPlayer.keyDown (note, vel, gl); else lower.noteOn (note, vel, gl); }
        playedUpper[(size_t) note] = playU;
        playedLower[(size_t) note] = playL;
    }
    void noteOff (int note)
    {
        // Suelta en la capa que la tocó, aunque el modo haya cambiado entretanto.
        if (playedUpper[(size_t) note]) { if (upperPlayer.active()) upperPlayer.keyUp (note); else upper.noteOff (note); }
        if (playedLower[(size_t) note]) { if (lowerPlayer.active()) lowerPlayer.keyUp (note); else lower.noteOff (note); }
        playedUpper[(size_t) note] = playedLower[(size_t) note] = false;
    }
    void setSustain (bool on)
    {
        if (g.sustainUpper || ! on) upper.setSustain (on);
        if (g.sustainLower || ! on) lower.setSustain (on);
    }
    void allNotesOff()
    {
        upperPlayer.stop(); lowerPlayer.stop();
        upper.allNotesOff(); lower.allNotesOff();
        playedUpper.fill (false); playedLower.fill (false);
    }

    void setPitchBend (float v) { bend = std::clamp (v, -1.0f, 1.0f); }   // -1..1
    void setModWheel (float v) { wheel = std::clamp (v, 0.0f, 1.0f); }   // 0..1

    // Actualiza los controles de interpretación de cada capa. Llamar una vez por bloque.
    void updatePlay()
    {
        const float ds = g.driftOn ? conditionScale (g.condition) : 0.0f;
        for (int i = 0; i < 2; ++i)
        {
            const bool isUpper = i == 0;
            auto& pp = (isUpper ? upper : lower).play;
            const bool bendHere = g.bendTarget == Target::Both || (g.bendTarget == Target::Upper) == isUpper;
            const float st = bendHere ? bend * g.bendRange : 0.0f;
            pp.bendVco1 = g.bendDest != ModDest::Vco2 ? st : 0.0f;
            pp.bendVco2 = g.bendDest != ModDest::Vco1 ? st : 0.0f;
            pp.bendVcf = bendHere ? bend * g.bendVcf * 2.0f : 0.0f;
            pp.wheelVco = bendHere ? wheel * g.wheelVco * 2.0f : 0.0f;
            pp.wheelVcf = bendHere ? wheel * g.wheelVcf * 2.0f : 0.0f;
            const bool portaHere = g.portaOn && (g.portaTarget == Target::Both || (g.portaTarget == Target::Upper) == isUpper);
            pp.glide = portaHere ? g.portamento : 0.0f;
            pp.driftVco = g.driftVco * ds; pp.driftVcf = g.driftVcf * ds;
            pp.driftVca = g.driftVca * ds; pp.envSlop = std::min (1.0f, g.envSlop * ds);
            pp.tuneSt = g.tune;
        }
    }

    void process (float& l, float& r)
    {
        upperPlayer.tick (beat, hostSync);
        lowerPlayer.tick (beat, hostSync);
        beat += tempo / (60.0 * (double) sr);

        float ul = 0, ur = 0, ll = 0, lr = 0;
        upper.process (ul, ur);
        lower.process (ll, lr);
        const float gu = std::min (1.0f, 1.0f + g.balance), gl = std::min (1.0f, 1.0f - g.balance);
        l = (ul * gu + ll * gl) * g.volume;
        r = (ur * gu + lr * gl) * g.volume;
        reduction = g.limiter ? limiter.process (l, r) : 1.0f;
    }

    float lastReduction() const { return reduction; }

private:
    // ¿Toca la capa esta nota, y con qué nivel?
    bool routes (int note, bool isUpper, float& gain) const
    {
        gain = 1;
        switch (g.keyMode)
        {
            case KeyMode::Whole: return isUpper;
            case KeyMode::Dual: return true;
            case KeyMode::Split: return isUpper == (note >= g.splitPoint);
            case KeyMode::SplitFade:
            {
                // Fundido de una octava centrado en el punto de división.
                const float x = std::clamp (((float) note - (float) g.splitPoint + 6.0f) / 12.0f, 0.0f, 1.0f);
                gain = isUpper ? std::sqrt (x) : std::sqrt (1.0f - x);
                return gain > 0.01f;
            }
        }
        return isUpper;
    }

    float bend = 0, wheel = 0, reduction = 1, sr = 48000;
    double tempo = 120, beat = 0;
    bool hostSync = false;
    PlayerMode lastMode[2] { PlayerMode::Off, PlayerMode::Off };
    Limiter limiter;
    std::array<bool, 128> playedUpper {}, playedLower {};
};
} // namespace s8
