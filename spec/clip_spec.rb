require_relative 'spec_helper'

RSpec.describe NativeAudio::Clip do
  let(:clip) { NativeAudio::Clip.new('tap.wav') }

  it "loads a clip" do
    expect(clip.clip).to be_a(Integer)
  end

  it "returns a positive duration" do
    expect(clip.duration).to be > 0
  end
end
