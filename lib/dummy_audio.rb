# frozen_string_literal: true

# Dummy audio backend for CI/testing environments without audio hardware.
# Has the same interface as the Audio C extension but does nothing.
module DummyAudio
  @sound_count = 0
  @tap_counts = {}

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
    channel
  end

  def self.stop(channel)
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
end
