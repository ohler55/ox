#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: Ox::Builder left an out-of-range depth after the
# "XML too deeply nested" raise.
#
# builder_element() incremented b->depth before validating it and did not roll
# the increment back when it raised, so the Builder survived with
# b->depth == MAX_DEPTH while struct _builder only has stack[MAX_DEPTH]. Every
# later call reached i_am_a_child() or pop(), which index &b->stack[b->depth]
# behind only a "0 <= b->depth" guard. Each further rescued element() call
# pushed the index another element past the end, so the writes marched
# arbitrarily far out of the allocation and pop()/builder_free() eventually read
# a name pointer out of adjacent memory and free()d it.
#
# Reusing such a builder segfaulted; under Valgrind it showed as an invalid
# 1-byte read and two invalid 1-byte writes in i_am_a_child().
#
# builder_free() also stopped one element short of the deepest one, leaking the
# strdup'd name of any element whose name did not fit the 64-byte inline buffer.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class BuilderDepthTest < ::Test::Unit::TestCase
  MAX_DEPTH = 128

  # Nesting right up to the limit must still work, so the fix cannot have moved
  # the threshold.
  def test_nesting_to_the_limit_is_allowed
    b = Ox::Builder.new
    assert_nothing_raised do
      MAX_DEPTH.times { |i| b.element("e#{i}") }
    end
    xml = b.to_s
    assert_equal(MAX_DEPTH, xml.scan(/<e\d+/).size)
  end

  def test_one_past_the_limit_raises
    b = Ox::Builder.new
    MAX_DEPTH.times { |i| b.element("e#{i}") }
    assert_raise(Ox::ArgError) { b.element('too_deep') }
  end

  # The crash: rescue the raise, then keep using the same builder.
  def test_builder_is_usable_after_a_rescued_depth_raise
    b = Ox::Builder.new
    (MAX_DEPTH + 1).times do |i|
      begin
        b.element("e#{i}")
      rescue Ox::ArgError
        # expected once the limit is reached
      end
    end
    # Before the fix these walked past the end of the element stack.
    assert_nothing_raised { b.text('text') }
    assert_nothing_raised { b.close }
  end

  # Repeated rescued calls used to push the index further out on every one.
  def test_many_rescued_depth_raises_then_reuse
    b = Ox::Builder.new
    300.times do |i|
      begin
        b.element("e#{i}")
      rescue Ox::ArgError
        # expected past the limit
      end
    end
    assert_nothing_raised { b.text('text') }
    assert_nothing_raised { b.close }
    assert_operator(b.to_s.length, :>, 0)
  end

  # The same shape, but the builder is dropped and collected rather than closed,
  # so builder_free() walks the stack instead of pop().
  def test_gc_after_rescued_depth_raises
    200.times do |i|
      b = Ox::Builder.new
      (MAX_DEPTH + 5).times do |j|
        begin
          b.element("e#{i}_#{j}")
        rescue Ox::ArgError
          # expected past the limit
        end
      end
      b = nil
    end
    assert_nothing_raised { GC.start(full_mark: true, immediate_sweep: true) }
  end

  # builder_free()'s loop bound: names over the 64-byte inline buffer are
  # strdup'd, and the deepest element's copy was never freed. The leak itself is
  # caught by rake test:valgrind; this keeps the path exercised.
  def test_dropped_builder_with_long_element_names
    200.times do |i|
      b = Ox::Builder.new
      3.times { |j| b.element("#{'n' * 100}#{i}_#{j}") }
      b = nil
    end
    assert_nothing_raised { GC.start(full_mark: true, immediate_sweep: true) }
  end

  # Normal use must be unchanged.
  def test_normal_nesting_output_unchanged
    xml = Ox::Builder.new(indent: 2) do |b|
      b.element('top') do
        b.element('mid', 'a' => '1') do
          b.text('hello')
        end
      end
    end
    assert_equal(%(<top>\n  <mid a="1">hello</mid>\n</top>\n), xml)
  end
end
