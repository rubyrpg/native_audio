#!/usr/bin/env ruby
# frozen_string_literal: true

require_relative 'lib/native_audio'

clip = NativeAudio::Clip.new('knock.wav')
source = NativeAudio::AudioSource.new(clip)

puts "Playing with Schroeder reverb..."
source.play

# Enable reverb with room-like settings
source.set_reverb(
  room_size: 0.7,  # 0.0 = small, 1.0 = large hall
  damping: 0.5,    # High freq decay (0 = bright, 1 = muffled)
  wet: 0.5,        # Reverb level
  dry: 0.1         # Original signal level
)

sleep clip.duration + 1.5  # Extra time for reverb tail

puts "Done!"
