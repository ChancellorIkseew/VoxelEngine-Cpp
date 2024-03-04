#ifndef AUDIO_NOAUDIO_H_
#define AUDIO_NOAUDIO_H_

#include "audio.h"

namespace audio {
    class NoSound : public Sound {
        std::shared_ptr<PCM> pcm;
        duration_t duration;
    public:
        NoSound(std::shared_ptr<PCM> pcm, bool keepPCM);
        ~NoSound() {}

        duration_t getDuration() const override {
            return duration;
        }

        std::shared_ptr<PCM> getPCM() const override {
            return pcm;
        }

        Speaker* newInstance(int priority, int channel) const override {
            return nullptr;
        }
    };

    class NoStream : public Stream {
        std::shared_ptr<PCMStream> source;
        duration_t duration;
    public:
        NoStream(std::shared_ptr<PCMStream> source, bool keepSource) {
            duration = source->getTotalDuration();
            if (keepSource) {
                this->source = source;
            }
        }

        std::shared_ptr<PCMStream> getSource() const override {
            return source;
        }

        void bindSpeaker(speakerid_t speaker) override {
        }

        Speaker* createSpeaker(bool loop, int channel) override{
            return nullptr;
        }

        speakerid_t getSpeaker() const override {
            return 0;
        }

        void update(double delta) override {
        }

        void setTime(duration_t time) override {
        }
    };

    class NoAudio : public Backend {
    public:
        ~NoAudio() {}

        Sound* createSound(std::shared_ptr<PCM> pcm, bool keepPCM) override;
        Stream* openStream(std::shared_ptr<PCMStream> stream, bool keepSource) override;

        void setListener(
            glm::vec3 position,
            glm::vec3 velocity,
            glm::vec3 at,
            glm::vec3 up
        ) override {}

        void update(double delta) override {}

        bool isDummy() const override {
            return true;
        }

        static NoAudio* create();
    };
}

#endif // AUDIO_NOAUDIO_H_