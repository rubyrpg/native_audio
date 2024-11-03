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

0.upto(360) do |angle|
  source.set_pos(angle, 5)
  sleep 0.02
end

source.stop

sleep 1
source.play
sleep 5



