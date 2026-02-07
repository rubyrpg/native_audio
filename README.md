# What is native_audio?

Native audio is a thin wrapper around miniaudio for simple audio playback from Ruby, with built-in support for delay and reverb effects.

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
source = NativeAudio::AudioSource.new(clip)

# Play the clip
source.play

# Pause and resume
source.pause
source.resume

# Set pitch (1.0 = normal, 0.5 = octave down, 2.0 = octave up)
source.set_pitch(1.5)

# Set position relative to listener (angle: 0-360, distance: 0-255)
source.set_pos(90, 200)  # right side, mid distance

# Set volume (0-128)
source.set_volume(64)

# Loop playback
source.set_looping(true)

# Stop the clip
source.stop
```

## Effects

Each audio source has a built-in effects chain:

```
sound ──▶ delay ──▶ reverb ──▶ output
```

### Delay Taps

Add discrete echo effects with up to 16 taps per source:

```ruby
source.play

# Add delay taps (returns a DelayTap object)
tap1 = source.add_delay_tap(time_ms: 200, volume: 0.5)
tap2 = source.add_delay_tap(time_ms: 400, volume: 0.3)

# Modify taps at runtime
tap1.volume = 0.4
tap1.time_ms = 250

# Remove a tap
tap2.remove

# Query active taps
source.delay_taps  # => [tap1]
```

### Reverb

Add room ambience with a Schroeder reverb:

```ruby
source.play

# Enable reverb with custom settings
source.set_reverb(
  room_size: 0.7,  # 0.0 = small room, 1.0 = large hall
  damping: 0.5,    # 0.0 = bright, 1.0 = muffled
  wet: 0.4,        # reverb signal level
  dry: 1.0         # original signal level
)

# Or just enable with defaults
source.enable_reverb
source.enable_reverb(false)  # disable
```

### Combining Effects

Delay and reverb work together - each echo gets reverb applied:

```ruby
source.play

# Slapback echo with room reverb
source.add_delay_tap(time_ms: 150, volume: 0.4)
source.set_reverb(room_size: 0.5, wet: 0.3, dry: 1.0)
```

## Environment Variables

### `NATIVE_AUDIO_DRIVER`

Set `NATIVE_AUDIO_DRIVER=null` to use miniaudio's null backend. This initializes the audio engine but outputs no sound - useful for CI/testing on systems without audio hardware.

```bash
NATIVE_AUDIO_DRIVER=null ruby your_script.rb
```

### `DUMMY_AUDIO_BACKEND`

Set `DUMMY_AUDIO_BACKEND=true` to bypass miniaudio entirely and use a pure Ruby dummy backend. The C extension still loads (validating it compiles correctly), but no audio engine is initialized and all audio calls are no-ops.

```bash
DUMMY_AUDIO_BACKEND=true ruby your_script.rb
```

> **Note:** On Windows Server (e.g., GitHub Actions runners), miniaudio's null backend crashes due to threading issues in headless environments. Use `DUMMY_AUDIO_BACKEND=true` instead. macOS and Linux work fine with `NATIVE_AUDIO_DRIVER=null`.

## Development

```bash
bundle install
bundle exec rake compile
```

To test:

```bash
ruby test_audio.rb
```

## Releasing

1. Update the version in `native_audio.gemspec`
2. Go to GitHub Actions → "Release to RubyGems"
3. Click "Run workflow" → "Run workflow"
