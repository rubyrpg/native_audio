require 'mkmf'

# Detect platform
PLATFORM = case RUBY_PLATFORM
when /darwin/ then :macos
when /linux/  then :linux
when /mingw/  then :windows
end

# miniaudio is a single-header library that uses native audio APIs:
# - macOS: CoreAudio (auto-linked)
# - Windows: Native APIs (auto-linked)
# - Linux: Needs pthread, math, dl

case PLATFORM
when :macos
  # CoreAudio frameworks are auto-detected by miniaudio
  $LDFLAGS << " -framework CoreFoundation -framework CoreAudio -framework AudioToolbox "
when :linux
  $LDFLAGS << " -lpthread -lm -ldl "
when :windows
  # Windows uses native APIs, no extra linking needed
end

create_makefile('audio')
