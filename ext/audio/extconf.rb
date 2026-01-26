require 'mkmf'

# Get the root directory of the gem (two levels up from ext/audio/)
ROOT_DIR = File.expand_path('../..', __dir__)
ASSETS_DIR = File.join(ROOT_DIR, 'assets')

# Detect platform
PLATFORM = case RUBY_PLATFORM
when /darwin/ then :macos
when /linux/  then :linux
when /mingw/  then :windows
end

def add_flags(type, flags)
  case type
  when :c  then $CFLAGS << " #{flags} "
  when :ld then $LDFLAGS << " #{flags} "
  end
end

def check_sdl
  return if have_library('SDL2') && have_library('SDL2_mixer')

  msg = ["SDL2 libraries not found."]

  if PLATFORM == :linux
    if system('which apt >/dev/null 2>&1')
      msg << "Install with: sudo apt install libsdl2-dev libsdl2-mixer-dev"
    elsif system('which dnf >/dev/null 2>&1') || system('which yum >/dev/null 2>&1')
      msg << "Install with: sudo dnf install SDL2-devel SDL2_mixer-devel"
    elsif system('which pacman >/dev/null 2>&1')
      msg << "Install with: sudo pacman -S sdl2 sdl2_mixer"
    elsif system('which zypper >/dev/null 2>&1')
      msg << "Install with: sudo zypper install libSDL2-devel libSDL2_mixer-devel"
    end
  end

  abort msg.join("\n")
end

def set_linux_flags
  check_sdl
  add_flags(:ld, "-lSDL2 -lSDL2_mixer -lm")
end

def use_dev_libs
  case PLATFORM
  when :macos
    add_flags(:c, `sdl2-config --cflags`)
    add_flags(:c, '-I/opt/homebrew/include')
    add_flags(:ld, `sdl2-config --libs`)
    add_flags(:ld, '-lSDL2 -lSDL2_mixer')
  when :windows
    add_flags(:ld, '-lSDL2 -lSDL2_mixer')
  when :linux
    set_linux_flags
  end
end

def use_bundled_libs
  add_flags(:c, '-std=c11')

  case PLATFORM
  when :macos
    add_flags(:c, "-I#{ASSETS_DIR}/include")
    ldir = "#{ASSETS_DIR}/macos/universal/lib"

    add_flags(:ld, "#{ldir}/libSDL2.a #{ldir}/libSDL2_mixer.a")
    add_flags(:ld, "#{ldir}/libmpg123.a #{ldir}/libogg.a #{ldir}/libFLAC.a")
    add_flags(:ld, "#{ldir}/libvorbis.a #{ldir}/libvorbisfile.a #{ldir}/libmodplug.a")
    add_flags(:ld, "-lz -liconv -lstdc++")

    frameworks = %w[Cocoa Carbon GameController ForceFeedback
                    AudioToolbox CoreAudio IOKit CoreHaptics CoreVideo Metal]
    add_flags(:ld, frameworks.map { |f| "-Wl,-framework,#{f}" }.join(' '))

  when :windows
    add_flags(:c, "-I#{ASSETS_DIR}/include")

    ldir = if RUBY_PLATFORM =~ /ucrt/
      "#{ASSETS_DIR}/windows/mingw-w64-ucrt-x86_64/lib"
    else
      "#{ASSETS_DIR}/windows/mingw-w64-x86_64/lib"
    end

    add_flags(:ld, "-Wl,--start-group")
    add_flags(:ld, "#{ldir}/libSDL2.a #{ldir}/libSDL2_mixer.a")
    add_flags(:ld, "#{ldir}/libmpg123.a #{ldir}/libFLAC.a #{ldir}/libvorbis.a")
    add_flags(:ld, "#{ldir}/libvorbisfile.a #{ldir}/libogg.a #{ldir}/libmodplug.a")
    add_flags(:ld, "#{ldir}/libopus.a #{ldir}/libopusfile.a #{ldir}/libsndfile.a")
    add_flags(:ld, "#{ldir}/libstdc++.a #{ldir}/libz.a")
    add_flags(:ld, '-lmingw32 -lole32 -loleaut32 -limm32')
    add_flags(:ld, '-lversion -lwinmm -lrpcrt4 -mwindows -lsetupapi -ldwrite')
    add_flags(:ld, "-Wl,--end-group")

  when :linux
    set_linux_flags

  else
    use_dev_libs
  end
end

# Main configuration
if ARGV.include?('dev')
  use_dev_libs
else
  use_bundled_libs
end

$CFLAGS.gsub!(/\n/, ' ')
$LDFLAGS.gsub!(/\n/, ' ')

create_header
create_makefile('audio')
