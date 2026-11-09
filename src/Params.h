#pragma once
// Tabla de parámetros de una capa. La usan el procesador (crear y leer) y la
// interfaz (dibujar cada sección). Los ID llevan el prefijo de la capa ("u_").
#include <JuceHeader.h>
#include "dsp/Synth.h"

namespace s8
{
enum class PType { Float, Choice, Bool, Int };

struct ParamDef
{
    const char* id;
    const char* name;
    const char* section;
    PType type;
    float min, max, def, skew;
    const char* choices;   // separadas por '|'
};

inline const std::vector<ParamDef>& layerParamDefs()
{
    static const std::vector<ParamDef> defs {
        // VCO-1
        { "vco1_range", "Registro", "VCO-1", PType::Choice, 0, 3, 1, 1, "16'|8'|4'|2'" },
        { "vco1_wave",  "Forma",    "VCO-1", PType::Choice, 0, 3, 1, 1, "Triángulo|Sierra|Pulso|Cuadrada" },
        { "xmod",       "Cruzada",  "VCO-1", PType::Float, 0, 1, 0, 1, nullptr },
        // VCO-2
        { "vco2_range", "Registro", "VCO-2", PType::Choice, 0, 3, 1, 1, "16'|8'|4'|2'" },
        { "vco2_fine",  "Afinar",   "VCO-2", PType::Float, -12, 12, 0, 1, nullptr },
        { "vco2_wave",  "Forma",    "VCO-2", PType::Choice, 0, 3, 1, 1, "Senoide|Sierra|Pulso|Ruido" },
        { "vco2_low",   "Baja frec.", "VCO-2", PType::Bool, 0, 1, 0, 1, nullptr },
        { "sync",       "Sync",     "VCO-2", PType::Bool, 0, 1, 0, 1, nullptr },
        // Modulador de los VCO
        { "vco_lfo",    "LFO",      "Mod. VCO", PType::Float, 0, 1, 0, 0.5f, nullptr },
        { "vco_env",    "ENV-1",    "Mod. VCO", PType::Float, 0, 1, 0, 0.5f, nullptr },
        { "vco_dest",   "Destino",  "Mod. VCO", PType::Choice, 0, 2, 2, 1, "VCO-1|VCO-2|Ambos" },
        { "pw",         "Ancho",    "Mod. VCO", PType::Float, 0, 1, 0, 1, nullptr },
        { "pwm",        "PWM",      "Mod. VCO", PType::Float, 0, 1, 0, 1, nullptr },
        { "pwm_src",    "Fuente PWM", "Mod. VCO", PType::Choice, 0, 1, 0, 1, "LFO|ENV-1" },
        // Mezclador
        { "vco1_lvl",   "VCO-1",    "Mezcla", PType::Float, 0, 1, 1, 1, nullptr },
        { "vco2_lvl",   "VCO-2",    "Mezcla", PType::Float, 0, 1, 0, 1, nullptr },
        { "hpf",        "Paso alto", "Mezcla", PType::Float, 0, 1, 0, 1, nullptr },
        // VCF
        { "cutoff",     "Corte",    "VCF", PType::Float, 0, 1, 0.7f, 1, nullptr },
        { "reso",       "Resonancia", "VCF", PType::Float, 0, 1, 0, 1, nullptr },
        { "slope",      "Pendiente", "VCF", PType::Choice, 0, 1, 1, 1, "12 dB|24 dB" },
        { "vcf_env",    "Env",      "VCF", PType::Float, 0, 1, 0, 1, nullptr },
        { "vcf_env_src", "Fuente env", "VCF", PType::Choice, 0, 1, 0, 1, "ENV-1|ENV-2" },
        { "vcf_lfo",    "LFO",      "VCF", PType::Float, 0, 1, 0, 0.5f, nullptr },
        { "vcf_key",    "Teclado",  "VCF", PType::Float, 0, 1, 0.5f, 1, nullptr },
        // VCA
        { "level",      "Nivel",    "VCA", PType::Float, 0, 1, 0.8f, 1, nullptr },
        { "vca_lfo",    "LFO",      "VCA", PType::Float, 0, 1, 0, 1, nullptr },
        // ENV-1
        { "env1_a", "A", "ENV-1", PType::Float, 0.001f, 10, 0.005f, 0.25f, nullptr },
        { "env1_d", "D", "ENV-1", PType::Float, 0.001f, 10, 0.4f, 0.25f, nullptr },
        { "env1_s", "S", "ENV-1", PType::Float, 0, 1, 0, 1, nullptr },
        { "env1_r", "R", "ENV-1", PType::Float, 0.001f, 10, 0.3f, 0.25f, nullptr },
        { "env1_key", "Teclado", "ENV-1", PType::Float, 0, 1, 0, 1, nullptr },
        { "env1_vel", "Velocidad", "ENV-1", PType::Float, 0, 1, 0, 1, nullptr },
        { "env1_inv", "Invertir", "ENV-1", PType::Bool, 0, 1, 0, 1, nullptr },
        // ENV-2
        { "env2_a", "A", "ENV-2", PType::Float, 0.001f, 10, 0.005f, 0.25f, nullptr },
        { "env2_d", "D", "ENV-2", PType::Float, 0.001f, 10, 0.3f, 0.25f, nullptr },
        { "env2_s", "S", "ENV-2", PType::Float, 0, 1, 1, 1, nullptr },
        { "env2_r", "R", "ENV-2", PType::Float, 0.001f, 10, 0.3f, 0.25f, nullptr },
        { "env2_key", "Teclado", "ENV-2", PType::Float, 0, 1, 0, 1, nullptr },
        { "env2_vel", "Velocidad", "ENV-2", PType::Float, 0, 1, 0.5f, 1, nullptr },
        // LFO
        { "lfo_rate",  "Velocidad", "LFO", PType::Float, 0.05f, 40, 4, 0.3f, nullptr },
        { "lfo_delay", "Retardo",   "LFO", PType::Float, 0, 5, 0, 0.5f, nullptr },
        { "lfo_wave",  "Forma",     "LFO", PType::Choice, 0, 4, 1, 1, "Senoide|Triángulo|Sierra|Cuadrada|Aleatoria" },
        // Asignación de voces
        { "assign",    "Modo",      "Asignación", PType::Choice, 0, 4, 3, 1, "Solo|Unísono|Poli 1|Poli 2|Acorde" },
        { "priority",  "Prioridad", "Asignación", PType::Choice, 0, 2, 2, 1, "Grave|Aguda|Última" },
        { "trigger",   "Disparo",   "Asignación", PType::Choice, 0, 1, 0, 1, "Legato|Múltiple" },
        { "uni_voices", "Voces uní.", "Asignación", PType::Int, 1, 16, 4, 1, nullptr },
        { "uni_detune", "Desafinar", "Asignación", PType::Float, 0, 1, 0.3f, 1, nullptr },
        { "poly_voices", "Voces poli", "Asignación", PType::Int, 1, 16, 16, 1, nullptr },
    };
    return defs;
}

// Parámetros globales (sin prefijo).
inline const std::vector<ParamDef>& globalParamDefs()
{
    static const std::vector<ParamDef> defs {
        // Teclado y capas
        { "key_mode",   "Modo",       "Teclado", PType::Choice, 0, 3, 0, 1, "Entera|Doble|División|División con fundido" },
        { "split",      "División",   "Teclado", PType::Int, 24, 96, 60, 1, nullptr },
        { "balance",    "Balance",    "Teclado", PType::Float, -1, 1, 0, 1, nullptr },
        { "sus_upper",  "Pedal sup.", "Teclado", PType::Bool, 0, 1, 1, 1, nullptr },
        { "sus_lower",  "Pedal inf.", "Teclado", PType::Bool, 0, 1, 1, 1, nullptr },
        // Bender y rueda de modulación
        { "bend_range", "Bender VCO", "Bender", PType::Float, 0, 12, 2, 1, nullptr },
        { "bend_dest",  "Destino",    "Bender", PType::Choice, 0, 2, 2, 1, "VCO-1|VCO-2|Ambos" },
        { "bend_vcf",   "Bender VCF", "Bender", PType::Float, 0, 1, 0, 1, nullptr },
        { "bend_target", "Capas",     "Bender", PType::Choice, 0, 2, 1, 1, "Superior|Ambas|Inferior" },
        { "wheel_vco",  "Rueda VCO",  "Bender", PType::Float, 0, 1, 0.3f, 1, nullptr },
        { "wheel_vcf",  "Rueda VCF",  "Bender", PType::Float, 0, 1, 0, 1, nullptr },
        // Portamento
        { "porta_on",   "Activo",     "Portamento", PType::Bool, 0, 1, 0, 1, nullptr },
        { "porta",      "Tiempo",     "Portamento", PType::Float, 0, 5, 0.15f, 0.35f, nullptr },
        { "porta_target", "Capas",    "Portamento", PType::Choice, 0, 2, 1, 1, "Superior|Ambas|Inferior" },
        // Deriva analógica
        { "drift_on",   "Activa",     "Deriva", PType::Bool, 0, 1, 0, 1, nullptr },
        { "condition",  "Estado",     "Deriva", PType::Choice, 0, 4, 1, 1, "Recién salido|De estudio|De gira|Veterano|Al taller" },
        { "drift_vco",  "VCO",        "Deriva", PType::Float, 0, 1, 0.3f, 1, nullptr },
        { "drift_vcf",  "VCF",        "Deriva", PType::Float, 0, 1, 0.3f, 1, nullptr },
        { "drift_vca",  "VCA",        "Deriva", PType::Float, 0, 1, 0.3f, 1, nullptr },
        { "env_slop",   "Envolventes", "Deriva", PType::Float, 0, 1, 0.3f, 1, nullptr },
        // Máster
        { "master",     "Volumen",    "Máster", PType::Float, 0, 1, 0.7f, 1, nullptr },
        { "tune",       "Afinación",  "Máster", PType::Float, -1, 1, 0, 1, nullptr },
        { "limiter",    "Limitador",  "Máster", PType::Bool, 0, 1, 1, 1, nullptr },
    };
    return defs;
}

inline juce::StringArray choicesOf (const ParamDef& d)
{
    return juce::StringArray::fromTokens (juce::String (juce::CharPointer_UTF8 (d.choices)), "|", "");
}
} // namespace s8
