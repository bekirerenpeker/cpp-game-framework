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
    deviceConfig.pUserData = &AudioManager::get();

    // 3. Initialize and start the device
    if (ma_device_init(NULL, &deviceConfig, &m_device) != MA_SUCCESS) {
        LOG_ERROR("Failed to initialize audio device.");
        return;
    }
    ma_device_start(&m_device);

    m_busNodes["Master"] = new AudioBusNode("Master");
}
AudioManager::~AudioManager()
{
    for (auto& [name, nodePtr] : m_busNodes) delete nodePtr;
    m_busNodes.clear();

    ma_device_uninit(&m_device);
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
const std::vector<std::pair<IdType, AudioInstance>>& AudioManager::getAllInstances() const
{
    return m_instances.getAll();
}

// the function miniaudio calls to get the current frames values ran every few milliseconds
void AudioManager::data_callback(
    ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount
)
{
    // 1. Cast the user data back to our AudioManager
    AudioManager* manager = static_cast<AudioManager*>(pDevice->pUserData);
    if (!manager) return;

    // 2. The output buffer from miniaudio might contain garbage, ALWAYS clear it first
    float* pOutF32 = static_cast<float*>(pOutput);
    std::memset(pOutF32, 0, frameCount * 2 * sizeof(float));   // * 2 for stereo

    // 3. Create a temporary mixing buffer on the stack to hold data from a single instance
    // (A VLA or std::vector is slow here, use a fixed size buffer or alloca if frameCount varies wildly,
    // but a 4096 frame buffer is usually safe for most hardware callbacks)
    constexpr int MAX_FRAMES = 4096;
    float tempBuffer[MAX_FRAMES * 2] = {0};

    ma_uint32 framesToMix = std::min(frameCount, (ma_uint32)MAX_FRAMES);

    // 4. LOCK THE MUTEX
    // This stops the main thread from adding/removing sounds while we are reading them
    std::lock_guard<std::mutex> lock(manager->m_audioMutex);

    // 5. Loop over all active instances and mix them
    auto& instances = manager->m_instances.getAll();

    for (auto& pair : instances) {
        AudioInstance& instance = pair.second;

        // Clear temp buffer
        std::memset(tempBuffer, 0, framesToMix * 2 * sizeof(float));

        // Read data from the instance (which calls the source)
        if (instance.read(tempBuffer, framesToMix)) {

            // Apply volume from the combined PlaybackOptions
            float volume = instance.getOptions().volume;

            // Mix this instance into the main output buffer
            for (ma_uint32 i = 0; i < framesToMix * 2; ++i) {
                pOutF32[i] += tempBuffer[i] * volume;
            }
        }
    }
}
}   // namespace Engine
