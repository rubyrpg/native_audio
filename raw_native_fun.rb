# frozen_string_literal: true

require "./ext/foobar"

puts Foobar::Foo.new.bar
Foobar::Foo.new.play
sleep 3
