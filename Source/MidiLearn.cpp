#include "MidiLearn.h"
#include <algorithm>

// ─── message-thread helpers ──────────────────────────────────────────────────

void MidiLearn::removeBindingsForParam(juce::RangedAudioParameter* param)
{
    for (auto& [cc, p] : bindings_)
        if (p == param) {
            ccToParam_[static_cast<std::size_t>(cc)].store(nullptr, std::memory_order_relaxed);
            cc = -1; // mark for erasure
        }
    bindings_.erase(
        std::remove_if(bindings_.begin(), bindings_.end(),
                       [](const auto& b) { return b.first < 0; }),
        bindings_.end());
}

void MidiLearn::removeBindingsForCC(int cc)
{
    bindings_.erase(
        std::remove_if(bindings_.begin(), bindings_.end(),
                       [cc](const auto& b) { return b.first == cc; }),
        bindings_.end());
}

// ─── public API ──────────────────────────────────────────────────────────────

void MidiLearn::registerParam(juce::RangedAudioParameter* /*param*/)
{
    // No-op in the current design; kept for API symmetry and possible future use.
}

void MidiLearn::handleCC(int cc, int value)
{
    if (cc < 0 || cc > 127)
        return;

    // Learning mode: capture the first CC that arrives.
    auto* lp = learningParam_.load(std::memory_order_relaxed);
    if (lp != nullptr) {
        int expected = -1;
        capturedCC_.compare_exchange_strong(expected, cc,
                                            std::memory_order_release,
                                            std::memory_order_relaxed);
        return; // do not dispatch during learn
    }

    // Normal dispatch.
    auto* param = ccToParam_[static_cast<std::size_t>(cc)].load(std::memory_order_relaxed);
    if (param != nullptr)
        param->setValueNotifyingHost(static_cast<float>(value) / 127.f);
}

bool MidiLearn::processCapture()
{
    const int cc = capturedCC_.load(std::memory_order_acquire);
    if (cc < 0)
        return false;

    auto* param = learningParam_.load(std::memory_order_relaxed);
    if (param == nullptr) {
        capturedCC_.store(-1, std::memory_order_relaxed);
        return false;
    }

    // 1. Remove any existing binding for this param (old CC → null).
    removeBindingsForParam(param);

    // 2. Remove any existing binding for this CC (old param evicted).
    auto* old = ccToParam_[static_cast<std::size_t>(cc)].exchange(nullptr, std::memory_order_relaxed);
    if (old != nullptr)
        removeBindingsForCC(cc);

    // 3. Commit the new binding.
    ccToParam_[static_cast<std::size_t>(cc)].store(param, std::memory_order_release);
    bindings_.emplace_back(cc, param);

    // 4. Clear learning state.
    learningParam_.store(nullptr, std::memory_order_relaxed);
    capturedCC_.store(-1, std::memory_order_relaxed);

    return true;
}

void MidiLearn::startLearning(juce::RangedAudioParameter* param)
{
    capturedCC_.store(-1, std::memory_order_relaxed);
    learningParam_.store(param, std::memory_order_relaxed);
}

void MidiLearn::stopLearning()
{
    learningParam_.store(nullptr, std::memory_order_relaxed);
    capturedCC_.store(-1, std::memory_order_relaxed);
}

int MidiLearn::getCCForParam(juce::RangedAudioParameter* param) const
{
    for (const auto& [cc, p] : bindings_)
        if (p == param)
            return cc;
    return -1;
}

void MidiLearn::unbind(int cc)
{
    if (cc < 0 || cc > 127)
        return;
    ccToParam_[static_cast<std::size_t>(cc)].store(nullptr, std::memory_order_relaxed);
    removeBindingsForCC(cc);
}

juce::XmlElement* MidiLearn::createBindingsXml() const
{
    auto* xml = new juce::XmlElement("MIDI_LEARN");
    for (const auto& [cc, param] : bindings_)
        if (param != nullptr) {
            auto* child = xml->createNewChildElement("BINDING");
            child->setAttribute("cc",      cc);
            child->setAttribute("paramID", param->getParameterID());
        }
    return xml;
}

void MidiLearn::restoreBindingsXml(
    const juce::XmlElement* xml,
    const juce::Array<juce::AudioProcessorParameter*>& params)
{
    if (xml == nullptr || xml->getTagName() != "MIDI_LEARN")
        return;

    // Clear existing state first.
    for (auto& a : ccToParam_)
        a.store(nullptr, std::memory_order_relaxed);
    bindings_.clear();

    for (auto* child : xml->getChildIterator()) {
        if (!child->hasTagName("BINDING"))
            continue;
        const int cc = child->getIntAttribute("cc", -1);
        if (cc < 0 || cc > 127)
            continue;
        const juce::String pid = child->getStringAttribute("paramID");
        for (auto* ap : params) {
            auto* rp = dynamic_cast<juce::RangedAudioParameter*>(ap);
            if (rp && rp->getParameterID() == pid) {
                ccToParam_[static_cast<std::size_t>(cc)].store(rp, std::memory_order_relaxed);
                bindings_.emplace_back(cc, rp);
                break;
            }
        }
    }
}
