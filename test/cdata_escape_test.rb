#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: a CDATA value containing "]]>" closed its own section and the
# rest of the value was written as markup.
#
# Both writers put the value between a literal "<![CDATA[" and "]]>" with no
# scan of their own -- builder_cdata() in builder.c and dump_gen_val_node() in
# dump.c. A CDATA section ends at the first "]]>", so
#
#   Ox::Builder.new { |b| b.element('note') { b.cdata(params[:msg]) } }
#
# with a msg of ']]><script>alert(1)</script>' produced
#
#   <note><![CDATA[]]><script>alert(1)</script>]]></note>
#
# and the script was markup by the time anything read it back.
#
# Each occurrence is now split into "]]" + "]]>" + "<![CDATA[" + ">", the
# standard encoding, which reads back as the same three characters. A value that
# holds no "]]>" is written exactly as before.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class CDataEscapeTest < ::Test::Unit::TestCase
  ESCAPES = [
    ']]>',
    ']]><script>alert(1)</script>',
    'a]]>b]]>c',
    ']]]>',
    ']]]]>>',
    "line1\nline2]]>tail",
    ']]>' * 50,
    "]]>あ"
  ].freeze

  # Values that hold no terminator, so nothing about them may change.
  UNCHANGED = ['plain', 'a b c', "multi\nline", '<&>"', ']]', '>', 'x' * 5000, '', "あい"].freeze

  def build(text, indent: -1)
    Ox::Builder.new(indent: indent) { |b| b.element('n') { b.cdata(text) } }
  end

  def dump(text, indent: -1)
    e = Ox::Element.new('n')
    e << Ox::CData.new(text)
    Ox.dump(e, indent: indent)
  end

  # Ox parses adjacent CDATA sections into separate nodes, which is what a split
  # value comes back as, so join them.
  def loaded_text(xml)
    Ox.load(xml).nodes.map { |n| n.respond_to?(:value) ? n.value : n.to_s }.join.force_encoding('UTF-8')
  end

  def test_builder_output_is_a_single_cdata_section
    assert_equal('<n><![CDATA[]]]]><![CDATA[><script>alert(1)</script>]]></n>',
                 build(']]><script>alert(1)</script>'))
  end

  def test_dump_output_is_a_single_cdata_section
    assert_equal('<n><![CDATA[]]]]><![CDATA[><script>alert(1)</script>]]></n>',
                 dump(']]><script>alert(1)</script>'))
  end

  def test_every_occurrence_is_split
    assert_equal('<n><![CDATA[a]]]]><![CDATA[>b]]]]><![CDATA[>c]]></n>', build('a]]>b]]>c'))
  end

  def test_builder_round_trips_the_terminator
    ESCAPES.each do |t|
      [-1, 0, 2].each { |i| assert_equal(t, loaded_text(build(t, indent: i)), "#{t.inspect} indent #{i}") }
    end
  end

  def test_dump_round_trips_the_terminator
    ESCAPES.each do |t|
      [-1, 0, 2].each { |i| assert_equal(t, loaded_text(dump(t, indent: i)), "#{t.inspect} indent #{i}") }
    end
  end

  # Nothing is injected: every child of <n> is still a CDATA node. On the
  # unfixed code the tail of the value comes back as an Ox::Element.
  def test_no_markup_escapes_the_section
    ESCAPES.each do |t|
      [build(t), dump(t)].each do |xml|
        kinds = Ox.load(xml).nodes.map(&:class).uniq
        assert_equal([Ox::CData], kinds, "#{t.inspect} produced #{kinds.inspect}")
      end
    end
  end

  def test_values_without_the_terminator_are_untouched
    UNCHANGED.each do |t|
      [-1, 0, 2, 4].each do |i|
        section = build(t, indent: i)[/<!\[CDATA\[.*?\]\]>/m].force_encoding('UTF-8')
        assert_equal("<![CDATA[#{t}]]>", section, "#{t.inspect} indent #{i}")
        assert_equal(t, loaded_text(build(t, indent: i)), "#{t.inspect} indent #{i}")
      end
    end
  end

  def test_line_and_column_still_track
    b = Ox::Builder.new(indent: -1)
    b.element('n')
    b.cdata("a]]>b\nc")
    b.pop
    assert_equal(2, b.line)
  end
end
