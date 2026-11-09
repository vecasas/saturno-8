// Pruebas de efectos y matriz de modulación sin JUCE.
#include "../src/dsp/Synth.h"
#include <cstdio>
#include <vector>

static int failures = 0;
#define CHECK(c) do { if (! (c)) { std::printf ("FALLO %s:%d  %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

using namespace s8;
static const float sr = 48000;

static float freqOf (const std::vector<float>& x, size_t from)
{
    int first = -1, last = -1, count = 0;
    for (size_t i = from + 1; i < x.size(); ++i)
        if (x[i - 1] < 0 && x[i] >= 0) { if (first < 0) first = (int) i; last = (int) i; ++count; }
    return count > 1 ? (float) (count - 1) * sr / (float) (last - first) : 0.0f;
}

int main()
{
    // Todos los efectos al máximo: 10 s de notas y 40 s de silencio. Nada de NaN y la cola
    // acaba apagándose (ninguna realimentación se sostiene sola).
    {
        Synth S; S.setSampleRate (sr); S.g.limiter = false; S.g.keyMode = KeyMode::Dual;
        for (LayerFx* f : { &S.upperFx, &S.lowerFx })
        {
            auto& p = f->p;
            p.driveOn = p.chorusOn = p.phaserOn = p.delayOn = true;
            p.drive = 1; p.chorusMix = 1; p.chorusMode = 2; p.phaserFeedback = 1; p.phaserMix = 1;
            p.delayFeedback = 1; p.delayMix = 1; p.delayDamp = 0; p.pingPong = f == &S.lowerFx;
        }
        S.globalFx.p.eqOn = S.globalFx.p.reverbOn = true; S.globalFx.p.eqLow = 12; S.globalFx.p.eqHigh = 12;
        S.globalFx.p.reverbSize = 1; S.globalFx.p.reverbDamp = 0; S.globalFx.p.reverbMix = 1;
        S.setTransport (140, false, 0);
        float peak = 0, last = 0; bool finite = true;
        for (int i = 0; i < (int) sr * 50; ++i)
        {
            if (i < sr * 10 && i % 24000 == 0) S.noteOn (48 + (i / 24000) % 24, 1);
            if (i < sr * 10 && i % 24000 == 12000) S.noteOff (48 + (i / 24000) % 24);
            S.updatePlay();
            float l = 0, r = 0; S.process (l, r);
            finite = finite && std::isfinite (l) && std::isfinite (r);
            peak = std::max ({ peak, std::abs (l), std::abs (r) });
            if (i > sr * 49) last = std::max ({ last, std::abs (l), std::abs (r) });
        }
        CHECK (finite);
        CHECK (last < peak * 0.01f);
    }

    // El delay repite el golpe al tiempo de la figura: 1/4 a 120 BPM = 0,5 s.
    {
        LayerFx fx; fx.prepare (sr);
        fx.p.delayOn = true; fx.p.delayDiv = 6; fx.p.delayFeedback = 0; fx.p.delayMix = 1; fx.p.delayDamp = 0;
        int peakAt = -1; float peak = 0;
        for (int i = 0; i < (int) sr; ++i)
        {
            float l = i == 0 ? 1.0f : 0.0f, r = l;
            fx.process (l, r, 120.0);
            if (i > 100 && std::abs (l) > peak) { peak = std::abs (l); peakAt = i; }
        }
        CHECK (std::abs (peakAt - 24000) < 4);
    }

    // La reverb deja cola y se apaga.
    {
        GlobalFx fx; fx.prepare (sr); fx.p.reverbOn = true; fx.p.reverbMix = 1;
        float tail = 0, late = 0;
        for (int i = 0; i < (int) sr * 8; ++i)
        {
            float l = i < 48 ? 1.0f : 0.0f, r = l;
            fx.process (l, r);
            if (i > sr * 0.3f && i < sr * 0.6f) tail = std::max (tail, std::abs (l));
            if (i > sr * 7) late = std::max (late, std::abs (l));
        }
        CHECK (tail > 0.001f);
        CHECK (late < tail * 0.01f);
        if (late >= tail * 0.01f) std::printf ("cola %g, final %g\n", tail, late);
    }

    // EQ a 0 dB no cambia la señal.
    {
        GlobalFx fx; fx.prepare (sr); fx.p.eqOn = true;
        Noise nz; float diff = 0;
        for (int i = 0; i < 2000; ++i) { float x = nz.next(), l = x, r = x; fx.process (l, r); diff = std::max (diff, std::abs (l - x)); }
        CHECK (diff < 1e-5f);
    }

    // Matriz: velocidad -> tono de VCO-1 con +0,5 = +12 st a velocidad 1.
    {
        Layer L; L.setSampleRate (sr); L.p.cutoff = 1; L.p.keyFollow = 0; L.p.slope24 = false;
        L.p.matrix[0] = { ModSrc::Velocity, ModDst::Pitch, 0.5f };
        L.noteOn (57, 1.0f);
        std::vector<float> x;
        for (int i = 0; i < 24000; ++i) { float l = 0, r = 0; L.process (l, r); x.push_back (l + r); }
        CHECK (std::abs (freqOf (x, 2000) - 440.0f) < 2.0f);
    }

    // Matriz: rueda -> corte cierra el filtro con cantidad negativa.
    {
        auto energy = [] (float wheel) {
            Synth S; S.setSampleRate (sr); S.g.limiter = false;
            S.upper.p.cutoff = 0.8f; S.upper.p.matrix[1] = { ModSrc::Wheel, ModDst::Cutoff, -1.0f };
            S.setModWheel (wheel); S.updatePlay();
            S.noteOn (72, 1);
            double e = 0;
            for (int i = 0; i < 9600; ++i) { float l = 0, r = 0; S.process (l, r); if (i > 4800) e += l * l; }
            return e;
        };
        CHECK (energy (1.0f) < energy (0.0f) * 0.1);
    }

    std::printf (failures ? "%d fallos\n" : "OK: pruebas de efectos y matriz pasan\n", failures);
    return failures ? 1 : 0;
}
