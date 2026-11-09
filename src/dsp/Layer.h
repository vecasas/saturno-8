#pragma once
// Capa de Saturno 8: 16 voces, LFO compartido y modos de asignación
// (solo, unísono, poli 1, poli 2 y memoria de acorde). Sin dependencias de JUCE.

#include "Voice.h"
#include <array>
#include <vector>

namespace s8
{
constexpr int kMaxVoices = 16;

enum class Assign { Solo, Unison, Poly1, Poly2, Chord };
enum class Priority { Low, High, Last };

struct AssignParams
{
    Assign mode = Assign::Poly2;
    Priority priority = Priority::Last;
    bool legato = true;          // solo/unísono: false = redispara con cada tecla (Multi)
    int unisonVoices = 4;        // 1..16
    float unisonDetune = 0.3f;   // 0..1 (hasta ±50 cents en los extremos)
    int polyVoices = 16;         // 1..16 voces disponibles en modo poli
};

class Layer
{
public:
    LayerParams p;
    AssignParams a;

    Layer()
    {
        for (int i = 0; i < kMaxVoices; ++i) voices[(size_t) i].setSeed (0x9e3779b9u * (uint32_t) (i + 1));
        voiceKey.fill (-1);
        stack.reserve (128);
        chord.reserve (kMaxVoices);
        chord = { 0, 4, 7 };   // acorde inicial: tríada mayor
    }

    void setSampleRate (float newSr)
    {
        sr = newSr;
        for (auto& v : voices) v.setSampleRate (sr);
    }

    void noteOn (int note, float vel)
    {
        ++clock;
        if (! anyHeld()) lfo.restartFade();
        pressed[(size_t) note] = true;
        sustained[(size_t) note] = false;
        removeFromStack (note);
        stack.push_back (note);

        switch (a.mode)
        {
            case Assign::Solo:
            case Assign::Unison: updateMono (vel, true); break;
            case Assign::Chord:
                for (int iv : chord)
                    if (note + iv < 128) polyStart (note, note + iv, vel, false);
                break;
            case Assign::Poly1: polyStart (note, note, vel, false); break;
            case Assign::Poly2: polyStart (note, note, vel, true); break;
        }
    }

    void noteOff (int note)
    {
        pressed[(size_t) note] = false;
        if (sustainPedal) { sustained[(size_t) note] = true; return; }
        releaseKey (note);
    }

    void setSustain (bool on)
    {
        sustainPedal = on;
        if (! on)
            for (int n = 0; n < 128; ++n)
                if (sustained[(size_t) n]) { sustained[(size_t) n] = false; if (! pressed[(size_t) n]) releaseKey (n); }
    }

    void allNotesOff()
    {
        ++clock;
        for (auto& v : voices) v.release (clock);
        stack.clear();
        pressed.fill (false);
        sustained.fill (false);
        voiceKey.fill (-1);
    }

    void reset()
    {
        allNotesOff();
        for (auto& v : voices) v.kill();
    }

    // Memoria de acorde: guarda los intervalos de las teclas pulsadas, desde la más grave.
    bool learnChord()
    {
        int lowest = -1;
        for (int n = 0; n < 128; ++n)
            if (pressed[(size_t) n])
            {
                if (lowest < 0) { lowest = n; chord.clear(); }
                if ((int) chord.size() < kMaxVoices) chord.push_back (n - lowest);
            }
        return lowest >= 0;
    }
    const std::vector<int>& chordIntervals() const { return chord; }
    void setChordIntervals (const std::vector<int>& c) { if (! c.empty() && c.size() <= (size_t) kMaxVoices) chord.assign (c.begin(), c.end()); }

    void process (float& l, float& r)
    {
        const float lfoOut = lfo.process (p, sr);
        for (auto& v : voices) v.process (p, lfoOut, l, r);
    }

    int activeVoices() const
    {
        int n = 0;
        for (auto& v : voices) n += v.active() ? 1 : 0;
        return n;
    }
    int soundingNotes (std::vector<int>& out) const
    {
        out.clear();
        for (auto& v : voices) if (v.active() && v.held()) out.push_back (v.currentNote());
        return (int) out.size();
    }
    const Voice& voice (int i) const { return voices[(size_t) i]; }

private:
    bool anyHeld() const
    {
        for (int n = 0; n < 128; ++n) if (pressed[(size_t) n] || sustained[(size_t) n]) return true;
        return false;
    }

    void removeFromStack (int note)
    {
        stack.erase (std::remove (stack.begin(), stack.end(), note), stack.end());
    }

    void releaseKey (int note)
    {
        ++clock;
        removeFromStack (note);
        if (a.mode == Assign::Solo || a.mode == Assign::Unison) { updateMono (1.0f, false); return; }
        for (int i = 0; i < kMaxVoices; ++i)
            if (voiceKey[(size_t) i] == note) { voices[(size_t) i].release (clock); voiceKey[(size_t) i] = -1; }
    }

    // Elige la voz para una nota nueva en modo poli.
    int pickVoice (int sounding, bool reuseSame)
    {
        const int n = std::clamp (a.polyVoices, 1, kMaxVoices);
        if (reuseSame)                                   // poli 2: la misma nota vuelve a su voz
            for (int i = 0; i < n; ++i)
                if (voices[(size_t) i].active() && ! voices[(size_t) i].held() && voices[(size_t) i].currentNote() == sounding)
                    return i;
        int best = -1;
        if (reuseSame)                                   // poli 2: primero una voz en silencio
            for (int i = 0; i < n && best < 0; ++i)
                if (! voices[(size_t) i].active()) best = i;
        if (best < 0)                                    // la voz soltada hace más tiempo
            for (int i = 0; i < n; ++i)
                if (! voices[(size_t) i].held() && (best < 0 || voices[(size_t) i].stamp() < voices[(size_t) best].stamp()))
                    best = i;
        if (best < 0)                                    // todas pulsadas: roba la más antigua
            for (int i = 0; i < n; ++i)
                if (best < 0 || voices[(size_t) i].stamp() < voices[(size_t) best].stamp()) best = i;
        return best;
    }

    void polyStart (int key, int sounding, float vel, bool reuseSame)
    {
        const int i = pickVoice (sounding, reuseSame);
        auto& v = voices[(size_t) i];
        v.offs = {};
        v.start (sounding, vel, true, clock);
        voiceKey[(size_t) i] = key;
    }

    int monoNote() const
    {
        int pick = -1;
        for (int n : stack)
        {
            if (pick < 0) { pick = n; continue; }
            if (a.priority == Priority::Low  && n < pick) pick = n;
            if (a.priority == Priority::High && n > pick) pick = n;
            if (a.priority == Priority::Last) pick = n;
        }
        return pick;
    }

    void updateMono (float vel, bool fromNoteOn)
    {
        int count = a.mode == Assign::Unison ? std::clamp (a.unisonVoices, 1, kMaxVoices) : 1;
        int note = monoNote();
        // Con el pedal, la última nota mantenida sigue sonando.
        if (note < 0)
            for (int n = 0; n < 128; ++n) if (sustained[(size_t) n]) note = n;

        if (note < 0)
        {
            for (int i = 0; i < kMaxVoices; ++i) voices[(size_t) i].release (clock);
            monoPlaying = -1;
            return;
        }
        const bool wasSounding = monoPlaying >= 0;
        const bool retrigger = fromNoteOn ? (! a.legato || ! wasSounding) : false;
        if (note == monoPlaying && ! (fromNoteOn && ! a.legato)) return;

        for (int i = 0; i < kMaxVoices; ++i)
        {
            auto& v = voices[(size_t) i];
            if (i >= count) { v.release (clock); continue; }
            const float spread = count > 1 ? (float) i / (float) (count - 1) * 2.0f - 1.0f : 0.0f;
            v.offs.detuneSt = spread * a.unisonDetune * 0.5f;
            v.offs.pan = spread * 0.7f;
            if (wasSounding && v.held() && ! retrigger) v.glideTo (note);
            else v.start (note, fromNoteOn ? vel : monoVel, retrigger || ! v.held(), clock);
        }
        if (fromNoteOn) monoVel = vel;
        monoPlaying = note;
    }

    float sr = 48000;
    uint64_t clock = 0;
    std::array<Voice, kMaxVoices> voices;
    std::array<int, kMaxVoices> voiceKey;
    std::array<bool, 128> pressed {}, sustained {};
    std::vector<int> stack, chord;
    bool sustainPedal = false;
    int monoPlaying = -1;
    float monoVel = 1.0f;
    Lfo lfo;
};
} // namespace s8
