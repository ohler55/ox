#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: the name caches had no lock, and Ractors adding names at
# the same time corrupted them (ox #277).

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class LoadRactorTest < ::Test::Unit::TestCase
  def test_load_new_attribute_names_in_ractors
    omit('needs Ractor') unless defined?(Ractor)
    experimental = Warning[:experimental]
    Warning[:experimental] = false
    ractors = Array.new(4) do |k|
      Ractor.new(k) do |id|
        20_000.times.count do |i|
          e = Ox.load(%(<e a#{id}_#{i}="x" b="y"><c d#{i}="z"/></e>), mode: :generic)
          e.attributes == { "a#{id}_#{i}": 'x', b: 'y' }
        end
      end
    end
    counts = Thread.new { ractors.map { |r| r.respond_to?(:value) ? r.value : r.take } }
    # Ruby 4.1 runs the GC per Ractor, so allocate here while the Ractors intern.
    loops = 0
    until counts.join(0)
      1000.times { Array.new(100) { +'x' * 10 } }
      loops += 1
      GC.start if (loops % 20).zero?
    end
    assert_equal([20_000] * 4, counts.value)
  ensure
    Warning[:experimental] = experimental unless experimental.nil?
  end
end
