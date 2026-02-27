#!/usr/bin/env ruby
# frozen_string_literal: true

require_relative 'lib/native_audio'

clip = NativeAudio::Clip.new('tap.wav')
source = NativeAudio::AudioSource.new(clip)
source2 = NativeAudio::AudioSource.new(clip)
puts "Playing with looping, delay + reverb..."
source.play
source2.set_looping(true)
source2.play

# Add a slapback echo
#source.add_delay_tap(time_ms: 200, volume: 0.5)

# Then reverb processes both original and echo
source.set_reverb(
  room_size: 0.7,
  damping: 0.5,
  wet: 0.5,
  dry: 0.5
)

sleep 0.1
source.stop

sleep 1
source.play
sleep 3

puts "Done!"
