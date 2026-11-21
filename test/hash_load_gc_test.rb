#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: hash mode handing a collected object to the result.
#
# end_element pops the element off the helper stack and then builds the key and
# the parent Hash or Array before it stores the element's value. Once more than
# 16 elements are open the stack is on the heap, and the popped slot is past the
# part mark_pi_cb marks, so nothing kept the value alive across those
# allocations. A GC there freed it and the parent got whatever reused the slot.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class HashLoadGcTest < ::Test::Unit::TestCase
  DEPTH = 20

  def setup
    @opts = Ox.default_options
    Ox.default_options = {encoding: nil, symbolize_keys: true}
  end

  def teardown
    Ox.default_options = @opts
  end

  def xml(cnt)
    ('<a>' * DEPTH) + (0...cnt).map { |i| format('<b>value %04d</b>', i) }.join + ('</a>' * DEPTH)
  end

  def inner(h)
    DEPTH.times { h = h[:a] }
    h[:b]
  end

  # The key mod runs between the pop and the store.
  def test_element_key_mod_gc
    mod = lambda { |k|
      GC.start(full_mark: true, immediate_sweep: true)
      @junk = Array.new(1_000) { |i| "junk #{i}" }
      k.to_sym
    }
    h = Ox.load(xml(4), mode: :hash, element_key_mod: mod)
    assert_equal((0...4).map { |i| format('value %04d', i) }, inner(h))
  end

  # Without a key mod the parent Hash allocated for the first child is enough.
  def test_gc_stress
    GC.stress = true
    begin
      h = Ox.load(xml(4), mode: :hash)
    ensure
      GC.stress = false
    end
    GC.start
    @junk = Array.new(1_000) { |i| "junk #{i}" }
    assert_equal((0...4).map { |i| format('value %04d', i) }, inner(h))
  end
end
