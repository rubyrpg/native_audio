require 'mkmf'

# miniaudio linking requirements per platform:
# - macOS:   CoreFoundation, CoreAudio, AudioToolbox frameworks
# - Linux:   pthread, dl, m (math)
# - Windows: uses native APIs via runtime linking, no extra libs needed

case RUBY_PLATFORM
when /darwin/
  $LDFLAGS << " -framework CoreFoundation -framework CoreAudio -framework AudioToolbox "
when /linux/
  $LDFLAGS << " -ldl -lpthread -lm "
when /mingw|mswin/
  # Windows uses runtime linking to native audio APIs
end

create_makefile('audio')
