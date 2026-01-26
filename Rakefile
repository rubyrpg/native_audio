# frozen_string_literal: true

require "rake/extensiontask"
require "rubygems/package_task"

GEMSPEC = Gem::Specification.load("native_audio.gemspec")

Gem::PackageTask.new(GEMSPEC) do |pkg|
  pkg.need_zip = false
  pkg.need_tar = false
end

Rake::ExtensionTask.new("audio", GEMSPEC) do |ext|
  ext.lib_dir = "lib"
end

task default: :compile
