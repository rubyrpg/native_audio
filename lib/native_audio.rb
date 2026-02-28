# frozen_string_literal: true

require_relative './audio'
require_relative './dummy_audio'

unless ENV['DUMMY_AUDIO_BACKEND'] == 'true'
  Audio.init
end

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
    attr_writer :id

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
      @delay_taps = []
      @params = {}
      @channel = nil
    end

    def play
      acquire_channel unless @channel
      NativeAudio.audio_driver.play(@channel, @clip.clip)
      apply_params
    end

    def stop
      NativeAudio.audio_driver.stop(@channel)
      self.class.owners.delete(@channel)
      @channel = nil
    end

    def channel_freed
      @channel = nil
    end

    def self.owners
      @owners ||= {}
    end

    def self.setup_channel_freed_callback
      NativeAudio.audio_driver.on_channel_freed(proc { |channel|
        owner = owners.delete(channel)
        owner&.channel_freed
      })
    end

    def pause
      NativeAudio.audio_driver.pause(@channel)
    end

    def resume
      NativeAudio.audio_driver.resume(@channel)
    end

    def set_pos(angle, distance)
      @params[:pos] = [angle, distance]
      NativeAudio.audio_driver.set_pos(@channel, angle, distance)
    end

    def set_pan(pan)
      @params[:pan] = pan
      NativeAudio.audio_driver.set_pan(@channel, pan)
    end

    def seek(seconds)
      NativeAudio.audio_driver.seek(@channel, seconds)
    end

    def set_volume(volume)
      @params[:volume] = volume
      NativeAudio.audio_driver.set_volume(@channel, volume)
    end

    def set_pitch(pitch)
      @params[:pitch] = pitch
      NativeAudio.audio_driver.set_pitch(@channel, pitch)
    end

    def set_looping(looping)
      @params[:looping] = looping
      NativeAudio.audio_driver.set_looping(@channel, looping)
    end

    def add_delay_tap(time_ms:, volume:)
      tap_id = NativeAudio.audio_driver.add_delay_tap(@channel, time_ms, volume)
      tap = DelayTap.new(self, tap_id, time_ms, volume)
      @delay_taps << tap
      tap
    end

    def enable_reverb(enabled = true)
      @params[:reverb_enabled] = enabled
      NativeAudio.audio_driver.enable_reverb(@channel, enabled)
    end

    def set_reverb(room_size: 0.5, damping: 0.3, wet: 0.3, dry: 1.0)
      @params[:reverb] = { room_size: room_size, damping: damping, wet: wet, dry: dry }
      NativeAudio.audio_driver.enable_reverb(@channel, true)
      NativeAudio.audio_driver.set_reverb_room_size(@channel, room_size)
      NativeAudio.audio_driver.set_reverb_damping(@channel, damping)
      NativeAudio.audio_driver.set_reverb_wet(@channel, wet)
      NativeAudio.audio_driver.set_reverb_dry(@channel, dry)
    end

    def delay_taps
      @delay_taps
    end

    private

    def acquire_channel
      @channel = NativeAudio.audio_driver.next_free_channel
      raise "No free audio channels available" if @channel < 0
      self.class.owners[@channel] = self
    end

    def apply_params
      NativeAudio.audio_driver.set_volume(@channel, @params[:volume]) if @params.key?(:volume)
      NativeAudio.audio_driver.set_pitch(@channel, @params[:pitch]) if @params.key?(:pitch)
      NativeAudio.audio_driver.set_looping(@channel, @params[:looping]) if @params.key?(:looping)
      NativeAudio.audio_driver.set_pan(@channel, @params[:pan]) if @params.key?(:pan)
      NativeAudio.audio_driver.set_pos(@channel, *@params[:pos]) if @params.key?(:pos)

      if @params.key?(:reverb)
        r = @params[:reverb]
        NativeAudio.audio_driver.enable_reverb(@channel, true)
        NativeAudio.audio_driver.set_reverb_room_size(@channel, r[:room_size])
        NativeAudio.audio_driver.set_reverb_damping(@channel, r[:damping])
        NativeAudio.audio_driver.set_reverb_wet(@channel, r[:wet])
        NativeAudio.audio_driver.set_reverb_dry(@channel, r[:dry])
      elsif @params.key?(:reverb_enabled)
        NativeAudio.audio_driver.enable_reverb(@channel, @params[:reverb_enabled])
      end

      @delay_taps.each do |tap|
        tap.id = NativeAudio.audio_driver.add_delay_tap(@channel, tap.time_ms, tap.volume)
      end
    end
  end

  AudioSource.setup_channel_freed_callback
end
