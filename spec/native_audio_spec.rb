# frozen_string_literal: true

require_relative 'spec_helper'

RSpec.describe NativeAudio::Clip do
  let(:clip) { NativeAudio::Clip.new('tap.wav') }

  it "loads a clip" do
    expect(clip.clip).to be_a(Integer)
  end

  it "returns a positive duration" do
    expect(clip.duration).to be > 0
  end
end

RSpec.describe NativeAudio::AudioSource do
  let(:clip) { NativeAudio::Clip.new('tap.wav') }

  describe "lifecycle" do
    it "starts with nil channel" do
      source = NativeAudio::AudioSource.new(clip)
      expect(source.channel).to be_nil
    end

    it "acquires a channel on play" do
      source = NativeAudio::AudioSource.new(clip)
      source.play
      expect(source.channel).to be_a(Integer)
      expect(source.channel).to be >= 0
    end

    it "nils channel on stop" do
      source = NativeAudio::AudioSource.new(clip)
      source.play
      source.stop
      expect(source.channel).to be_nil
    end

    it "can play again after stop" do
      source = NativeAudio::AudioSource.new(clip)
      source.play
      source.stop
      expect { source.play }.not_to raise_error
      expect(source.channel).to be >= 0
    end
  end

  describe "channel allocation" do
    it "assigns different channels to concurrent sources" do
      a = NativeAudio::AudioSource.new(clip)
      b = NativeAudio::AudioSource.new(clip)
      a.play
      b.play
      expect(a.channel).not_to eq(b.channel)
    end

    it "does not exhaust channels when repeatedly creating and stopping sources" do
      100.times do
        source = NativeAudio::AudioSource.new(clip)
        source.play
        source.stop
      end

      source = NativeAudio::AudioSource.new(clip)
      expect { source.play }.not_to raise_error
      expect(source.channel).to be < 1024
    end
  end

  describe "channel freed callback" do
    it "can play again after channel is freed by cleanup" do
      source = NativeAudio::AudioSource.new(clip)
      source.play
      source.set_looping(false)

      sleep(clip.duration + 0.1)

      # Trigger cleanup which fires the callback and nils source's channel
      other = NativeAudio::AudioSource.new(clip)
      other.play
      expect(source.channel).to be_nil

      # Source should acquire a new channel when played again
      expect { source.play }.not_to raise_error
      expect(source.channel).to be_a(Integer)
      expect(source.channel).to be >= 0
    end

    it "does not steal another source's channel when replaying after completion" do
      a = NativeAudio::AudioSource.new(clip)
      b = NativeAudio::AudioSource.new(clip)

      a.play
      a.set_looping(false)
      b.play
      b.set_looping(true)

      b_channel = b.channel

      # Let a's clip finish, then trigger cleanup via a new play
      sleep(clip.duration + 0.1)
      a.play

      # a should get a different channel, not steal b's
      expect(a.channel).not_to eq(b_channel)
      expect(b.channel).to eq(b_channel)
    end

    it "nils channel when a non-looping clip finishes and cleanup runs" do
      source = NativeAudio::AudioSource.new(clip)
      source.play
      source.set_looping(false)

      channel_before = source.channel
      expect(channel_before).not_to be_nil

      # Wait for the null backend to finish processing the clip,
      # then trigger cleanup_finished_channels via play.
      sleep(clip.duration + 0.1)
      other = NativeAudio::AudioSource.new(clip)
      other.play

      expect(source.channel).to be_nil
    end
  end

  describe "channel recycling under pressure" do
    it "can play more than 1024 total sources by recycling completed channels" do
      played = 0

      5.times do
        sources = 250.times.map do
          s = NativeAudio::AudioSource.new(clip)
          s.play
          s.set_looping(false)
          played += 1
          s
        end

        # Wait for clips to finish, then next batch's play calls
        # trigger cleanup which reclaims the completed channels.
        sleep(clip.duration + 0.1)
      end

      # We've played 1250 sources total without hitting the 1024 limit.
      expect(played).to eq(1250)

      # And we can still play more
      final = NativeAudio::AudioSource.new(clip)
      expect { final.play }.not_to raise_error
    end
  end

  describe "reusing a draining channel" do
    it "can play on a channel that is still draining effects" do
      source = NativeAudio::AudioSource.new(clip)
      source.play
      source.set_reverb(room_size: 0.8, damping: 0.5, wet: 0.5, dry: 0.5)
      source.add_delay_tap(time_ms: 200.0, volume: 0.4)
      source.stop

      # Channel is now draining (sound freed, effects still active).
      # A new source should be able to grab it and set up its own effects.
      new_source = NativeAudio::AudioSource.new(clip)
      new_source.play

      expect {
        new_source.set_reverb(room_size: 0.3, damping: 0.2, wet: 0.2, dry: 0.8)
      }.not_to raise_error

      expect {
        new_source.add_delay_tap(time_ms: 100.0, volume: 0.3)
      }.not_to raise_error
    end

    it "effects on a new source don't crash after reusing a drained channel" do
      # Play and stop with effects to create a draining channel
      old = NativeAudio::AudioSource.new(clip)
      old.play
      old.set_reverb(room_size: 0.9, damping: 0.5, wet: 0.6, dry: 0.4)
      tap = old.add_delay_tap(time_ms: 150.0, volume: 0.5)
      old.stop

      # Reuse the channel
      fresh = NativeAudio::AudioSource.new(clip)
      fresh.play
      fresh_tap = fresh.add_delay_tap(time_ms: 300.0, volume: 0.6)

      # Modify the new tap - should work and not affect old drained effects
      expect { fresh_tap.volume = 0.2 }.not_to raise_error
      expect { fresh_tap.time_ms = 400.0 }.not_to raise_error
      expect(fresh_tap.volume).to eq(0.2)
      expect(fresh_tap.time_ms).to eq(400.0)
    end
  end

  describe "parameters survive stop/play cycle" do
    it "can set volume, pitch, pan, looping, then stop and play without crashing" do
      source = NativeAudio::AudioSource.new(clip)
      source.play
      source.set_volume(64)
      source.set_pitch(1.5)
      source.set_pan(-0.5)
      source.set_looping(true)
      source.stop

      expect { source.play }.not_to raise_error
    end
  end

  describe "pause and resume" do
    it "can pause and resume a playing source" do
      source = NativeAudio::AudioSource.new(clip)
      source.play
      expect { source.pause }.not_to raise_error
      expect { source.resume }.not_to raise_error
    end
  end

  describe "reverb" do
    it "can set reverb parameters" do
      source = NativeAudio::AudioSource.new(clip)
      source.play
      expect {
        source.set_reverb(room_size: 0.8, damping: 0.4, wet: 0.6, dry: 0.4)
      }.not_to raise_error
    end
  end

  describe "delay taps" do
    it "can add and remove delay taps" do
      source = NativeAudio::AudioSource.new(clip)
      source.play
      tap = source.add_delay_tap(time_ms: 200.0, volume: 0.5)
      expect(tap).to be_a(NativeAudio::DelayTap)
      expect(source.delay_taps.size).to eq(1)

      tap.remove
      expect(source.delay_taps.size).to eq(0)
    end

    it "can modify delay tap parameters" do
      source = NativeAudio::AudioSource.new(clip)
      source.play
      tap = source.add_delay_tap(time_ms: 200.0, volume: 0.5)

      expect { tap.volume = 0.3 }.not_to raise_error
      expect { tap.time_ms = 300.0 }.not_to raise_error

      expect(tap.volume).to eq(0.3)
      expect(tap.time_ms).to eq(300.0)
    end
  end
end
