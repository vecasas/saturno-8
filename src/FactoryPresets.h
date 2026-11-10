#pragma once
// Presets de fábrica de Saturno 8 (programación propia).
// Cada preset parte de los valores por defecto y cambia solo lo que se indica:
// "id=valor" separados por espacios, en unidades reales (las listas, por índice).
// Prefijo u_ = capa superior, l_ = capa inferior; sin prefijo = global.
// Opcional: acorde ("chord=0,3,7") y secuencia de la capa ("useq=..." / "lseq=...": nota[:L][:A][:-] por paso).

namespace s8
{
struct FactoryPreset { const char* name; const char* values; };

inline const FactoryPreset* factoryPresets (int& count)
{
    static const FactoryPreset list[] {
        { "Inicial", "" },
        { "Metales del anillo",
          "u_vco1_wave=1 u_vco2_wave=1 u_vco2_lvl=0.8 u_vco2_fine=0.08 u_cutoff=0.38 u_reso=0.15 u_vcf_env=0.45 "
          "u_env1_a=0.06 u_env1_d=0.5 u_env1_s=0.45 u_env2_a=0.03 u_env2_s=0.9 u_env2_r=0.35 u_vco_lfo=0.01 "
          "u_lfo_rate=5.5 u_lfo_delay=0.6 u_env1_vel=0.4 drift_on=1 condition=1 rv_on=1 rv_mix=0.15" },
        { "Cuerdas de Titán",
          "u_vco1_wave=1 u_vco2_wave=1 u_vco2_lvl=0.9 u_vco2_fine=0.12 u_vco1_range=1 u_vco2_range=0 u_cutoff=0.55 "
          "u_hpf=0.25 u_env2_a=0.6 u_env2_r=1.4 u_env2_s=1 u_fx_chorus_on=1 u_fx_chorus_mode=1 u_fx_chorus_mix=0.7 "
          "drift_on=1 condition=2 rv_on=1 rv_size=0.75 rv_mix=0.3 u_slope=0" },
        { "Pad pulsante",
          "u_vco1_wave=2 u_pwm=0.6 u_lfo_rate=0.7 u_vco2_wave=2 u_vco2_lvl=0.6 u_vco2_range=0 u_vco2_fine=0.1 "
          "u_cutoff=0.5 u_slope=0 u_reso=0.2 u_env2_a=0.8 u_env2_r=1.5 u_hpf=0.25 u_fx_phaser_on=1 u_fx_phaser_rate=0.15 "
          "u_fx_phaser_mix=0.4 rv_on=1 rv_mix=0.25" },
        { "Bajo Encélado",
          "u_assign=0 u_vco1_range=0 u_vco1_wave=3 u_vco2_range=0 u_vco2_wave=1 u_vco2_lvl=0.7 u_vco2_fine=-12 "
          "u_cutoff=0.27 u_reso=0.55 u_vcf_env=0.5 u_env1_d=0.18 u_env1_s=0 u_env2_d=0.3 u_env2_s=0.7 u_env2_r=0.05 "
          "u_env1_vel=0.5 porta_on=1 porta=0.04" },
        { "Lead sincronizado",
          "u_assign=1 u_uni_voices=3 u_uni_detune=0.25 u_sync=1 u_vco1_lvl=0.2 u_vco2_lvl=1 u_vco2_wave=1 "
          "u_vco2_fine=5 u_vco_env=0.4 u_vco_dest=1 u_env1_d=0.6 u_env1_s=0.1 u_cutoff=0.68 u_env2_r=0.2 "
          "porta_on=1 porta=0.08 wheel_vco=0.4 u_fx_delay_on=1 u_fx_delay_div=4 u_fx_delay_mix=0.2 u_fx_delay_pp=1" },
        { "Campanas de hielo",
          "u_vco1_wave=0 u_vco1_range=2 u_xmod=0.55 u_vco2_wave=0 u_vco2_fine=3.86 u_cutoff=0.82 u_env2_d=1.4 "
          "u_env2_s=0 u_env2_r=1.4 u_env2_key=0.6 rv_on=1 rv_size=0.8 rv_mix=0.35" },
        { "Arpegio orbital",
          "u_arp_mode=1 u_arp_dir=2 u_arp_range=2 u_arp_div=3 u_arp_gate=0.4 u_vco1_wave=2 u_pw=0.4 u_cutoff=0.45 "
          "u_reso=0.45 u_vcf_env=0.45 u_env1_d=0.2 u_env1_s=0.05 u_env2_d=0.25 u_env2_s=0.3 u_env2_r=0.15 "
          "u_fx_delay_on=1 u_fx_delay_div=4 u_fx_delay_fb=0.45 u_fx_delay_mix=0.3 u_fx_delay_pp=1" },
        { "Secuencia de Febe",
          "u_arp_mode=2 u_seq_len=8 u_arp_div=3 u_arp_gate=0.55 u_assign=0 u_vco1_range=0 u_vco1_wave=1 u_vco2_wave=2 "
          "u_vco2_lvl=0.5 u_vco2_fine=-0.07 u_cutoff=0.3 u_reso=0.6 u_vcf_env=0.5 u_env1_d=0.15 u_env1_s=0 "
          "u_env2_d=0.2 u_env2_s=0.4 u_env2_r=0.08 u_env1_vel=0.6 "
          "useq=0:A,0,12,0,7,0:L,10,12:A" },
        { "Doble: piano y pad",
          "key_mode=1 balance=-0.1 "
          "u_vco1_wave=2 u_pw=0.3 u_vco2_wave=0 u_vco2_lvl=0.6 u_vco2_range=2 u_cutoff=0.5 u_vcf_env=0.35 u_vcf_key=0.8 "
          "u_env1_d=0.8 u_env1_s=0 u_env2_d=1.8 u_env2_s=0 u_env2_r=0.5 u_env2_vel=0.8 "
          "l_vco1_wave=1 l_vco2_wave=1 l_vco2_lvl=0.8 l_vco2_fine=0.1 l_cutoff=0.42 l_slope=0 l_env2_a=1.2 l_env2_r=2 "
          "l_level=0.6 l_fx_chorus_on=1 l_fx_chorus_mix=0.6 rv_on=1 rv_mix=0.25" },
        { "División: bajo y metales",
          "key_mode=2 split=55 "
          "l_assign=0 l_vco1_range=0 l_vco1_wave=1 l_vco2_range=0 l_vco2_wave=2 l_vco2_lvl=0.6 l_cutoff=0.3 l_reso=0.5 "
          "l_vcf_env=0.45 l_env1_d=0.2 l_env1_s=0 l_env2_s=0.8 l_env2_r=0.08 "
          "u_vco1_wave=1 u_vco2_wave=1 u_vco2_lvl=0.8 u_vco2_fine=0.08 u_cutoff=0.38 u_vcf_env=0.45 u_env1_a=0.06 "
          "u_env1_d=0.5 u_env1_s=0.45 u_env2_a=0.03 u_env2_r=0.35 drift_on=1" },
        { "Acorde menor 9",
          "u_assign=4 chord=0,3,7,10,14 u_vco1_wave=1 u_vco2_lvl=0.5 u_vco2_fine=-0.1 u_cutoff=0.3 u_reso=0.35 "
          "u_vcf_env=0.55 u_env1_d=0.35 u_env1_s=0.2 u_env2_d=0.6 u_env2_s=0.4 u_env2_r=0.3 "
          "u_fx_delay_on=1 u_fx_delay_div=4 u_fx_delay_mix=0.25 rv_on=1 rv_mix=0.2" },
        { "Barrido con ruido",
          "u_vco1_lvl=0 u_vco2_wave=3 u_vco2_lvl=1 u_cutoff=0.2 u_reso=0.85 u_vcf_env=0.6 u_env1_a=2.5 u_env1_d=3 "
          "u_env1_s=0.2 u_env2_a=1 u_env2_r=2.5 u_vcf_lfo=0.15 u_lfo_rate=0.3 rv_on=1 rv_size=0.9 rv_mix=0.4" },
        { "Viejo y desafinado",
          "u_vco1_wave=1 u_vco2_wave=1 u_vco2_lvl=0.7 u_vco2_fine=0.15 u_cutoff=0.45 u_vcf_env=0.3 u_env2_a=0.02 "
          "u_env2_r=0.5 drift_on=1 condition=4 drift_vco=0.7 drift_vcf=0.6 env_slop=0.6 u_fx_drive_on=1 "
          "u_fx_drive=0.25 u_fx_drive_mix=0.6 eq_on=1 eq_high=-5" },
        // --- Bajos ---
        { "Bajo redondo",
          "u_assign=0 u_vco1_range=0 u_vco1_wave=3 u_vco2_range=0 u_vco2_wave=0 u_vco2_lvl=0.6 u_cutoff=0.32 u_reso=0.2 "
          "u_vcf_env=0.35 u_env1_d=0.25 u_env1_s=0.1 u_env2_d=0.4 u_env2_s=0.8 u_env2_r=0.06 u_env1_vel=0.4" },
        { "Bajo ácido",
          "u_assign=0 u_vco1_range=0 u_vco1_wave=1 u_cutoff=0.22 u_reso=0.78 u_vcf_env=0.6 u_env1_d=0.22 u_env1_s=0 "
          "u_env2_r=0.05 u_env1_vel=0.5 porta_on=1 porta=0.06 u_fx_drive_on=1 u_fx_drive=0.35 u_fx_drive_mix=0.7" },
        { "Bajo sub",
          "u_assign=0 u_vco1_range=0 u_vco1_wave=0 u_vco2_range=0 u_vco2_wave=0 u_vco2_lvl=0.8 u_cutoff=0.4 "
          "u_env2_d=0.5 u_env2_s=0.9 u_env2_r=0.1" },
        { "Bajo de pulso",
          "u_assign=0 u_vco1_range=0 u_vco1_wave=2 u_pw=0.3 u_pwm=0.25 u_lfo_rate=0.4 u_cutoff=0.35 u_reso=0.3 "
          "u_vcf_env=0.4 u_env1_d=0.3 u_env1_s=0.15 u_env2_r=0.08" },
        { "Bajo unísono",
          "u_assign=1 u_uni_voices=4 u_uni_detune=0.2 u_vco1_range=0 u_vco1_wave=1 u_vco2_range=0 u_vco2_wave=1 "
          "u_vco2_lvl=0.7 u_cutoff=0.3 u_reso=0.35 u_vcf_env=0.45 u_env1_d=0.3 u_env1_s=0.1 u_env2_r=0.1 u_level=0.6" },
        // --- Solistas ---
        { "Lead de Rea",
          "u_assign=0 u_vco1_wave=1 u_vco2_wave=1 u_vco2_lvl=0.8 u_vco2_fine=0.1 u_cutoff=0.6 u_reso=0.3 u_vcf_env=0.25 "
          "u_env1_d=0.4 u_env1_s=0.5 u_env2_r=0.2 u_vco_lfo=0.01 u_lfo_rate=5 u_lfo_delay=0.5 porta_on=1 porta=0.1 "
          "u_mod1_src=7 u_mod1_dst=8 u_mod1_amt=0.4 u_fx_delay_on=1 u_fx_delay_div=4 u_fx_delay_mix=0.2 rv_on=1 rv_mix=0.15" },
        { "Lead cuadrado",
          "u_assign=0 u_vco1_wave=3 u_vco2_wave=2 u_vco2_range=2 u_vco2_lvl=0.4 u_cutoff=0.55 u_vcf_env=0.2 "
          "u_env1_s=0.6 u_env2_r=0.15 porta_on=1 porta=0.05 u_fx_delay_on=1 u_fx_delay_div=3 u_fx_delay_mix=0.25" },
        { "Lead silbante",
          "u_assign=0 u_vco1_wave=0 u_vco1_range=2 u_cutoff=0.5 u_reso=0.5 u_vco_lfo=0.02 u_lfo_rate=5 u_lfo_delay=0.4 "
          "u_env2_a=0.05 u_env2_r=0.3 porta_on=1 porta=0.12 rv_on=1 rv_mix=0.25" },
        { "Lead cruzado",
          "u_assign=0 u_xmod=0.35 u_vco_env=0.3 u_vco_dest=0 u_vco1_wave=1 u_vco2_wave=1 u_vco2_lvl=0.6 u_cutoff=0.6 "
          "u_reso=0.25 u_vcf_env=0.3 u_env1_d=0.5 u_env1_s=0.2 u_env2_r=0.2 u_fx_drive_on=1 u_fx_drive=0.3 u_fx_drive_mix=0.5" },
        { "Lead gritón",
          "u_assign=1 u_uni_voices=6 u_uni_detune=0.4 u_vco1_wave=1 u_vco2_wave=1 u_vco2_lvl=1 u_cutoff=0.7 u_reso=0.4 "
          "u_level=0.55 u_fx_drive_on=1 u_fx_drive=0.5 u_fx_drive_mix=0.6 u_fx_delay_on=1 u_fx_delay_div=6 "
          "u_fx_delay_mix=0.2 u_fx_delay_pp=1 rv_on=1 rv_mix=0.15" },
        // --- Pads ---
        { "Pad Hiperión",
          "u_vco1_wave=1 u_vco2_wave=1 u_vco2_lvl=0.9 u_vco2_fine=0.07 u_cutoff=0.45 u_vcf_lfo=0.05 u_lfo_rate=0.2 "
          "u_env2_a=1.5 u_env2_r=2.5 u_fx_chorus_on=1 u_fx_chorus_mode=2 u_fx_chorus_mix=0.6 rv_on=1 rv_size=0.85 rv_mix=0.35" },
        { "Pad de cristal",
          "u_vco1_wave=2 u_pw=0.2 u_vco2_wave=0 u_vco2_range=2 u_vco2_lvl=0.5 u_cutoff=0.65 u_hpf=0.35 u_env2_a=0.9 "
          "u_env2_r=2 u_fx_chorus_on=1 u_fx_chorus_mix=0.5 rv_on=1 rv_size=0.8 rv_mix=0.35" },
        { "Pad oscuro",
          "u_vco1_range=0 u_vco1_wave=1 u_vco2_wave=1 u_vco2_lvl=0.8 u_vco2_fine=-0.06 u_cutoff=0.25 u_reso=0.3 "
          "u_vcf_env=0.3 u_env1_a=2 u_env1_d=3 u_env1_s=0.4 u_env2_a=1.2 u_env2_r=2.5 rv_on=1 rv_mix=0.3" },
        { "Pad que respira",
          "u_vco1_wave=1 u_vco2_wave=2 u_vco2_lvl=0.6 u_pwm=0.3 u_cutoff=0.4 u_slope=0 u_vca_lfo=0.4 u_lfo_wave=0 "
          "u_lfo_rate=0.25 u_env2_a=0.8 u_env2_r=2 u_mod1_src=9 u_mod1_dst=11 u_mod1_amt=0.5 rv_on=1 rv_mix=0.3" },
        { "Coro espacial",
          "u_vco1_wave=2 u_pw=0.5 u_pwm=0.4 u_lfo_rate=0.5 u_vco2_wave=1 u_vco2_lvl=0.5 u_vco2_fine=0.05 u_cutoff=0.4 "
          "u_reso=0.45 u_slope=0 u_env2_a=0.7 u_env2_r=1.8 u_level=0.5 u_fx_chorus_on=1 u_fx_chorus_mode=2 u_fx_chorus_mix=0.8 "
          "rv_on=1 rv_size=0.9 rv_mix=0.4" },
        { "Amanecer lento",
          "u_vco1_wave=1 u_vco2_wave=1 u_vco2_lvl=0.8 u_vco2_fine=0.09 u_cutoff=0.15 u_reso=0.35 u_vcf_env=0.6 "
          "u_env1_a=4 u_env1_d=5 u_env1_s=0.7 u_env2_a=1.5 u_env2_r=3 u_fx_phaser_on=1 u_fx_phaser_rate=0.1 "
          "u_fx_phaser_mix=0.35 rv_on=1 rv_size=0.85 rv_mix=0.35" },
        // --- Cuerdas y metales ---
        { "Cuerdas de cámara",
          "u_vco1_wave=1 u_vco2_wave=1 u_vco2_lvl=0.85 u_vco2_fine=0.1 u_cutoff=0.5 u_hpf=0.3 u_env2_a=0.35 u_env2_r=0.9 "
          "u_vco_lfo=0.008 u_lfo_rate=5.5 u_lfo_delay=0.8 u_fx_chorus_on=1 u_fx_chorus_mix=0.5 rv_on=1 rv_mix=0.3" },
        { "Cuerdas de seda",
          "u_vco1_wave=1 u_vco2_wave=1 u_vco2_lvl=0.9 u_vco2_fine=0.06 u_vco2_range=2 u_cutoff=0.6 u_slope=0 u_hpf=0.2 "
          "u_env2_a=0.8 u_env2_r=1.6 u_fx_chorus_on=1 u_fx_chorus_mode=2 u_fx_chorus_mix=0.7 rv_on=1 rv_size=0.8 rv_mix=0.35" },
        { "Metales brillantes",
          "u_vco1_wave=1 u_vco2_wave=1 u_vco2_lvl=0.8 u_vco2_fine=0.06 u_cutoff=0.48 u_reso=0.1 u_vcf_env=0.55 "
          "u_env1_a=0.04 u_env1_d=0.35 u_env1_s=0.55 u_env2_a=0.02 u_env2_r=0.3 u_env1_vel=0.5 "
          "u_fx_drive_on=1 u_fx_drive=0.15 u_fx_drive_mix=0.4 rv_on=1 rv_mix=0.15" },
        { "Metales suaves",
          "u_vco1_wave=1 u_vco2_wave=1 u_vco2_lvl=0.7 u_vco2_fine=0.09 u_cutoff=0.32 u_slope=0 u_vcf_env=0.4 "
          "u_env1_a=0.15 u_env1_d=0.8 u_env1_s=0.5 u_env2_a=0.1 u_env2_r=0.4 rv_on=1 rv_mix=0.2" },
        // --- Teclas y pulsados ---
        { "Piano eléctrico",
          "u_vco1_wave=0 u_vco2_wave=0 u_vco2_range=2 u_vco2_lvl=0.4 u_xmod=0.15 u_cutoff=0.6 u_vcf_key=0.8 "
          "u_env2_d=1.5 u_env2_s=0 u_env2_r=0.4 u_env2_vel=0.8 u_fx_chorus_on=1 u_fx_chorus_mix=0.4 rv_on=1 rv_mix=0.2" },
        { "Teclado pulsado",
          "u_vco1_wave=2 u_pw=0.15 u_cutoff=0.5 u_reso=0.3 u_vcf_env=0.45 u_env1_d=0.2 u_env1_s=0 u_env2_d=0.5 "
          "u_env2_s=0 u_env2_r=0.1 u_env1_vel=0.7 u_env2_vel=0.7 u_fx_phaser_on=1 u_fx_phaser_mix=0.3" },
        { "Órgano de Jápeto",
          "u_vco1_wave=3 u_vco2_wave=2 u_pw=0.5 u_vco2_range=2 u_vco2_lvl=0.6 u_cutoff=0.7 u_env2_a=0.005 "
          "u_env2_r=0.05 u_env2_vel=0 u_fx_chorus_on=1 u_fx_chorus_mode=1 u_fx_chorus_mix=0.5 "
          "u_fx_drive_on=1 u_fx_drive=0.2 u_fx_drive_mix=0.5" },
        { "Marimba",
          "u_vco1_wave=0 u_vco2_wave=0 u_vco2_range=3 u_vco2_lvl=0.3 u_cutoff=0.55 u_vcf_env=0.3 u_env1_d=0.15 "
          "u_env1_s=0 u_env2_d=0.35 u_env2_s=0 u_env2_r=0.3 u_env2_vel=0.8 rv_on=1 rv_mix=0.2" },
        { "Pulsado brillante",
          "u_vco1_wave=1 u_vco2_wave=2 u_vco2_lvl=0.5 u_vco2_fine=0.08 u_cutoff=0.3 u_reso=0.3 u_vcf_env=0.6 "
          "u_env1_d=0.25 u_env1_s=0 u_env2_d=0.4 u_env2_s=0 u_env2_r=0.3 u_fx_delay_on=1 u_fx_delay_div=3 "
          "u_fx_delay_mix=0.3 u_fx_delay_pp=1 rv_on=1 rv_mix=0.2" },
        // --- Campanas y efectos ---
        { "Campana de Titania",
          "u_vco1_wave=0 u_vco2_wave=0 u_xmod=0.7 u_vco2_fine=7.02 u_cutoff=0.8 u_env2_d=2.5 u_env2_s=0 u_env2_r=2.5 "
          "u_env2_vel=0.7 rv_on=1 rv_size=0.85 rv_mix=0.4" },
        { "Gotas",
          "u_vco1_wave=0 u_vco1_range=3 u_vco_env=0.35 u_vco_dest=0 u_env1_d=0.08 u_env1_s=0 u_cutoff=0.8 "
          "u_env2_d=0.2 u_env2_s=0 u_env2_r=0.2 u_fx_delay_on=1 u_fx_delay_div=3 u_fx_delay_fb=0.5 u_fx_delay_mix=0.35 "
          "u_fx_delay_pp=1 rv_on=1 rv_mix=0.3" },
        { "Viento solar",
          "u_vco1_lvl=0 u_vco2_wave=3 u_vco2_lvl=1 u_cutoff=0.45 u_reso=0.6 u_slope=0 u_vcf_lfo=0.25 u_lfo_wave=0 "
          "u_lfo_rate=0.15 u_env2_a=0.6 u_env2_r=3 rv_on=1 rv_size=0.95 rv_mix=0.5" },
        { "Sirena",
          "u_assign=0 u_vco1_wave=1 u_vco_lfo=0.4 u_lfo_wave=1 u_lfo_rate=0.5 u_cutoff=0.6 u_reso=0.2 u_env2_r=0.5 "
          "rv_on=1 rv_mix=0.25" },
        { "Láser",
          "u_vco1_wave=1 u_vco_env=0.8 u_vco_dest=2 u_env1_d=0.25 u_env1_s=0 u_cutoff=0.7 u_reso=0.4 u_env2_d=0.3 "
          "u_env2_s=0 u_env2_r=0.2 u_fx_delay_on=1 u_fx_delay_div=4 u_fx_delay_mix=0.3 u_fx_delay_pp=1" },
        { "Filtro aleatorio",
          "u_vco1_wave=1 u_vco2_wave=2 u_vco2_lvl=0.5 u_cutoff=0.4 u_reso=0.7 u_vcf_lfo=0.4 u_lfo_wave=4 u_lfo_rate=8 "
          "u_env2_r=0.4 u_fx_delay_on=1 u_fx_delay_div=4 u_fx_delay_mix=0.25 u_fx_delay_pp=1" },
        // --- Arpegios y secuencias ---
        { "Arpegio de cristal",
          "u_arp_mode=1 u_arp_dir=0 u_arp_range=3 u_arp_div=3 u_arp_gate=0.3 u_vco1_wave=0 u_vco1_range=2 u_vco2_wave=0 "
          "u_vco2_range=3 u_vco2_lvl=0.3 u_cutoff=0.7 u_env2_d=0.3 u_env2_s=0 u_env2_r=0.3 "
          "u_fx_delay_on=1 u_fx_delay_div=5 u_fx_delay_mix=0.3 u_fx_delay_pp=1 rv_on=1 rv_mix=0.3" },
        { "Arpegio azaroso",
          "u_arp_mode=1 u_arp_pattern=3 u_arp_chance=0.8 u_arp_div=3 u_arp_range=2 u_arp_gate=0.5 u_vco1_wave=2 u_pw=0.35 "
          "u_cutoff=0.4 u_reso=0.5 u_vcf_env=0.4 u_env1_d=0.15 u_env1_s=0 u_env2_d=0.2 u_env2_s=0.3 u_env2_r=0.1 "
          "u_fx_delay_on=1 u_fx_delay_div=4 u_fx_delay_mix=0.25" },
        { "Pulso de Encélado",
          "u_arp_mode=1 u_arp_dir=2 u_arp_div=3 u_arp_gate=0.2 u_arp_swing=0.6 u_arp_range=2 u_vco1_wave=1 u_cutoff=0.35 "
          "u_reso=0.4 u_vcf_env=0.5 u_env1_d=0.12 u_env1_s=0 u_env2_d=0.15 u_env2_s=0 u_env2_r=0.08 "
          "u_fx_phaser_on=1 u_fx_phaser_rate=0.2 u_fx_phaser_mix=0.35 rv_on=1 rv_mix=0.15" },
        { "Secuencia de Mimas",
          "u_arp_mode=2 u_seq_len=16 u_arp_div=3 u_arp_gate=0.5 u_assign=0 u_vco1_range=0 u_vco1_wave=1 u_cutoff=0.22 "
          "u_reso=0.75 u_vcf_env=0.55 u_env1_d=0.18 u_env1_s=0 u_env2_r=0.05 u_env1_vel=0.5 "
          "u_fx_drive_on=1 u_fx_drive=0.3 u_fx_drive_mix=0.6 u_fx_delay_on=1 u_fx_delay_div=4 u_fx_delay_mix=0.2 "
          "useq=0,0:-,12:A,0,3,0:L,5,7:A,0,12,0:-,10,0,7:A,3,0" },
        // --- Capas y acordes ---
        { "Doble: cuerdas y campana",
          "key_mode=1 "
          "u_vco1_wave=0 u_vco2_wave=0 u_xmod=0.5 u_vco2_fine=3.86 u_cutoff=0.8 u_env2_d=1.6 u_env2_s=0 u_env2_r=1.6 "
          "u_level=0.5 l_vco1_wave=1 l_vco2_wave=1 l_vco2_lvl=0.85 l_vco2_fine=0.1 l_cutoff=0.5 l_env2_a=0.5 l_env2_r=1.2 "
          "l_fx_chorus_on=1 l_fx_chorus_mix=0.6 rv_on=1 rv_size=0.8 rv_mix=0.3" },
        { "División: bajo y lead",
          "key_mode=2 split=52 "
          "l_assign=0 l_vco1_range=0 l_vco1_wave=3 l_vco2_range=0 l_vco2_wave=1 l_vco2_lvl=0.5 l_cutoff=0.3 l_reso=0.4 "
          "l_vcf_env=0.4 l_env1_d=0.25 l_env1_s=0 l_env2_r=0.06 "
          "u_assign=0 u_vco1_wave=1 u_vco2_wave=1 u_vco2_lvl=0.8 u_vco2_fine=0.1 u_cutoff=0.6 u_reso=0.3 u_env1_s=0.5 "
          "u_env2_r=0.2 porta_on=1 porta=0.08 porta_target=0 u_fx_delay_on=1 u_fx_delay_div=4 u_fx_delay_mix=0.2" },
        { "Fundido: pad y piano",
          "key_mode=3 split=60 "
          "l_vco1_wave=1 l_vco2_wave=1 l_vco2_lvl=0.8 l_vco2_fine=0.08 l_cutoff=0.4 l_env2_a=0.9 l_env2_r=2 "
          "l_fx_chorus_on=1 l_fx_chorus_mix=0.6 "
          "u_vco1_wave=0 u_vco2_wave=0 u_vco2_range=2 u_vco2_lvl=0.4 u_xmod=0.15 u_cutoff=0.6 u_env2_d=1.5 u_env2_s=0 "
          "u_env2_r=0.4 rv_on=1 rv_mix=0.25" },
        { "Acorde de quintas",
          "u_assign=4 chord=0,7,12 u_level=0.45 u_vco1_wave=1 u_vco2_wave=1 u_vco2_lvl=0.7 u_vco2_fine=0.08 u_cutoff=0.35 u_reso=0.3 "
          "u_vcf_env=0.5 u_env1_d=0.3 u_env1_s=0.2 u_env2_d=0.5 u_env2_s=0.5 u_env2_r=0.3 "
          "u_fx_delay_on=1 u_fx_delay_div=4 u_fx_delay_mix=0.2 rv_on=1 rv_mix=0.2" },
    };
    count = (int) (sizeof (list) / sizeof (list[0]));
    return list;
}
} // namespace s8
