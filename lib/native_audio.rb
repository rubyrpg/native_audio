# frozen_string_literal: true

require_relative './audio'
require_relative './dummy_audio'

Audio.init unless ENV['DUMMY_AUDIO_BACKEND'] == 'true'

module NativeAudio
  def self.audio_driver
    ENV['DUMMY_AUDIO_BACKEND'] == 'true' ? DummyAudio : Audio
  end

  class Clip
    attr_reader :clip

    def initialize(path)
      @path = path
      @clip = NativeAudio.audio_driver.load(path)
    end

    def duration
      NativeAudio.audio_driver.duration(@clip)
    end
  end

  class AudioSource
    def initialize(clip)
      @clip = clip
      @channel = AudioSource.channels.count
      AudioSource.channels << self
    end

    def play
      NativeAudio.audio_driver.play(@channel, @clip.clip)
    end

    def stop
      NativeAudio.audio_driver.stop(@channel)
    end

    def pause
      NativeAudio.audio_driver.pause(@channel)
    end

    def resume
      NativeAudio.audio_driver.resume(@channel)
    end

    def set_pos(angle, distance)
      NativeAudio.audio_driver.set_pos(@channel, angle, distance)
    end

    def set_volume(volume)
      NativeAudio.audio_driver.set_volume(@channel, volume)
    end

    def set_pitch(pitch)
      NativeAudio.audio_driver.set_pitch(@channel, pitch)
    end

    def self.channels
      @channels ||= []
    end
  end
end
