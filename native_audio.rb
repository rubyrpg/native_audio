# frozen_string_literal: true

require "./ext/audio"

module NativeAudio
  class Clip
    attr_reader :clip

    def initialize(path)
      @path = path
      @clip = Audio.load(path)
    end
  end

  class AudioSource
    def initialize(clip)
      @clip = clip
      @channel = AudioSource.channels.count
      AudioSource.channels << self
    end

    def play
      Audio.play(@channel, @clip.clip)
    end

    def stop
      Audio.stop(@channel)
    end

    def pause
      Audio.pause(@channel)
    end

    def resume
      Audio.resume(@channel)
    end

    def set_pos(angle, distance)
      Audio.set_pos(@channel, angle, distance)
    end

    def self.channels
      @channels ||= []
    end
  end
end

clip = NativeAudio::Clip.new("boom.wav")

source = NativeAudio::AudioSource.new(clip)
source.play
sleep 3
source.pause
sleep 1
source.resume
sleep 3



