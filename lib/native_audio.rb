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

  class DelayTap
    attr_reader :id, :audio_source, :time_ms, :volume

    def initialize(audio_source, id, time_ms, volume)
      @audio_source = audio_source
      @id = id
      @time_ms = time_ms
      @volume = volume
    end

    def volume=(val)
      NativeAudio.audio_driver.set_delay_tap_volume(@audio_source.channel, @id, val)
      @volume = val
    end

    def time_ms=(val)
      NativeAudio.audio_driver.set_delay_tap_time(@audio_source.channel, @id, val)
      @time_ms = val
    end

    def remove
      NativeAudio.audio_driver.remove_delay_tap(@audio_source.channel, @id)
      @audio_source.delay_taps.delete(self)
    end
  end

  class AudioSource
    attr_reader :channel

    def initialize(clip)
      @clip = clip
      @channel = AudioSource.channels.count
      @delay_taps = []
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

    def set_looping(looping)
      NativeAudio.audio_driver.set_looping(@channel, looping)
    end

    def add_delay_tap(time_ms:, volume:)
      tap_id = NativeAudio.audio_driver.add_delay_tap(@channel, time_ms, volume)
      tap = DelayTap.new(self, tap_id, time_ms, volume)
      @delay_taps << tap
      tap
    end

    def enable_reverb(enabled = true)
      NativeAudio.audio_driver.enable_reverb(@channel, enabled)
    end

    def set_reverb(room_size: 0.5, damping: 0.3, wet: 0.3, dry: 1.0)
      NativeAudio.audio_driver.enable_reverb(@channel, true)
      NativeAudio.audio_driver.set_reverb_room_size(@channel, room_size)
      NativeAudio.audio_driver.set_reverb_damping(@channel, damping)
      NativeAudio.audio_driver.set_reverb_wet(@channel, wet)
      NativeAudio.audio_driver.set_reverb_dry(@channel, dry)
    end

    def delay_taps
      @delay_taps
    end

    def self.channels
      @channels ||= []
    end
  end
end
