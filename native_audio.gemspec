Gem::Specification.new do |spec|
  spec.name          = 'native_audio'
  spec.version       = '0.1.0'
  spec.authors       = ['Max Hatfull']
  spec.email         = ['max.hatfull@gmail.com']
  spec.summary       = 'A simple audio library for Ruby'
  spec.description   = 'A simple audio library for Ruby'
  spec.files         = Dir['ext/**/*', 'lib/**/*', 'assets/**/*']
  spec.extensions    = ['ext/extconf.rb']
  spec.homepage      = 'https://github.com/rubyrpg/native_audio'
  spec.license       = 'MIT'
end
