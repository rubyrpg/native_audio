#!/usr/bin/env ruby
# frozen_string_literal: true

require_relative 'lib/native_audio'

clip = NativeAudio::Clip.new('knock.wav')
source = NativeAudio::AudioSource.new(clip)

puts "Playing with multi-tap delay..."
source.play

# Add echo-like delay taps
# tap2 = source.add_delay_tap(time_ms: 25, volume: 0.2)
# tap3 = source.add_delay_tap(time_ms: 50, volume: 0.2)
tap4 = source.add_delay_tap(time_ms: 75, volume: 0.3)

sleep clip.duration + 0.7

puts "Done!"
