#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: tolerant mode walking off the end of the buffer on a
# closing tag that belongs to an outer element and is cut off before its '>'.
#
# When a closing tag doesn't match the element being read, tolerant mode
# closes that element and hands the name up to its parent, and the parent
# whose name matches steps pi->s over the '>'. The '>' was never checked on
# that path, so for <a><></a followed by white space and the end of the input
# pi->s sat on the terminating NUL, the step put it one past, and the next
# next_non_white() read out of bounds.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class TolerantUnclosedEndTagTest < ::Test::Unit::TestCase
  # Was an out-of-bounds read under ASan.
  def test_mismatched_end_tag_at_end_does_not_overread
    [:generic, :limited, :hash].each do |mode|
      ["<a><></a\r", '<a><></a ', "<x><a><></a\r"].each do |xml|
        assert_raise(Ox::ParseError, "#{mode} #{xml.inspect}") do
          Ox.load(xml, mode: mode, effort: :tolerant)
        end
      end
    end
  end

  # Same error as when the closing tag matches the element directly.
  def test_same_error_as_direct_close
    direct = assert_raise(Ox::ParseError) { Ox.load("<a></a\r", mode: :generic, effort: :tolerant) }
    nested = assert_raise(Ox::ParseError) { Ox.load("<a><b></a\r", mode: :generic, effort: :tolerant) }
    assert_match(/element not closed/, direct.message)
    assert_match(/element not closed/, nested.message)
  end

  # A closing tag that skips an unclosed child still works when it is complete.
  def test_mismatched_end_tag_still_closes
    e = Ox.load('<a><b>x</a>', mode: :generic, effort: :tolerant)
    assert_equal('a', e.value)
    assert_equal('b', e.nodes[0].value)
    assert_equal(['x'], e.nodes[0].nodes)
  end
end
