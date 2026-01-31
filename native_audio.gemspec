Gem::Specification.new do |spec|
  spec.name          = 'native_audio'
  spec.version       = '0.4.0'
  spec.authors       = ['Max Hatfull']
  spec.email         = ['max.hatfull@gmail.com']
  spec.summary       = 'A simple audio library for Ruby'
  spec.description   = 'A simple audio library for Ruby'
  spec.files         = Dir['ext/**/*.{rb,c,h}', 'lib/**/*.rb', 'README.md', 'LICENSE']
  spec.extensions    = ['ext/audio/extconf.rb']
  spec.homepage      = 'https://github.com/rubyrpg/native_audio'
  spec.license       = 'MIT'
end
