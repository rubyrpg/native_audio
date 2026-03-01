# frozen_string_literal: true

ENV['NATIVE_AUDIO_DRIVER'] = 'null'

require_relative '../lib/native_audio'

RSpec.configure do |config|
  config.after(:each) do
    # Stop all active sources to free channels between tests
    NativeAudio::AudioSource.owners.each_value(&:stop)
  end
end
