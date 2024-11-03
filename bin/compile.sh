#!/usr/bin/env sh

cd ext

ruby extconf.rb
make
mv audio.bundle ../lib/audio.bundle

cd ..
