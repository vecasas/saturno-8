// Renderiza unos sonidos de ejemplo a WAV para escuchar el motor sin DAW.
// Uso: render_demo <carpeta_salida>
#include "../src/dsp/Synth.h"
#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

static void writeWav (const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) { std::perror (path.c_str()); return; }
    const uint32_t n = (uint32_t) l.size();
    const uint32_t dataBytes = n * 2 * 2;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr);
    w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        int16_t a = (int16_t) std::lround (std::clamp (l[i], -1.0f, 1.0f) * 32767.0f);
        int16_t b = (int16_t) std::lround (std::clamp (r[i], -1.0f, 1.0f) * 32767.0f);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

struct Demo { const char* name; std::function<void (s8::Layer&)> setup; std::vector<std::vector<int>> chords; float beat; };

int main (int argc, char** argv)
{
    using namespace s8;
    const std::string out = argc > 1 ? argv[1] : ".";
    const int sr = 48000;

    const std::vector<Demo> demos {
        { "01_metales", [] (Layer& L) {
            L.p.vco1Wave = Wave1::Saw; L.p.vco2Wave = Wave2::Saw; L.p.vco2Level = 0.8f; L.p.vco2Fine = 0.08f;
            L.p.cutoff = 0.35f; L.p.resonance = 0.15f; L.p.vcfEnv = 0.45f; L.p.a1 = 0.06f; L.p.d1 = 0.5f; L.p.s1 = 0.45f;
            L.p.a2 = 0.03f; L.p.s2 = 0.9f; L.p.r2 = 0.35f; L.p.vcoLfo = 0.01f; L.p.lfoRate = 5.5f; L.p.lfoDelay = 0.6f; },
          { { 60, 64, 67 }, { 62, 65, 69 }, { 59, 62, 67 }, { 60, 64, 67, 72 } }, 1.0f },
        { "02_pad_pwm", [] (Layer& L) {
            L.p.vco1Wave = Wave1::Pulse; L.p.pwmAmount = 0.6f; L.p.lfoRate = 0.7f; L.p.vco2Wave = Wave2::Pulse; L.p.vco2Level = 0.6f;
            L.p.vco2Range = 0; L.p.vco2Fine = 0.1f; L.p.cutoff = 0.5f; L.p.slope24 = false; L.p.resonance = 0.2f;
            L.p.a2 = 0.8f; L.p.r2 = 1.5f; L.p.hpf = 0.25f; },
          { { 57, 60, 64, 67 }, { 53, 57, 60, 64 } }, 2.0f },
        { "03_bajo_solo", [] (Layer& L) {
            L.a.mode = Assign::Solo; L.p.vco1Range = 0; L.p.vco1Wave = Wave1::Square; L.p.vco2Range = 0; L.p.vco2Wave = Wave2::Saw;
            L.p.vco2Level = 0.7f; L.p.vco2Fine = -12; L.p.cutoff = 0.25f; L.p.resonance = 0.55f; L.p.vcfEnv = 0.5f;
            L.p.d1 = 0.18f; L.p.s1 = 0; L.p.d2 = 0.3f; L.p.s2 = 0.7f; L.p.r2 = 0.05f; L.p.vel1 = 0.5f; },
          { { 36 }, { 36 }, { 48 }, { 36 }, { 39 }, { 41 }, { 43 }, { 46 } }, 0.25f },
        { "04_lead_sync", [] (Layer& L) {
            L.a.mode = Assign::Unison; L.a.unisonVoices = 3; L.a.unisonDetune = 0.25f;
            L.p.sync = true; L.p.vco1Level = 0.2f; L.p.vco2Level = 1; L.p.vco2Wave = Wave2::Saw; L.p.vco2Fine = 5;
            L.p.vcoEnv = 0.4f; L.p.vcoModDest = ModDest::Vco2; L.p.d1 = 0.6f; L.p.s1 = 0.1f;
            L.p.cutoff = 0.65f; L.p.r2 = 0.2f; },
          { { 67 }, { 70 }, { 72 }, { 74 }, { 72 }, { 70 } }, 0.5f },
        { "05_campanas_xmod", [] (Layer& L) {
            L.p.vco1Wave = Wave1::Triangle; L.p.vco1Range = 2; L.p.crossMod = 0.55f; L.p.vco2Wave = Wave2::Sine; L.p.vco2Fine = 3.86f;
            L.p.cutoff = 0.8f; L.p.d2 = 1.4f; L.p.s2 = 0; L.p.r2 = 1.4f; L.p.key2 = 0.6f; },
          { { 72 }, { 79 }, { 76 }, { 84 }, { 72, 76, 79 } }, 0.6f },
        { "06_acorde_memoria", [] (Layer& L) {
            L.a.mode = Assign::Chord; L.p.vco1Wave = Wave1::Saw; L.p.vco2Level = 0.5f; L.p.vco2Fine = -0.1f;
            L.p.cutoff = 0.3f; L.p.resonance = 0.35f; L.p.vcfEnv = 0.55f; L.p.d1 = 0.35f; L.p.s1 = 0.2f; L.p.d2 = 0.6f; L.p.s2 = 0.4f; L.p.r2 = 0.3f;
            for (int n : { 60, 63, 67, 70 }) L.noteOn (n, 1);
            L.learnChord();
            L.reset(); },
          { { 48 }, { 48 }, { 51 }, { 53 }, { 46 }, { 46 }, { 53 }, { 55 } }, 0.375f },
    };

    double renderedSecs = 0, cpuSecs = 0;
    for (auto& d : demos)
    {
        Layer L; L.setSampleRate ((float) sr);
        d.setup (L);
        std::vector<float> lo, ro;
        auto render = [&] (float secs) {
            const auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < (int) (secs * sr); ++i) { float l = 0, r = 0; L.process (l, r); lo.push_back (l * 0.8f); ro.push_back (r * 0.8f); }
            cpuSecs += std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            renderedSecs += secs;
        };
        for (auto& c : d.chords)
        {
            for (int n : c) L.noteOn (n, 0.9f);
            render (d.beat * 0.85f);
            for (int n : c) L.noteOff (n);
            render (d.beat * 0.15f);
        }
        render (2.0f);
        writeWav (out + "/" + d.name + ".wav", lo, ro, sr);
        std::printf ("%s.wav\n", d.name);
    }
    // Capas: bajo solo en la inferior y metales en la superior, división en Do 4,
    // con deriva "de gira" y un toque de bender al final.
    {
        Synth S; S.setSampleRate ((float) sr);
        S.g.keyMode = KeyMode::Split; S.g.splitPoint = 60; S.g.driftOn = true; S.g.condition = Condition::Road;
        demos[2].setup (S.lower);
        demos[0].setup (S.upper);
        S.g.portaOn = true; S.g.portamento = 0.08f; S.g.portaTarget = Target::Lower;
        std::vector<float> lo, ro;
        auto render = [&] (float secs, float bendFrom = 0, float bendTo = 0) {
            const int n = (int) (secs * sr);
            for (int i = 0; i < n; ++i)
            {
                S.setPitchBend (bendFrom + (bendTo - bendFrom) * (float) i / (float) n);
                S.updatePlay();
                float l, r; S.process (l, r); lo.push_back (l); ro.push_back (r);
            }
        };
        const int bass[] { 36, 36, 43, 36, 39, 41, 43, 46 };
        const std::vector<std::vector<int>> chords { { 67, 72, 76 }, { 69, 72, 77 }, { 67, 71, 74 }, { 67, 72, 76 } };
        for (int bar = 0; bar < 4; ++bar)
        {
            for (int n : chords[(size_t) bar]) S.noteOn (n, 0.9f);
            for (int st = 0; st < 8; ++st)
            {
                S.noteOn (bass[st], 1.0f); render (0.2f); S.noteOff (bass[st]); render (0.05f);
            }
            for (int n : chords[(size_t) bar]) S.noteOff (n);
        }
        for (int n : { 72, 76, 79 }) S.noteOn (n, 0.9f);
        render (0.6f); render (0.6f, 0, -1); render (0.6f, -1, 0);
        for (int n : { 72, 76, 79 }) S.noteOff (n);
        render (2.0f);
        writeWav (out + "/07_capas_division.wav", lo, ro, sr);
        std::printf ("07_capas_division.wav\n");
    }

    std::printf ("Tiempo real x%.1f (una capa)\n", renderedSecs / cpuSecs);
    return 0;
}
