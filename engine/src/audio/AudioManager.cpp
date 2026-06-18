#include "audio/AudioManager.hpp"
#include "core/logging/LoggerMacros.hpp"

namespace Engine {

AudioManager::AudioManager()
{
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = ma_format_f32;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate = 48000;

    // 2. Link the callback and pass 'this' so the static function can access the class
    deviceConfig.dataCallback = data_callback;
    deviceConfig.pUserData = this;

    // 3. Initialize and start the device
    if (ma_device_init(NULL, &deviceConfig, &m_device) != MA_SUCCESS) {
        LOG_ERROR("Failed to initialize audio device.");
        return;
    }
    ma_device_start(&m_device);

    m_busNodes["Master"] = new AudioBusNode("Master");
}
AudioManager::~AudioManager() { shutdown(); }
void AudioManager::shutdown()
{
    ma_device_uninit(&m_device);

    m_instances.clear();

    for (auto& [name, nodePtr] : m_busNodes) delete nodePtr;
    m_busNodes.clear();
}

void AudioManager::addBusNode(
    const std::string& name, const std::string& parentName, const PlaybackOptions& initalOptions
)
{
    if (!m_busNodes.count(parentName)) {
        LOG_ERROR("trying to insert a new bus node under {} but it doesn't exist", parentName);
        return;
    }
    AudioBusNode* parent = m_busNodes[parentName];
    m_busNodes[name] = new AudioBusNode(name, parent, initalOptions);
}
AudioBusNode* AudioManager::getBusNode(const std::string& name)
{
    return m_busNodes.count(name) ? m_busNodes[name] : nullptr;
}
bool AudioManager::removeBusNode(const std::string& name)
{
    if (!m_busNodes.count(name)) return false;

    std::queue<std::string> queue;
    queue.push(name);

    while (!queue.empty()) {
        std::string currName = queue.front();
        queue.pop();

        AudioBusNode* currNode = m_busNodes[currName];
        for (AudioBusNode* child : currNode->getChildren()) { queue.push(child->getName()); }

        m_busNodes.erase(currName);
        delete currNode;
    }

    return true;
}

IdType AudioManager::playAudio(
    IAudioSource* source, const std::string& busName, const PlaybackOptions& options
)
{
    if (!m_busNodes.count(busName)) {
        LOG_ERROR("trying to play audio under bus {} but it doesn't exist", busName);
        return INVALID_ID;
    }
    AudioBusNode* busNode = m_busNodes[busName];

    std::lock_guard<std::mutex> lock(m_audioMutex);
    return m_instances.add(AudioInstance(source, options.combined(busNode->getOptions())));
}
void AudioManager::stopAudioInstance(IdType id)
{
    std::lock_guard<std::mutex> lock(m_audioMutex);
    m_instances.remove(id);
}
AudioInstance* AudioManager::getAudioInstance(IdType id) { return m_instances.get(id); }

// the function miniaudio calls to get the current frames values ran every few milliseconds
void AudioManager::data_callback(
    ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount
)
{
    AudioManager* manager = static_cast<AudioManager*>(pDevice->pUserData);
    if (!manager) return;

    float* pOutF32 = static_cast<float*>(pOutput);
    std::memset(pOutF32, 0, frameCount * 2 * sizeof(float));   // * 2 for stereo

    // (A VLA or std::vector is slow here, use a fixed size buffer or alloca if frameCount varies wildly,
    // but a 4096 frame buffer is usually safe for most hardware callbacks)
    constexpr int MAX_FRAMES = 4096;
    float tempBuffer[MAX_FRAMES * 2] = {0};

    ma_uint32 framesToMix = std::min(frameCount, (ma_uint32)MAX_FRAMES);

    // This stops the main thread from adding/removing sounds while we are reading them
    std::lock_guard<std::mutex> lock(manager->m_audioMutex);

    auto& instances = manager->m_instances.getAll();

    // stack allocated array for performance
    constexpr int MAX_FINISHED_PER_FRAME = 64;
    IdType finishedIds[MAX_FINISHED_PER_FRAME];
    int finishedCount = 0;

    for (auto& pair : instances) {
        IdType id = pair.first;
        AudioInstance& instance = pair.second;
        if (instance.isPaused()) continue;

        std::memset(tempBuffer, 0, framesToMix * 2 * sizeof(float));

        if (instance.read(tempBuffer, framesToMix)) {
            float volume = instance.getOptions().volume;
            float pan = instance.getOptions().pan;

            float leftMultiplier = (pan <= 0.0f) ? 1.0f : (1.0f - pan);
            float rightMultiplier = (pan >= 0.0f) ? 1.0f : (1.0f + pan);
            float leftVol = volume * leftMultiplier;
            float rightVol = volume * rightMultiplier;

            for (ma_uint32 i = 0; i < framesToMix; ++i) {
                pOutF32[i * 2 + 0] += tempBuffer[i * 2 + 0] * leftVol;
                pOutF32[i * 2 + 1] += tempBuffer[i * 2 + 1] * rightVol;
            }
        } else if (finishedCount < MAX_FINISHED_PER_FRAME) {
            finishedIds[finishedCount++] = id;
        }
    }

    for (int i = 0; i < finishedCount; ++i) manager->m_instances.remove(finishedIds[i]);
}

ma_uint64 AudioManager::getCursorFrames(IdType id)
{
    std::lock_guard<std::mutex> lock(m_audioMutex);
    AudioInstance* inst = m_instances.get(id);
    return inst ? inst->getCursorFrames() : 0;
}
float AudioManager::getCursorSeconds(IdType id)
{
    std::lock_guard<std::mutex> lock(m_audioMutex);
    AudioInstance* inst = m_instances.get(id);
    return inst ? inst->getCursorSeconds() : 0.0f;
}
void AudioManager::setCursorFrames(IdType id, ma_uint64 cursor)
{
    std::lock_guard<std::mutex> lock(m_audioMutex);
    AudioInstance* inst = m_instances.get(id);
    if (inst) { inst->setCursorFrames(cursor); }
}
void AudioManager::setCursorSeconds(IdType id, float seconds)
{
    std::lock_guard<std::mutex> lock(m_audioMutex);
    AudioInstance* inst = m_instances.get(id);
    if (inst) { inst->setCursorSeconds(seconds); }
}
ma_uint64 AudioManager::getDurationFrames(IdType id)
{
    std::lock_guard<std::mutex> lock(m_audioMutex);
    AudioInstance* inst = m_instances.get(id);
    if (inst && inst->getSource()) { return inst->getSource()->getDurationFrames(); }
    return 0;
}
float AudioManager::getDurationSeconds(IdType id)
{
    std::lock_guard<std::mutex> lock(m_audioMutex);
    AudioInstance* inst = m_instances.get(id);
    if (inst && inst->getSource()) { return inst->getSource()->getDurationSeconds(); }
    return 0.0f;
}
bool AudioManager::isPaused(IdType id)
{
    std::lock_guard<std::mutex> lock(m_audioMutex);
    AudioInstance* inst = m_instances.get(id);
    return inst ? inst->isPaused() : false;
}
void AudioManager::setIsPaused(IdType id, bool isPaused)
{
    std::lock_guard<std::mutex> lock(m_audioMutex);
    AudioInstance* inst = m_instances.get(id);
    if (inst) { inst->setIsPaused(isPaused); }
}
PlaybackOptions AudioManager::getOptions(IdType id)
{
    std::lock_guard<std::mutex> lock(m_audioMutex);
    AudioInstance* inst = m_instances.get(id);
    return inst ? inst->getOptions() : PlaybackOptions {};
}
void AudioManager::setOptions(IdType id, const PlaybackOptions& options)
{
    std::lock_guard<std::mutex> lock(m_audioMutex);
    AudioInstance* inst = m_instances.get(id);
    if (inst) { inst->setOptions(options); }
}

}   // namespace Engine
