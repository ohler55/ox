#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: Ox::Builder truncated a value at an embedded NUL instead of
# treating it as the invalid XML character it is.
#
# append_string() is given a size but guarded its three loops with
# '\0' != *str as well, the way a C string is walked. A NUL therefore ended the
# scan before it could reach the table lookup, so the bytes after it were
# dropped without a word. Every other character XML does not allow - 0x01 and
# the rest of the C0 range - reached that lookup and raised Ox::SyntaxError.
#
# NUL is not a character XML has any way to write: the Char production is
#
#   Char ::= #x9 | #xA | #xD | [#x20-#xD7FF] | [#xE000-#xFFFD] | [#x10000-#x10FFFF]
#
# and a character reference has to resolve to a Char as well, so &#0; is not a
# way out either. Ox.dump has always raised on it. Ox::Builder was the one
# writer out of step, and out of step with itself, since it raised on 0x01.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class BuilderNulTest < ::Test::Unit::TestCase
  # Every Builder call that runs its argument through append_string, as a block
  # taking the byte to plant. Keyed by what part of the document it writes.
  WRITERS = {
    'element name' => ->(b, c) { b.element("a#{c}bcd") },
    'void_element name' => ->(b, c) { b.void_element("a#{c}bcd") },
    'attribute name' => ->(b, c) { b.element('ok', "a#{c}b" => 'v') },
    'attribute value' => ->(b, c) { b.element('ok', 'k' => "a#{c}b") },
    'text' => ->(b, c) { b.element('ok'); b.text("a#{c}b") },
    'comment' => ->(b, c) { b.comment("a#{c}b") },
    'doctype' => ->(b, c) { b.doctype("a#{c}b") }
  }.freeze

  def outcome(byte)
    b = Ox::Builder.new
    yield b
    b.to_s
  rescue Ox::SyntaxError => e
    e.message
  end

  def test_every_writer_raises_on_a_nul
    WRITERS.each do |where, writer|
      assert_raise(Ox::SyntaxError, where) do
        Ox::Builder.new { |b| writer.call(b, "\0") }
      end
    end
  end

  def test_the_message_names_the_character
    e = assert_raise(Ox::SyntaxError) { Ox::Builder.new { |b| b.text("a\0b") } }
    assert_equal("'\\#x00' is not a valid XML character.", e.message)
  end

  # The defect stated as the difference it made: NUL was the one invalid
  # character that behaved differently from every other one.
  def test_nul_now_matches_the_other_invalid_characters
    WRITERS.each do |where, writer|
      nul = outcome("\0") { |b| writer.call(b, "\0") }
      soh = outcome("\x01") { |b| writer.call(b, "\x01") }
      assert_equal(soh.sub('x01', 'x00'), nul, where)
    end
  end

  # The word loop reads eight bytes at a time, the loop after it takes the tail
  # shorter than a word, and the arm after that takes one flagged byte. Walking
  # the NUL across a span longer than two words puts it in each of them.
  def test_a_nul_is_found_at_any_offset
    24.times do |n|
      s = "#{'a' * n}\0#{'b' * 24}"
      assert_raise(Ox::SyntaxError, "offset #{n}") { Ox::Builder.new { |b| b.text(s) } }
    end
  end

  def test_a_nul_alone_and_a_nul_last
    assert_raise(Ox::SyntaxError) { Ox::Builder.new { |b| b.text("\0") } }
    assert_raise(Ox::SyntaxError) { Ox::Builder.new { |b| b.text("ab\0") } }
    assert_raise(Ox::SyntaxError) { Ox::Builder.new { |b| b.text("#{'a' * 16}\0") } }
  end

  # text is the only writer that takes the option. Before the fix the NUL ended
  # the scan whatever it was set to, so the bytes after it went missing from the
  # stripped output as well.
  def test_strip_invalid_chars_drops_the_nul_and_keeps_the_rest
    xml = Ox::Builder.new(indent: -1) do |b|
      b.element('ok') { b.text("a\0b", true) }
    end
    assert_equal('<ok>ab</ok>', xml)
  end

  def test_strip_invalid_chars_matches_the_other_invalid_characters
    nul = Ox::Builder.new(indent: -1) { |b| b.element('ok') { b.text("a\0b\0c", true) } }
    soh = Ox::Builder.new(indent: -1) { |b| b.element('ok') { b.text("a\x01b\x01c", true) } }
    assert_equal(soh, nul)
    assert_equal('<ok>abc</ok>', nul)
  end

  # Ox.dump has always raised. It is the behaviour Builder is being brought into
  # line with, so a change to either side should fail here.
  def test_ox_dump_still_raises_the_same_way
    e = assert_raise(Ox::SyntaxError) { Ox.dump("a\0b") }
    assert_equal("'\\#x00' is not a valid XML character.", e.message)
  end

  # raw() is documented as writing its argument with no escaping at all, so it
  # never went through append_string and is deliberately left alone.
  def test_raw_is_unchanged
    assert_equal("a\0b\n", Ox::Builder.new { |b| b.raw("a\0b") })
  end

  # The escape path is shared with every other value, so pin what it produces
  # for input that has nothing wrong with it.
  def test_valid_output_is_unchanged
    xml = Ox::Builder.new do |b|
      b.instruct('xml', version: '1.0')
      b.element('top', 'a' => %(1 & 2 < 3 "q" 'r')) do
        b.element('mid') { b.text("hello & <world>\nsecond line") }
        b.element('utf8') { b.text('日本語 ☃ é') }
        b.element('long') { b.text('x' * 1000) }
        b.comment('note')
        b.void_element('br')
      end
    end
    expected = <<~XML
      <?xml version="1.0"?>
      <top a="1 &amp; 2 &lt; 3 &quot;q&quot; 'r'">
        <mid>hello &amp; &lt;world&gt;
      second line</mid>
        <utf8>日本語 ☃ é</utf8>
        <long>#{'x' * 1000}</long>
        <!--note-->
        <br>
      </top>
    XML
    # Builder writes bytes and only labels them when an encoding was asked for,
    # so what comes back is ASCII-8BIT while the heredoc above is UTF-8.
    assert_equal(expected.b, xml)
  end

  # A rescued call writes nothing at all, so the document reads as though it had
  # never been made. This assertion used to pin the opposite - the "<!--a" the
  # scan had already emitted before it reached the NUL - which is the behaviour
  # builder_resume_test.rb now covers for every writer.
  def test_the_builder_is_usable_after_a_rescued_nul_raise
    nul = Ox::Builder.new
    assert_raise(Ox::SyntaxError) { nul.comment("a\0b") }
    nul.element('x')
    nul.pop

    soh = Ox::Builder.new
    assert_raise(Ox::SyntaxError) { soh.comment("a\x01b") }
    soh.element('x')
    soh.pop

    assert_equal(soh.to_s, nul.to_s)
    assert_equal("<x/>\n".b, nul.to_s)
  end
end
