#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: a processing instruction that ends in '?' with no '>'
# after it.
#
# read_instruction() finds the end of the instruction by scanning for "?>",
# which can be inside a quoted attribute value. When the real end is a '?'
# with nothing after it, the final '>' check stepped pi->s over the
# terminating NUL before calling set_error(), and building the error location
# read one byte past the end of the buffer.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class InstructionUnterminatedTest < ::Test::Unit::TestCase
  # Was an out-of-bounds read under ASan.
  def test_question_mark_at_end_does_not_overread
    ['<?x a="?>"?', "<?x a='?>'?", '<?xml version="?>"?'].each do |xml|
      e = assert_raise(Ox::ParseError, xml) { Ox.parse(xml) }
      assert_match(/processing instruction not terminated/, e.message)
    end
    # Inside an element the parse carries on and fails later, but it must not
    # read past the end on the way.
    assert_raise(Ox::ParseError) { Ox.parse('<a><?x a="?>"?') }
  end

  def test_instruction_with_attributes_still_parses
    doc = Ox.parse('<?x a="b"?><r/>')
    assert_equal('x', doc.nodes[0].value)
    assert_equal('b', doc.nodes[0].attributes[:a])
  end
end
