#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: Ox::Builder#to_s wrote to the document it was asked to read.
#
# The closing newline was appended to the buffer rather than to the String that
# to_s returns, and line/column/pos were moved along with it. On a finished
# document that looks idempotent, since the buffer already ends in a newline and
# the second call adds nothing. Called before the document is finished, the
# newline stays where the builder was:
#
#   b.element('r'); b.text('a')
#   b.to_s                        # <- just a read
#   b.text('b'); b.pop
#   b.to_s                        #=> "<r>a\nb</r>\n"   and not "<r>ab</r>\n"
#
# Inside a start tag it lands between the name and the '>', which XML survives.
# Inside a value it is a changed value, which is the part that matters.
#
# The newline now goes on the returned String, so to_s answers the same thing
# whether or not it was called before, and leaves the builder alone.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class BuilderToSTest < ::Test::Unit::TestCase
  # Where a to_s can fall, as a pair of blocks run either side of it.
  MIDPOINTS = {
    'between two texts' => [->(b) { b.element('r'); b.text('a') }, ->(b) { b.text('b'); b.pop }],
    'after a start tag' => [->(b) { b.element('r') }, ->(b) { b.text('t'); b.pop }],
    'before a child' => [->(b) { b.element('r'); b.element('c') }, ->(b) { b.text('t'); b.pop; b.pop }],
    'after a comment' => [->(b) { b.element('r'); b.comment('c') }, ->(b) { b.text('t'); b.pop }],
    'after an attribute' => [->(b) { b.element('r', 'k' => 'v') }, ->(b) { b.text('t'); b.pop }],
    'at the very start' => [->(_b) {}, ->(b) { b.element('r') { b.text('t') } }]
  }.freeze

  def test_a_to_s_in_the_middle_does_not_change_the_document
    MIDPOINTS.each do |where, (before, after)|
      [-1, 2].each do |indent|
        clean = Ox::Builder.new(indent: indent)
        before.call(clean)
        after.call(clean)

        dirty = Ox::Builder.new(indent: indent)
        before.call(dirty)
        dirty.to_s
        after.call(dirty)

        assert_equal(clean.to_s, dirty.to_s, "#{where} indent #{indent}")
      end
    end
  end

  # The value is what a stray newline would corrupt without XML noticing.
  def test_a_value_is_not_split
    b = Ox::Builder.new(indent: 2)
    b.element('r')
    b.text('a')
    b.to_s
    b.text('b')
    b.pop
    assert_equal("<r>ab</r>\n".b, b.to_s)
    assert_equal('ab', Ox.parse(b.to_s).text)
  end

  def test_many_to_s_calls_change_nothing
    b = Ox::Builder.new(indent: 2)
    b.element('r')
    10.times do
      b.to_s
      b.text('x')
    end
    b.pop
    assert_equal("<r>#{'x' * 10}</r>\n".b, b.to_s)
  end

  def test_to_s_does_not_move_the_counters
    MIDPOINTS.each do |where, (before, _)|
      [-1, 2].each do |indent|
        b = Ox::Builder.new(indent: indent)
        before.call(b)
        state = [b.pos, b.line, b.column]
        3.times { b.to_s }
        assert_equal(state, [b.pos, b.line, b.column], "#{where} indent #{indent}")
      end
    end
  end

  def test_to_s_is_idempotent
    b = Ox::Builder.new(indent: 2)
    b.element('r') { b.text('t') }
    first = b.to_s
    3.times { assert_equal(first, b.to_s) }
  end

  # What to_s returns for a finished document is unchanged, which is the whole
  # point: only the buffer behind it stops being written to.
  def test_the_closing_newline_is_still_there
    assert_equal("<r>t</r>\n".b, Ox::Builder.new(indent: 2) { |b| b.element('r') { b.text('t') } })
    assert_equal('<r>t</r>'.b, Ox::Builder.new(indent: -1) { |b| b.element('r') { b.text('t') } })

    b = Ox::Builder.new(indent: 2)
    b.element('r') { b.text('t') }
    assert_equal("<r>t</r>\n".b, b.to_s)

    tight = Ox::Builder.new(indent: -1)
    tight.element('r') { tight.text('t') }
    assert_equal('<r>t</r>'.b, tight.to_s)
  end

  # ...and it still does not add a second newline to a document that has one.
  def test_no_second_newline
    b = Ox::Builder.new(indent: 2)
    b.element('r') { b.text("a\n") }
    assert_equal("<r>a\n</r>\n".b, b.to_s)
  end

  def test_an_empty_builder
    assert_equal("\n".b, Ox::Builder.new(indent: 2).to_s)
    assert_equal(''.b, Ox::Builder.new(indent: -1).to_s)
    assert_equal(0, Ox::Builder.new(indent: 2).pos)
  end

  def test_the_block_form_is_unchanged
    xml = Ox::Builder.new do |b|
      b.instruct('xml', version: '1.0')
      b.element('top', 'a' => '1 & 2') do
        b.element('mid') { b.text("hello\nthere") }
        b.comment('note')
        b.doctype('html')
        b.void_element('br')
        b.cdata('c]]>d')
      end
    end
    expected = <<~XML
      <?xml version="1.0"?>
      <top a="1 &amp; 2">
        <mid>hello
      there</mid>
        <!--note-->
        <!DOCTYPE html>
        <br>
        <![CDATA[c]]]]><![CDATA[>d]]>
      </top>
    XML
    assert_equal(expected.b, xml)
  end

  # A file builder writes as it goes and has no to_s, which is unchanged.
  def test_a_file_builder_still_refuses_to_s
    require 'tempfile'
    Tempfile.create('ox_to_s') do |f|
      Ox::Builder.file(f.path, indent: -1) do |b|
        b.element('r') { b.text('t') }
        assert_raise(Ox::ArgError) { b.to_s }
      end
      assert_equal('<r>t</r>', File.read(f.path))
    end
  end

  # The encoding comes from the instruct, and to_s still labels what it returns
  # with it - including the newline it now appends there.
  def test_the_encoding_is_still_applied
    b = Ox::Builder.new(indent: 2)
    b.instruct('xml', encoding: 'UTF-8')
    b.element('r') { b.text('日本語') }
    assert_equal(Encoding::UTF_8, b.to_s.encoding)
    assert_equal("<?xml encoding=\"UTF-8\"?>\n<r>日本語</r>\n", b.to_s)
  end
end
