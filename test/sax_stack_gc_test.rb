#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: the names of open elements being collected by the GC.
#
# The SAX parser keeps the name of each open element on a stack that starts in
# its C stack frame and moves to the heap once more than 32 elements are open.
# Nothing marked the names on the heap copy, so a name only referenced from
# there could be freed while its element was still open, and end_element was
# then called with whatever object took over the slot.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'stringio'
require 'test/unit'
require 'ox'

class SaxStackGcTest < ::Test::Unit::TestCase
  DEPTH = 40

  class Deep < ::Ox::Sax
    attr_reader :ends

    def initialize
      super
      @ends = []
    end

    # Collect while every element is open, then allocate so the freed slots
    # get reused.
    def text(_value)
      6.times { GC.start(full_mark: true, immediate_sweep: true) }
      @junk = Array.new(10_000) { |i| [i] }
    end

    # Record what came in without keeping a reference to it.
    def end_element(name)
      @ends << "#{name.class} #{name}"
    end
  end

  def check(names, symbolize)
    xml = names.map { |n| "<#{n}>" }.join + 'x' + names.reverse.map { |n| "</#{n}>" }.join
    h = Deep.new
    Ox.sax_parse(h, StringIO.new(xml), symbolize: symbolize)
    klass = symbolize ? 'Symbol' : 'String'
    assert_equal(names.reverse.map { |n| "#{klass} #{n}" }, h.ends)
  end

  # Short names come from the name cache, which lets go of them after a few
  # GCs. Names of 35 bytes or more are never cached.
  def test_short_names
    names = (0...DEPTH).map { |i| format('e%04d', i) }
    check(names, true)
    check(names, false)
  end

  def test_long_names
    names = (0...DEPTH).map { |i| format('element_with_a_name_long_enough_to_skip_the_cache_%04d', i) }
    check(names, true)
    check(names, false)
  end
end
