#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: Ox.dump(:indent) integer overflow -> output buffer overflow.
#
# :indent was taken as an unbounded int. For a nested element the indent size
# arithmetic (depth * indent, then + overhead) overflowed int, causing the
# output buffer grow() to be skipped and fill_indent() to write past it.
# Oversized indents must now be rejected; valid ones must still work.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class DumpIndentTest < ::Test::Unit::TestCase
  def test_oversized_indent_raises
    [2**20 + 1, 2**30, 2**31 - 1].each do |ind|
      assert_raise(Ox::ParseError) do
        Ox.dump([[1]], indent: ind)
      end
    end
  end

  def test_valid_indent_still_works
    assert_nothing_raised do
      Ox.dump([[1]], indent: 4)
      Ox.dump([1], indent: 16_384) # OX_MAX_INDENT, the maximum allowed
      Ox.dump([[1]], indent: -1)   # negative == tight (no newlines)
    end
  end

  def test_indent_actually_indents
    out = Ox.dump([[1]], indent: 2)
    assert(out.include?("\n"), 'indented dump should contain newlines')
  end
end
