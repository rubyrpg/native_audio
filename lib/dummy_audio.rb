# frozen_string_literal: true

require 'set'

# Dummy audio backend for CI/testing environments without audio hardware.
# Has the same interface as the Audio C extension but does nothing.
module DummyAudio
  @sound_count = 0
  @tap_counts = {}
  @active_channels = Set.new
  @channel_freed_callback = nil

  def self.init
    nil
  end

  def self.load(path)
    id = @sound_count
    @sound_count += 1
    id
  end

  def self.duration(clip)
    5.0
  end

  def self.play(channel, clip)
    @tap_counts[channel] = 0
    @active_channels << channel
    channel
  end

  def self.stop(channel)
    @active_channels.delete(channel)
    nil
  end

  def self.pause(channel)
    nil
  end

  def self.resume(channel)
    nil
  end

  def self.set_volume(channel, volume)
    nil
  end

  def self.set_pitch(channel, pitch)
    nil
  end

  def self.set_pos(channel, angle, distance)
    nil
  end

  def self.set_pan(channel, pan)
    nil
  end

  def self.seek(channel, seconds)
    nil
  end

  def self.set_looping(channel, looping)
    nil
  end

  def self.add_delay_tap(channel, time_ms, volume)
    @tap_counts[channel] ||= 0
    tap_id = @tap_counts[channel]
    @tap_counts[channel] += 1
    tap_id
  end

  def self.remove_delay_tap(channel, tap_id)
    nil
  end

  def self.set_delay_tap_volume(channel, tap_id, volume)
    nil
  end

  def self.set_delay_tap_time(channel, tap_id, time_ms)
    nil
  end

  def self.enable_reverb(channel, enabled)
    nil
  end

  def self.set_reverb_room_size(channel, size)
    nil
  end

  def self.set_reverb_damping(channel, damp)
    nil
  end

  def self.set_reverb_wet(channel, wet)
    nil
  end

  def self.set_reverb_dry(channel, dry)
    nil
  end

  def self.next_free_channel
    (0..1023).find { |i| !@active_channels.include?(i) } || -1
  end

  def self.on_channel_freed(callback)
    @channel_freed_callback = callback
  end
end
