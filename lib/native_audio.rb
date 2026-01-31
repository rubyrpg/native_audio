# frozen_string_literal: true

require_relative './audio'

module NativeAudio
  class Clip
    attr_reader :clip

    def initialize(path)
      @path = path
      @clip = Audio.load(path)
    end

    def duration
      Audio.duration(@clip)
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

    def set_volume(volume)
      Audio.set_volume(@channel, volume)
    end

    def set_pitch(pitch)
      Audio.set_pitch(@channel, pitch)
    end

    def self.channels
      @channels ||= []
    end
  end
end
