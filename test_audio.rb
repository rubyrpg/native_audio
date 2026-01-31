#!/usr/bin/env ruby
# frozen_string_literal: true

require_relative 'lib/native_audio'

# Generate a simple test WAV file (440Hz sine wave)
def generate_test_wav(path, duration: 1.0, frequency: 440)
  sample_rate = 44100
  num_samples = (sample_rate * duration).to_i

  samples = num_samples.times.map do |i|
    amplitude = 0.3
    t = i.to_f / sample_rate
    (amplitude * Math.sin(2 * Math::PI * frequency * t) * 32767).to_i
  end

  File.open(path, 'wb') do |f|
    # WAV header
    f.write('RIFF')
    f.write([36 + num_samples * 2].pack('V'))  # file size - 8
    f.write('WAVE')
    f.write('fmt ')
    f.write([16].pack('V'))                     # chunk size
    f.write([1].pack('v'))                      # audio format (PCM)
    f.write([1].pack('v'))                      # num channels
    f.write([sample_rate].pack('V'))            # sample rate
    f.write([sample_rate * 2].pack('V'))        # byte rate
    f.write([2].pack('v'))                      # block align
    f.write([16].pack('v'))                     # bits per sample
    f.write('data')
    f.write([num_samples * 2].pack('V'))        # data size
    samples.each { |s| f.write([s].pack('s<')) }
  end
end

puts "Generating test WAV file..."
test_file = '/tmp/test_tone.wav'
generate_test_wav(test_file, duration: 2.0, frequency: 440)

puts "Loading clip..."
clip = NativeAudio::Clip.new(test_file)

puts "Creating audio source..."
source = NativeAudio::AudioSource.new(clip)

puts "Playing (center)..."
source.play
sleep 0.5

puts "Setting volume to 50%..."
source.set_volume(64)
sleep 0.5

puts "Panning right (90 degrees)..."
source.set_pos(90, 128)
sleep 0.5

puts "Panning left (270 degrees)..."
source.set_pos(270, 128)
sleep 0.5

puts "Back to center..."
source.set_pos(0, 0)
sleep 0.5

puts "Pausing..."
source.pause
sleep 0.3

puts "Resuming..."
source.resume
sleep 0.5

puts "Stopping..."
source.stop

puts "Done!"
