// Pruebas del arpegiador y del secuenciador sin JUCE.
#include "../src/dsp/Synth.h"
#include <cstdio>
#include <vector>

static int failures = 0;
#define CHECK(c) do { if (! (c)) { std::printf ("FALLO %s:%d  %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

using namespace s8;
static const float sr = 48000;

struct Event { int sample, note; };

// Renderiza n muestras y apunta cada nota que empieza a sonar en la capa superior.
static std::vector<Event> run (Synth& S, int n, int& clock)
{
    std::vector<Event> ev;
    int prev = S.upperPlayer.soundingNote();
    for (int i = 0; i < n; ++i, ++clock)
    {
        float l = 0, r = 0; S.process (l, r);
        const int now = S.upperPlayer.soundingNote();
        if (now >= 0 && now != prev) ev.push_back ({ clock, now });
        prev = now;
    }
    return ev;
}
static std::vector<int> notesOf (const std::vector<Event>& ev) { std::vector<int> v; for (auto& e : ev) v.push_back (e.note); return v; }

static void setup (Synth& S, PlayerMode mode)
{
    S.setSampleRate (sr);
    S.setTransport (120, false, 0);
    S.upperPlayer.p.mode = mode;
    S.upperPlayer.p.division = 3;   // semicorcheas: 0,125 s a 120 BPM = 6000 muestras
    S.upperPlayer.p.gate = 0.5f;
    S.updatePlayers();
}

int main()
{
    // Arpegio hacia arriba: orden y tiempo de cada paso.
    {
        Synth S; setup (S, PlayerMode::Arp); int clock = 0;
        for (int n : { 67, 60, 64 }) S.noteOn (n, 1);
        auto ev = run (S, 6 * 6000, clock);
        CHECK ((notesOf (ev) == std::vector<int> { 60, 64, 67, 60, 64, 67 }));
        CHECK (ev.size() == 6 && ev[0].sample == 0 && ev[1].sample == 6000 && ev[5].sample == 30000);
        // La nota se calla a mitad de paso (gate 50 %).
        CHECK (S.upper.activeVoices() >= 1);
    }

    // Abajo, arriba-abajo con dos octavas, y salto.
    {
        Synth S; setup (S, PlayerMode::Arp); int clock = 0;
        S.upperPlayer.p.dir = ArpDir::Down;
        for (int n : { 60, 64, 67 }) S.noteOn (n, 1);
        CHECK ((notesOf (run (S, 3 * 6000, clock)) == std::vector<int> { 67, 64, 60 }));

        Synth U; setup (U, PlayerMode::Arp); clock = 0;
        U.upperPlayer.p.dir = ArpDir::UpDown; U.upperPlayer.p.range = 2;
        for (int n : { 60, 64 }) U.noteOn (n, 1);
        CHECK ((notesOf (run (U, 8 * 6000, clock)) == std::vector<int> { 60, 64, 72, 76, 72, 64, 60, 64 }));

        Synth L; setup (L, PlayerMode::Arp); clock = 0;
        L.upperPlayer.p.pattern = ArpPattern::Leap;
        for (int n : { 60, 64, 67 }) L.noteOn (n, 1);
        CHECK ((notesOf (run (L, 4 * 6000, clock)) == std::vector<int> { 60, 64, 60, 67 }));
    }

    // Orden de pulsación.
    {
        Synth S; setup (S, PlayerMode::Arp); int clock = 0;
        S.upperPlayer.p.pattern = ArpPattern::Order;
        for (int n : { 67, 60, 64 }) S.noteOn (n, 1);
        CHECK ((notesOf (run (S, 3 * 6000, clock)) == std::vector<int> { 67, 60, 64 }));
    }

    // Soltar las teclas para el arpegio; con enganche sigue sonando.
    {
        Synth S; setup (S, PlayerMode::Arp); int clock = 0;
        S.noteOn (60, 1); run (S, 3000, clock); S.noteOff (60);
        CHECK (run (S, 12000, clock).empty());
        S.upperPlayer.p.hold = true; S.updatePlayers();
        S.noteOn (62, 1); run (S, 3000, clock); S.noteOff (62);
        CHECK (run (S, 12000, clock).size() == 2);
        // Una tecla nueva tras soltar todo empieza una frase nueva.
        S.noteOn (65, 1); auto ev = run (S, 12000, clock);
        CHECK (! ev.empty() && ev.back().note == 65);
        for (auto& e : ev) CHECK (e.note == 65);
    }

    // Swing al 75 %: el segundo paso llega a 3/4 del par.
    {
        Synth S; setup (S, PlayerMode::Arp); int clock = 0;
        S.upperPlayer.p.swing = 0.75f;
        for (int n : { 60, 64 }) S.noteOn (n, 1);
        auto ev = run (S, 12000, clock);
        CHECK (ev.size() == 2 && ev[1].sample == 9000);
    }

    // Secuenciador: notas relativas a la tecla más grave, silencio, ligadura y longitud.
    {
        Synth S; setup (S, PlayerMode::Seq); int clock = 0;
        StepData steps; S.upperPlayer.steps = &steps;
        S.upperPlayer.p.length = 4;
        steps.set (0, { 0 }); steps.set (1, { 7 }); steps.set (2, { 0, false }); steps.set (3, { 12 });
        S.noteOn (48, 1);
        CHECK ((notesOf (run (S, 8 * 6000, clock)) == std::vector<int> { 48, 55, 60, 48, 55, 60 }));
        // Ligadura: el paso 2 prolonga el 1 (sin nota nueva y sin silencio entre ellos).
        steps.set (2, { 0, true, true });
        S.noteOff (48); run (S, 6000, clock);
        S.noteOn (50, 1);
        auto ev = run (S, 4 * 6000, clock);
        CHECK ((notesOf (ev) == std::vector<int> { 50, 57, 62 }));
        CHECK (ev.size() == 3 && ev[2].sample - ev[1].sample == 12000);
    }

    // Ida y vuelta.
    {
        Synth S; setup (S, PlayerMode::Seq); int clock = 0;
        StepData steps; S.upperPlayer.steps = &steps;
        S.upperPlayer.p.length = 3; S.upperPlayer.p.play = SeqPlay::PingPong;
        for (int i = 0; i < 3; ++i) steps.set (i, { i * 2 });
        S.noteOn (60, 1);
        CHECK ((notesOf (run (S, 6 * 6000, clock)) == std::vector<int> { 60, 62, 64, 62, 60, 62 }));
    }

    // Con el DAW en marcha, los pasos caen en la rejilla de su posición.
    {
        Synth S; setup (S, PlayerMode::Arp); int clock = 0;
        S.setTransport (120, true, 10.1);   // a mitad de un paso
        for (int n : { 60, 64 }) S.noteOn (n, 1);
        auto ev = run (S, 7000, clock);
        // Suena enseguida y el siguiente paso cae en 10,25 negras = 0,15 negras después = 3600 muestras.
        CHECK (ev.size() == 2 && ev[0].sample == 0 && ev[1].sample == 3600);
    }

    // Cambiar de modo suelta todo.
    {
        Synth S; setup (S, PlayerMode::Arp); int clock = 0;
        S.noteOn (60, 1); run (S, 1000, clock);
        S.upperPlayer.p.mode = PlayerMode::Off; S.updatePlayers(); run (S, 96000, clock);
        CHECK (S.upper.activeVoices() == 0);
    }

    std::printf (failures ? "%d fallos\n" : "OK: pruebas del arpegiador y secuenciador pasan\n", failures);
    return failures ? 1 : 0;
}
