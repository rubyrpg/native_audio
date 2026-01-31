# frozen_string_literal: true

# Dummy audio backend for CI/testing environments without audio hardware.
# Has the same interface as the Audio C extension but does nothing.
module DummyAudio
  @sound_count = 0

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
end
