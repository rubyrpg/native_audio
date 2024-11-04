# What is native_audio?

Native audio is a thin wrapper around the SDL2 mixer library.
It is designed to allow simple audio playback from within a ruby program.

## Installation

Add this line to your application's Gemfile:

```ruby
gem 'native_audio'
```
And then execute:

    $ bundle

## Usage

```ruby
require 'native_audio'

# Load a clip
clip = NativeAudio::Clip.new('path/to/sound.wav')

# Create an audio source to manage playback
audio_source = NativeAudio::AudioSource.new(clip)

# Play the clip
audio_source.play

# Pause the clip
audio_source.pause

# Resume the clip

audio_source.resume

# Set the position of the source relative to the listener
angle = rand(360)
distance = rand(255)
audio_source.set_pos(angle, distance)

# Set the volume of the source
volume = rand(128)
audio_source.set_volume(volume)

# Stop the clip
audio_source.stop
```
