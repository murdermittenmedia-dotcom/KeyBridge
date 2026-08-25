cmake_minimum_required(VERSION 3.22)
project(KeyBridge VERSION 0.1.0)

add_subdirectory(JUCE)
juce_add_plugin(KeyBridge
    COMPANY_NAME "KeyBridge"
    PLUGIN_MANUFACTURER_CODE KBrg
    PLUGIN_CODE KBr1
    FORMATS VST3 Standalone
    PRODUCT_NAME "KeyBridge"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

juce_generate_juce_header(KeyBridge)

target_sources(KeyBridge PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(KeyBridge PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(KeyBridge PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
