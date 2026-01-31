#!/usr/bin/env ruby
# frozen_string_literal: true

require_relative 'lib/native_audio'

clip = NativeAudio::Clip.new('boom.wav')

source1 = NativeAudio::AudioSource.new(clip)
source2 = NativeAudio::AudioSource.new(clip)

puts "Playing first boom (left)..."
source1.play
source1.set_pos(270, 255)  # left

sleep 3

puts "Playing second boom (right)..."
source2.play
source2.set_pos(90, 255)   # right

# Wait for both to finish
sleep clip.duration

puts "Done!"
