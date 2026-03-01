ENV['NATIVE_AUDIO_DRIVER'] = 'null'

require_relative '../lib/native_audio'

RSpec.configure do |config|
  config.after(:each) do
    NativeAudio::AudioSource.owners.each_value(&:stop)
    NativeAudio::AudioSource.owners.clear
    NativeAudio.audio_driver.reset_all_channels
  end
end
