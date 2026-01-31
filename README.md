# What is native_audio?

Native audio is a thin wrapper around miniaudio for simple audio playback from Ruby.

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

# Check duration
clip.duration  # => seconds

# Create an audio source to manage playback
audio_source = NativeAudio::AudioSource.new(clip)

# Play the clip
audio_source.play

# Pause and resume
audio_source.pause
audio_source.resume

# Set pitch (1.0 = normal, 0.5 = octave down, 2.0 = octave up)
audio_source.set_pitch(1.5)

# Set position relative to listener (angle: 0-360, distance: 0-255)
audio_source.set_pos(90, 200)  # right side, mid distance

# Set volume (0-128)
audio_source.set_volume(64)

# Stop the clip
audio_source.stop
```

## Development

```bash
bundle install
bundle exec rake compile
```

To test:

```bash
ruby test_audio.rb
```
