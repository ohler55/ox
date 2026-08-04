#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: a rescued Ox::SyntaxError left part of the value in the
# buffer, and nothing could take it back.
#
# append_string() writes as it scans, so by the time it reached a character XML
# has no way to write, the delimiters around the value and the bytes ahead of
# that character were already out. A caller skipping a bad value - rescue and
# carry on - was left holding a document that could not be parsed:
#
#   b.comment("a\1b") rescue nil    left "<!--" open over the rest of the file
#   b.element("a\1b") rescue nil    left "<a", and put the element on the stack
#   b.element('r', 'k' => "a\1b")   left the attribute quote open
#
# The element case had no way out at all. The name was on the stack, so pop()
# went to write "</a\1b>", raised on the same byte with "</a" already out, and
# decremented the depth on the way, so a second pop could not finish it either.
#
# The writers now check the value before they write anything, so a rescued call
# leaves the builder exactly as it was. An element that was validly opened stays
# open, since that call did succeed, and one bad attribute does not take the
# element or the attributes before it with it.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class BuilderResumeTest < ::Test::Unit::TestCase
  # Every writer that takes a value through append_string, with what it should
  # have written by the time the value is refused.
  WRITERS = {
    'comment' => [->(b, v) { b.comment(v) }, ''],
    'doctype' => [->(b, v) { b.doctype(v) }, ''],
    'element name' => [->(b, v) { b.element(v) }, ''],
    'element name sym' => [->(b, v) { b.element(v.to_sym) }, ''],
    'void_element' => [->(b, v) { b.void_element(v) }, ''],
    'void_element sym' => [->(b, v) { b.void_element(v.to_sym) }, ''],
    'instruct' => [->(b, v) { b.instruct(v) }, ''],
    'attr name' => [->(b, v) { b.element('r', v => 'x') }, '<r'],
    'attr value' => [->(b, v) { b.element('r', 'k' => v) }, '<r'],
    'attr name sym' => [->(b, v) { b.element('r', v.to_sym => 'x') }, '<r'],
    'text' => [->(b, v) { b.element('r'); b.text(v) }, '<r']
  }.freeze

  # A byte in each of the ranges the tables refuse, at offsets inside the word
  # loop, on its own, and past it.
  BAD = ["a\x01b", "\x01", "a\0b", "a\x1fb", "a\x0bb", "#{'x' * 9}\x01",
         "\x01#{'x' * 20}", "#{'x' * 20}\x01"].freeze

  def rescued(writer, value)
    b = Ox::Builder.new(indent: -1)
    assert_raise(Ox::SyntaxError) { writer.call(b, value) }
    b
  end

  def test_a_rescued_call_writes_nothing
    WRITERS.each do |where, (writer, expected)|
      BAD.each do |v|
        b = rescued(writer, v)
        assert_equal(expected.b, b.to_s, "#{where} #{v.inspect}")
      end
    end
  end

  # The counters are maintained by hand alongside the writes, so they have to
  # agree with a buffer that only holds what the calls before the refused one
  # put there. indent: -1 keeps it to one line, so column is pos + 1.
  def test_the_counters_agree_with_the_buffer
    WRITERS.each do |where, (writer, expected)|
      b = rescued(writer, "a\x01b")
      assert_equal([1, expected.length + 1, expected.length], [b.line, b.column, b.pos], where)
    end
  end

  # The point of the change: rescue a bad value, keep going, get a document.
  def test_the_document_parses_after_a_rescued_call
    WRITERS.each do |where, (writer, _)|
      BAD.each do |v|
        b = rescued(writer, v)
        b.element('z')
        b.pop
        begin
          b.pop
        rescue Ox::ArgError
          nil
        end
        assert_nothing_raised("#{where} #{v.inspect}") { Ox.parse(b.to_s) }
      end
    end
  end

  # pop() writes the name it kept on the stack. An element that was refused is
  # not on the stack at all, so there is nothing left for pop to trip over.
  def test_pop_after_a_rescued_element_name
    b = Ox::Builder.new
    assert_raise(Ox::SyntaxError) { b.element("a\x01b") }
    b.element('z')
    b.pop
    assert_raise(Ox::ArgError) { b.pop }
    assert_equal("<z/>\n".b, b.to_s)
  end

  # An attribute is written whole or not at all, so the ones before it stay and
  # the element is still the caller's to finish.
  def test_a_bad_attribute_keeps_the_element_and_the_good_attributes
    b = Ox::Builder.new(indent: -1)
    assert_raise(Ox::SyntaxError) { b.element('r', 'ok' => '1', 'bad' => "a\x01b") }
    b.text('t')
    b.pop
    assert_equal('<r ok="1">t</r>'.b, b.to_s)
  end

  # A depth the element never reached would leave a stack slot uninitialised,
  # which is the shape of the crash #449 fixed from the other direction.
  def test_many_rescued_calls_then_a_document
    b = Ox::Builder.new(indent: -1)
    200.times do
      begin
        b.element("a\x01b")
      rescue Ox::SyntaxError
        nil
      end
      begin
        b.comment("a\x01b")
      rescue Ox::SyntaxError
        nil
      end
    end
    b.element('r') { b.text('t') }
    assert_equal('<r>t</r>'.b, b.to_s)
    assert_nothing_raised { GC.start }
  end

  # strip_invalid_chars is the one writer that is allowed to take the value: it
  # drops the byte rather than refusing, so it still writes.
  def test_text_with_strip_invalid_chars_still_writes
    xml = Ox::Builder.new(indent: -1) do |b|
      b.element('r') { b.text("a\x01b", true) }
    end
    assert_equal('<r>ab</r>'.b, xml)
  end

  # raw() is documented as writing its argument untouched and never went through
  # append_string, so it is deliberately not checked.
  def test_raw_is_unchanged
    assert_equal("a\x01b".b, Ox::Builder.new(indent: -1) { |b| b.raw("a\x01b") })
  end

  # Nothing above may change what a good document looks like, including the
  # escapes, the newline bookkeeping and the argument type errors.
  def test_valid_output_is_unchanged
    xml = Ox::Builder.new do |b|
      b.instruct('xml', version: '1.0')
      b.element('top', 'a' => %(1 & 2 < 3 "q")) do
        b.element('mid') { b.text("hello & <world>\nsecond line") }
        b.element('utf8') { b.text('日本語 ☃') }
        b.comment('note')
        b.doctype('html')
        b.void_element('br')
        b.cdata('c]]>d')
      end
    end
    expected = <<~XML
      <?xml version="1.0"?>
      <top a="1 &amp; 2 &lt; 3 &quot;q&quot;">
        <mid>hello &amp; &lt;world&gt;
      second line</mid>
        <utf8>日本語 ☃</utf8>
        <!--note-->
        <!DOCTYPE html>
        <br>
        <![CDATA[c]]]]><![CDATA[>d]]>
      </top>
    XML
    assert_equal(expected.b, xml)
  end

  def test_argument_type_errors_are_unchanged
    assert_raise(Ox::ArgError) { Ox::Builder.new { |b| b.element(123) } }
    assert_raise(Ox::ArgError) { Ox::Builder.new { |b| b.void_element(123) } }
    assert_raise(Ox::ArgError) { Ox::Builder.new { |b| b.element('r', 123 => 'v') } }
    assert_raise(TypeError) { Ox::Builder.new { |b| b.element('r', 'k' => 123) } }
    assert_raise(TypeError) { Ox::Builder.new { |b| b.comment(123) } }
    assert_raise(Ox::ArgError) { Ox::Builder.new(&:element) }
  end

  def test_the_message_still_names_the_character
    e = assert_raise(Ox::SyntaxError) { Ox::Builder.new { |b| b.comment("a\x01b") } }
    assert_equal("'\\#x01' is not a valid XML character.", e.message)
    e = assert_raise(Ox::SyntaxError) { Ox::Builder.new { |b| b.element("a\0b") } }
    assert_equal("'\\#x00' is not a valid XML character.", e.message)
  end
end
