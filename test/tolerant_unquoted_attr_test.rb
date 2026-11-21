#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: tolerant mode walking off the end of the buffer on an
# unquoted attribute value that ends at the element close.
#
# In effort: :tolerant read_quoted_value() reads an unquoted value up to the
# '/' or '>' that ends it, overwrites that character with a '\0' to terminate
# the value, steps pi->s past it, and reports the character in pi->last. The
# attribute loop in read_element() then re-dispatched pi->last through its
# case '>' / case '/', each of which steps pi->s again. For a document whose
# close is the last byte (<u c=x>) that second step put pi->s one past the
# terminating NUL, and the next next_non_white() read out of bounds. The same
# double step also dropped the first byte of the following text and turned a
# valid /> into a spurious "not closed" error.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class TolerantUnquotedAttrTest < ::Test::Unit::TestCase
  # Was an out-of-bounds read (SEGV under ASan) on the unterminated element.
  def test_unquoted_attr_at_close_does_not_overread
    assert_raise(Ox::ParseError) do
      Ox.load('<u c=x>', mode: :generic, effort: :tolerant)
    end
  end

  # The first byte of the text after the close used to be dropped.
  def test_unquoted_attr_keeps_following_text
    e = Ox.load('<a b=c>hi</a>', mode: :generic, effort: :tolerant)
    assert_equal('c', e.attributes[:b])
    assert_equal(['hi'], e.nodes)
  end

  # A self closing element with an unquoted value used to raise "not closed".
  def test_unquoted_attr_self_closing
    e = Ox.load('<a b=c/>', mode: :generic, effort: :tolerant)
    assert_equal('c', e.attributes[:b])
    assert_equal([], e.nodes)
  end

  def test_unquoted_attrs_multiple
    e = Ox.load('<a b=c d=e>t</a>', mode: :generic, effort: :tolerant)
    assert_equal('c', e.attributes[:b])
    assert_equal('e', e.attributes[:d])
    assert_equal(['t'], e.nodes)
  end
end
