#pragma once
#include <BinaryData.h>
#include "PresetManager.h"
#include <vector>

inline std::vector<PresetManager::FactoryPreset> makeFactoryPresets()
{
    return {
        // Utility
        { "Init",           "Utility", juce::String::fromUTF8(BinaryData::Utility_Init_xml,           BinaryData::Utility_Init_xmlSize) },
        { "Passthrough",    "Utility", juce::String::fromUTF8(BinaryData::Utility_Passthrough_xml,    BinaryData::Utility_Passthrough_xmlSize) },
        { "Full Corpus",    "Utility", juce::String::fromUTF8(BinaryData::Utility_FullCorpus_xml,     BinaryData::Utility_FullCorpus_xmlSize) },
        // Drums
        { "Kick Follower",  "Drums",   juce::String::fromUTF8(BinaryData::Drums_KickFollower_xml,     BinaryData::Drums_KickFollower_xmlSize) },
        { "Snare Snatcher", "Drums",   juce::String::fromUTF8(BinaryData::Drums_SnareSnatcher_xml,    BinaryData::Drums_SnareSnatcher_xmlSize) },
        { "Hat Scatter",    "Drums",   juce::String::fromUTF8(BinaryData::Drums_HatScatter_xml,       BinaryData::Drums_HatScatter_xmlSize) },
        { "Drum Crush",     "Drums",   juce::String::fromUTF8(BinaryData::Drums_DrumCrush_xml,        BinaryData::Drums_DrumCrush_xmlSize) },
        { "Perc Ghost",     "Drums",   juce::String::fromUTF8(BinaryData::Drums_PercGhost_xml,        BinaryData::Drums_PercGhost_xmlSize) },
        { "Room Drums",     "Drums",   juce::String::fromUTF8(BinaryData::Drums_RoomDrums_xml,        BinaryData::Drums_RoomDrums_xmlSize) },
        { "Glue Layer",     "Drums",   juce::String::fromUTF8(BinaryData::Drums_GlueLayer_xml,        BinaryData::Drums_GlueLayer_xmlSize) },
        { "Beat Granule",   "Drums",   juce::String::fromUTF8(BinaryData::Drums_BeatGranule_xml,      BinaryData::Drums_BeatGranule_xmlSize) },
        // Texture
        { "Aether Pad",     "Texture", juce::String::fromUTF8(BinaryData::Texture_AetherPad_xml,     BinaryData::Texture_AetherPad_xmlSize) },
        { "Crystal Choir",  "Texture", juce::String::fromUTF8(BinaryData::Texture_CrystalChoir_xml,  BinaryData::Texture_CrystalChoir_xmlSize) },
        { "Silk Texture",   "Texture", juce::String::fromUTF8(BinaryData::Texture_SilkTexture_xml,   BinaryData::Texture_SilkTexture_xmlSize) },
        { "Frozen Air",     "Texture", juce::String::fromUTF8(BinaryData::Texture_FrozenAir_xml,     BinaryData::Texture_FrozenAir_xmlSize) },
        { "Warm Blanket",   "Texture", juce::String::fromUTF8(BinaryData::Texture_WarmBlanket_xml,   BinaryData::Texture_WarmBlanket_xmlSize) },
        { "Spectral Drift", "Texture", juce::String::fromUTF8(BinaryData::Texture_SpectralDrift_xml, BinaryData::Texture_SpectralDrift_xmlSize) },
        { "Shimmer Halo",   "Texture", juce::String::fromUTF8(BinaryData::Texture_ShimmerHalo_xml,   BinaryData::Texture_ShimmerHalo_xmlSize) },
        { "Deep Space",     "Texture", juce::String::fromUTF8(BinaryData::Texture_DeepSpace_xml,     BinaryData::Texture_DeepSpace_xmlSize) },
        { "Velvet Noise",   "Texture", juce::String::fromUTF8(BinaryData::Texture_VelvetNoise_xml,   BinaryData::Texture_VelvetNoise_xmlSize) },
        { "Nebula",         "Texture", juce::String::fromUTF8(BinaryData::Texture_Nebula_xml,        BinaryData::Texture_Nebula_xmlSize) },
        // Glitch
        { "Micro Stutter",  "Glitch",  juce::String::fromUTF8(BinaryData::Glitch_MicroStutter_xml,   BinaryData::Glitch_MicroStutter_xmlSize) },
        { "Shatter",        "Glitch",  juce::String::fromUTF8(BinaryData::Glitch_Shatter_xml,        BinaryData::Glitch_Shatter_xmlSize) },
        { "Data Rot",       "Glitch",  juce::String::fromUTF8(BinaryData::Glitch_DataRot_xml,        BinaryData::Glitch_DataRot_xmlSize) },
        { "Granular Melt",  "Glitch",  juce::String::fromUTF8(BinaryData::Glitch_GranularMelt_xml,   BinaryData::Glitch_GranularMelt_xmlSize) },
        { "Time Splice",    "Glitch",  juce::String::fromUTF8(BinaryData::Glitch_TimeSplice_xml,     BinaryData::Glitch_TimeSplice_xmlSize) },
        { "Bit Crumble",    "Glitch",  juce::String::fromUTF8(BinaryData::Glitch_BitCrumble_xml,     BinaryData::Glitch_BitCrumble_xmlSize) },
        { "Pulse Scatter",  "Glitch",  juce::String::fromUTF8(BinaryData::Glitch_PulseScatter_xml,   BinaryData::Glitch_PulseScatter_xmlSize) },
        { "Chaos Engine",   "Glitch",  juce::String::fromUTF8(BinaryData::Glitch_ChaosEngine_xml,    BinaryData::Glitch_ChaosEngine_xmlSize) },
        // Vocal
        { "Vowel Lock",     "Vocal",   juce::String::fromUTF8(BinaryData::Vocal_VowelLock_xml,       BinaryData::Vocal_VowelLock_xmlSize) },
        { "Choir Smear",    "Vocal",   juce::String::fromUTF8(BinaryData::Vocal_ChoirSmear_xml,      BinaryData::Vocal_ChoirSmear_xmlSize) },
        { "Formant Freeze", "Vocal",   juce::String::fromUTF8(BinaryData::Vocal_FormantFreeze_xml,   BinaryData::Vocal_FormantFreeze_xmlSize) },
        { "Breath Extract", "Vocal",   juce::String::fromUTF8(BinaryData::Vocal_BreathExtract_xml,   BinaryData::Vocal_BreathExtract_xmlSize) },
        { "Syllable Echo",  "Vocal",   juce::String::fromUTF8(BinaryData::Vocal_SyllableEcho_xml,    BinaryData::Vocal_SyllableEcho_xmlSize) },
        { "Haunt",          "Vocal",   juce::String::fromUTF8(BinaryData::Vocal_Haunt_xml,           BinaryData::Vocal_Haunt_xmlSize) },
        // LoFi
        { "Tape Warp",      "LoFi",    juce::String::fromUTF8(BinaryData::LoFi_TapeWarp_xml,         BinaryData::LoFi_TapeWarp_xmlSize) },
        { "Vinyl Dust",     "LoFi",    juce::String::fromUTF8(BinaryData::LoFi_VinylDust_xml,        BinaryData::LoFi_VinylDust_xmlSize) },
        { "Old Radio",      "LoFi",    juce::String::fromUTF8(BinaryData::LoFi_OldRadio_xml,         BinaryData::LoFi_OldRadio_xmlSize) },
        { "Lo Pad",         "LoFi",    juce::String::fromUTF8(BinaryData::LoFi_LoPad_xml,            BinaryData::LoFi_LoPad_xmlSize) },
        { "Washed Out",     "LoFi",    juce::String::fromUTF8(BinaryData::LoFi_WashedOut_xml,        BinaryData::LoFi_WashedOut_xmlSize) },
        { "Nostalgic",      "LoFi",    juce::String::fromUTF8(BinaryData::LoFi_Nostalgic_xml,        BinaryData::LoFi_Nostalgic_xmlSize) },
        // Tonal
        { "Pitch Trace",    "Tonal",   juce::String::fromUTF8(BinaryData::Tonal_PitchTrace_xml,      BinaryData::Tonal_PitchTrace_xmlSize) },
        { "Harmonic Cloud", "Tonal",   juce::String::fromUTF8(BinaryData::Tonal_HarmonicCloud_xml,   BinaryData::Tonal_HarmonicCloud_xmlSize) },
        { "Single Weight",  "Tonal",   juce::String::fromUTF8(BinaryData::Tonal_SingleWeight_xml,    BinaryData::Tonal_SingleWeight_xmlSize) },
        { "Bright Chord",   "Tonal",   juce::String::fromUTF8(BinaryData::Tonal_BrightChord_xml,     BinaryData::Tonal_BrightChord_xmlSize) },
        { "Bass Foundation","Tonal",   juce::String::fromUTF8(BinaryData::Tonal_BassFoundation_xml,  BinaryData::Tonal_BassFoundation_xmlSize) },
        // FX
        { "Riser Build",    "FX",      juce::String::fromUTF8(BinaryData::FX_RiserBuild_xml,         BinaryData::FX_RiserBuild_xmlSize) },
        { "Freeze Swell",   "FX",      juce::String::fromUTF8(BinaryData::FX_FreezeSwell_xml,        BinaryData::FX_FreezeSwell_xmlSize) },
        { "Chaos Riser",    "FX",      juce::String::fromUTF8(BinaryData::FX_ChaosRiser_xml,         BinaryData::FX_ChaosRiser_xmlSize) },
        { "Reverse Grain",  "FX",      juce::String::fromUTF8(BinaryData::FX_ReverseGrain_xml,       BinaryData::FX_ReverseGrain_xmlSize) },
        { "Noise Bloom",    "FX",      juce::String::fromUTF8(BinaryData::FX_NoiseBloom_xml,         BinaryData::FX_NoiseBloom_xmlSize) },
    };
}
