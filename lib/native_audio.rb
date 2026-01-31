# frozen_string_literal: true

$stderr.puts "[native_audio.rb] starting require"
$stderr.flush

require_relative './audio'

$stderr.puts "[native_audio.rb] audio extension loaded"
$stderr.flush

$stderr.puts "[native_audio.rb] calling Audio.init..."
$stderr.flush

Audio.init

$stderr.puts "[native_audio.rb] Audio.init complete"
$stderr.flush

$stderr.puts "[native_audio.rb] defining module..."
$stderr.flush

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

$stderr.puts "[native_audio.rb] module defined, require complete"
$stderr.flush
