
require 'mkmf'

case RUBY_PLATFORM
when /darwin/
  $PLATFORM = :macos
when /linux/
  $PLATFORM = :linux
  if `cat /etc/os-release` =~ /raspbian/
    $PLATFORM = :linux_rpi
  end
when /bsd/
  $PLATFORM = :bsd
when /mingw/
  $PLATFORM = :windows
else
  $PLATFORM = nil
end
$errors = []  # Holds errors

# Helper functions #############################################################

# Print installation errors
def print_errors
  puts "
#{"== #{"Ruby 2D Installation Errors".error} =======================================\n"}
  #{$errors.join("\n  ")}\n
#{"======================================================================"}"
end

# Add compiler and linker flags
def add_flags(type, flags)
  case type
  when :c
    $CFLAGS << " #{flags} "
  when :ld
    $LDFLAGS << " #{flags} "
  end
end

# Check for SDL libraries
def check_sdl
  unless have_library('SDL2') && have_library('SDL2_mixer')

    $errors << "Couldn't find packages needed."

    case $platform
    when :linux, :linux_rpi
      # Fedora and CentOS
      if system('which yum')
        $errors << "Install the following packages using `yum` (or `dnf`) and try again:\n" <<
          "  SDL2-devel SDL2_mixer-devel".bold

        # Arch
      elsif system('which pacman')
        $errors << "Install the following packages using `pacman` and try again:\n" <<
          "  sdl2 sdl2_mixer".bold

        # openSUSE
      elsif system('which zypper')
        $errors << "Install the following packages using `zypper` and try again:\n" <<
          "  libSDL2-devel libSDL2_mixer-devel".bold

        # Ubuntu, Debian, and Mint
        # `apt` must be last because openSUSE has it aliased to `zypper`
      elsif system('which apt')
        $errors << "Install the following packages using `apt` and try again:\n" <<
          "  libsdl2-dev libsdl2-mixer-dev".bold
      end
    when :bsd
      $errors << "Install the following packages using `pkg` and try again:\n" <<
        "  sdl2 sdl2_mixer".bold
    end

    $errors << "" << "See #{"ruby2d.com".bold} for additional help."
    print_errors; exit
  end
end

# Set Raspberry Pi flags
def set_rpi_flags
  if $platform == :linux_rpi
    add_flags(:c, '-I/opt/vc/include')
    add_flags(:ld, '-L/opt/vc/lib -lbrcmGLESv2')
  end
end

# Set flags for Linux and BSD
def set_linux_bsd_flags
  check_sdl
  set_rpi_flags
  add_flags(:ld, "-lSDL2 -lSDL2_mixer -lm")
  if $PLATFORM == :linux then add_flags(:ld, '-lGL') end
end


# Use SDL and other libraries installed by the user (not those bundled with the gem)
def use_usr_libs
  case $PLATFORM
  when :macos
    add_flags(:c, `sdl2-config --cflags`)
    add_flags(:c, '-I/opt/homebrew/include')
    add_flags(:ld, `sdl2-config --libs`)
    add_flags(:ld, '-lSDL2 -lSDL2_image -lSDL2_mixer -lSDL2_ttf')
    add_flags(:ld, '-Wl,-framework,OpenGL')
  when :windows
    add_flags(:ld, '-lSDL2 -lSDL2_mixer')
    add_flags(:ld, '-lopengl32 -lglew32')
  when :linux_rpi
    set_linux_bsd_flags
  end
end


# Configure native extension ###################################################

# Build Ruby 2D native extention using libraries installed by user
# To use install flag: `gem install ruby2d -- dev`
if ARGV.include? 'dev'
  use_usr_libs

  # Use libraries provided by the gem (default)
else
  add_flags(:c, '-std=c11')

  case $PLATFORM
  when :macos
    add_flags(:c, '-I../assets/include')
    ldir = "#{Dir.pwd}/../assets/macos/universal/lib"

    add_flags(:ld, "#{ldir}/libSDL2.a #{ldir}/libSDL2_mixer.a")
    add_flags(:ld, "#{ldir}/libmpg123.a #{ldir}/libogg.a #{ldir}/libFLAC.a #{ldir}/libvorbis.a #{ldir}/libvorbisfile.a #{ldir}/libmodplug.a")
    add_flags(:ld, "-lz -liconv -lstdc++")
    add_flags(:ld, "-Wl,-framework,Cocoa -Wl,-framework,Carbon -Wl,-framework,GameController -Wl,-framework,ForceFeedback -Wl,-framework,OpenGL -Wl,-framework,AudioToolbox -Wl,-framework,CoreAudio -Wl,-framework,IOKit -Wl,-framework,CoreHaptics -Wl,-framework,CoreVideo -Wl,-framework,Metal")

  when :windows
    add_flags(:c, '-I../assets/include')

    if RUBY_PLATFORM =~ /ucrt/
      ldir = "#{Dir.pwd}/../assets/windows/mingw-w64-ucrt-x86_64/lib"
    else
      ldir = "#{Dir.pwd}/../assets/windows/mingw-w64-x86_64/lib"
    end

    # Start linker flags (needed to avoid circular dependencies)
    add_flags(:ld, "-Wl,--start-group")

    # SDL2
    add_flags(:ld, "#{ldir}/libSDL2.a")

    # SDL2_mixer
    add_flags(:ld, "#{ldir}/libSDL2_mixer.a")
    add_flags(:ld, "#{ldir}/libmpg123.a #{ldir}/libFLAC.a #{ldir}/libvorbis.a #{ldir}/libvorbisfile.a #{ldir}/libogg.a "\
      "#{ldir}/libmodplug.a #{ldir}/libopus.a #{ldir}/libopusfile.a #{ldir}/libsndfile.a")

    # Other dependencies
    add_flags(:ld, "#{ldir}/libglew32.a #{ldir}/libstdc++.a #{ldir}/libz.a")
    add_flags(:ld, '-lmingw32 -lopengl32 -lole32 -loleaut32 -limm32 -lversion -lwinmm -lrpcrt4 -mwindows -lsetupapi -ldwrite')

    # End linker flags
    add_flags(:ld, "-Wl,--end-group")

  when :linux, :linux_rpi, :bsd
    set_linux_bsd_flags

    # If can't detect the platform, use libraries installed by the user
  else
    use_usr_libs
  end
end

$CFLAGS.gsub!(/\n/, ' ')  # remove newlines in flags, they can cause problems
$LDFLAGS.gsub!(/\n/, ' ')  # remove newlines in flags, they can cause problems

# Create Makefile
create_header
create_makefile('audio')
