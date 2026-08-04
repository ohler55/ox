#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: Ox::Builder counted more than it wrote.
#
# pos, column and line are maintained by hand alongside every write, so they only
# agree with the buffer if each write is accounted for exactly once. Three did
# not:
#
#   pop()      counted the element name twice - append_string() had already
#              advanced by e->len, then pop added e->len + 3 again
#   comment()  wrote "<!--" (4) and "-->" (3) and counted 5 for each
#   the strip_invalid_chars arm counted the table entry for a dropped byte,
#              which is 10, the longest escape, for something never written
#
#   <a>t</a>                 8 bytes, pos said  9
#   <abcde>t</abcde>        16 bytes, pos said 21
#   <!--hi-->                9 bytes, pos said 12
#   text("a\x01b", true)     9 bytes, pos said 20
#
# None of it is a memory safety problem - the counters are only readable as
# Ox::Builder#pos / #column / #line and are used for neither sizing nor writing -
# but they are what a caller is told about where they are in the output.
#
# What was written is measured with to_s, which had to stop writing to the buffer
# first for that to be a fair measurement; see builder_to_s_test.rb.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class BuilderCountersTest < ::Test::Unit::TestCase
  # Every writer, as a block over a builder. With indent: -1 the whole document
  # is one line and to_s adds nothing, so to_s.bytesize is exactly what was
  # written and pos has to equal it.
  WRITERS = {
    'element 1 char name' => ->(b) { b.element('a') { b.text('t') } },
    'element 2 char name' => ->(b) { b.element('ab') { b.text('t') } },
    'element 5 char name' => ->(b) { b.element('abcde') { b.text('t') } },
    'element 20 char name' => ->(b) { b.element('a' * 20) { b.text('t') } },
    'element symbol name' => ->(b) { b.element(:abcde) { b.text('t') } },
    'element no child' => ->(b) { b.element('a'); b.pop },
    'element empty text' => ->(b) { b.element('a') { b.text('') } },
    'void_element' => ->(b) { b.void_element('br') },
    'nested elements' => ->(b) { b.element('a') { b.element('b') { b.text('t') } } },
    'deeply nested' => ->(b) { 5.times { |i| b.element("e#{i}") }; 5.times { b.pop } },
    'attributes' => ->(b) { b.element('r', 'k' => 'v', 'k2' => 'v2') { b.text('t') } },
    'attribute escaped' => ->(b) { b.element('r', 'k' => 'a<b&c"d') },
    'text plain' => ->(b) { b.element('r') { b.text('hello') } },
    'text escaped' => ->(b) { b.element('r') { b.text('a<b&c>d') } },
    'text long' => ->(b) { b.element('r') { b.text('x' * 300) } },
    'text utf8' => ->(b) { b.element('r') { b.text('日本語 ☃') } },
    'comment' => ->(b) { b.comment('hi') },
    'comment long' => ->(b) { b.comment('hello world') },
    'comment empty' => ->(b) { b.comment('') },
    'comment escaped' => ->(b) { b.comment('a<b&c') },
    'doctype' => ->(b) { b.doctype('html') },
    'cdata' => ->(b) { b.element('r') { b.cdata('data') } },
    'raw' => ->(b) { b.element('r') { b.raw('<x/>') } },
    'instruct' => ->(b) { b.instruct('xml') },
    'instruct attrs' => ->(b) { b.instruct('xml', version: '1.0') },
    'strip 1 invalid' => ->(b) { b.element('r') { b.text("a\x01b", true) } },
    'strip 2 invalid' => ->(b) { b.element('r') { b.text("a\x01\x01b", true) } },
    'strip only invalid' => ->(b) { b.element('r') { b.text("\x01", true) } },
    'strip invalid in long' => ->(b) { b.element('r') { b.text("#{'x' * 20}\x01#{'y' * 20}", true) } }
  }.freeze

  def built(indent = -1)
    b = Ox::Builder.new(indent: indent)
    yield b
    b
  end

  # The whole of Z32 in one assertion, over every writer.
  def test_pos_equals_the_bytes_written
    WRITERS.each do |where, writer|
      b = built { |x| writer.call(x) }
      assert_equal(b.to_s.bytesize, b.pos, where)
    end
  end

  # One line, so column is pos + 1 and line never moves.
  def test_column_and_line_agree_on_one_line
    WRITERS.each do |where, writer|
      next if where.start_with?('text utf8') # multibyte: column counts bytes

      b = built { |x| writer.call(x) }
      assert_equal([1, b.pos + 1], [b.line, b.column], where)
    end
  end

  # The name was counted twice, so the error grew with its length.
  def test_a_closing_tag_counts_the_name_once
    (1..12).each do |n|
      b = built { |x| x.element('a' * n) { x.text('t') } }
      assert_equal(b.to_s.bytesize, b.pos, "name of #{n}")
    end
  end

  def test_a_comment_counts_its_own_delimiters
    ['', 'hi', 'hello world', 'x' * 100].each do |text|
      b = built { |x| x.comment(text) }
      assert_equal(b.to_s.bytesize, b.pos, text.length.to_s)
    end
  end

  # A dropped byte is written nowhere, so it takes up no room. The table holds
  # 10 for it, which is what was being added.
  def test_a_dropped_invalid_character_takes_no_room
    (0..5).each do |n|
      b = built { |x| x.element('r') { x.text("a#{"\x01" * n}b", true) } }
      assert_equal(b.to_s.bytesize, b.pos, "#{n} dropped")
      assert_equal('<r>ab</r>', b.to_s, "#{n} dropped")
    end
  end

  # An escape is written, so it is still counted by what it takes.
  def test_an_escape_is_still_counted_by_its_length
    b = built { |x| x.element('r') { x.text('&<>') } }
    assert_equal('<r>&amp;&lt;&gt;</r>', b.to_s)
    assert_equal(b.to_s.bytesize, b.pos)
  end

  def test_line_moves_with_the_newlines_in_a_value
    b = built { |x| x.element('r') { x.text("a\nb\nc") } }
    assert_equal(3, b.line)
    assert_equal(b.to_s.bytesize, b.pos)
  end

  def test_the_indented_form_agrees_too
    WRITERS.each do |where, writer|
      b = built(2) { |x| writer.call(x) }
      # to_s adds the closing newline to the String it returns and not to the
      # buffer, so it is one longer than what pos counts.
      s = b.to_s
      expected = s.end_with?("\n") ? s.bytesize - 1 : s.bytesize
      assert_equal(expected, b.pos, where)
    end
  end

  # The counters have to agree with the buffer at every step, not only once the
  # document is finished. Reading the buffer part way through is what
  # builder_to_s_test.rb covers; here it is the measuring instrument.
  def test_the_counters_agree_at_every_step
    b = Ox::Builder.new(indent: -1)
    steps = []
    check = lambda do |label|
      steps << [label, b.pos, b.to_s.bytesize, b.column]
    end

    b.element('r')
    check.call('open element')
    b.text('ab')
    check.call('text')
    b.comment('c')
    check.call('comment')
    b.element('in')
    check.call('open nested')
    b.text("x\x01y", true)
    check.call('stripped text')
    b.pop
    check.call('close nested')
    b.pop
    check.call('close element')

    steps.each do |label, pos, bytes, column|
      assert_equal(bytes, pos, label)
      assert_equal(pos + 1, column, label)
    end
    assert_equal('<r>ab<!--c--><in>xy</in></r>', b.to_s)
  end
end
