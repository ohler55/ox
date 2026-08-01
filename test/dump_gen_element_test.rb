#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: dump_gen_element() writing past its output buffer, and
# writing through an element name that Ruby moved out from under it.
#
# The reservation at the top of the function covers the opening tag only, and
# three things then wrote without a fresh one:
#
#   * the :no_empty branch wrote "></" + name + ">" after the attribute loop had
#     already spent the reservation. dump_gen_attr() reserves only its own
#     bytes, so it can return with out->cur == out->end.
#   * at depth 0 the margin is written once directly and once inside
#     fill_indent(), but was reserved once.
#   * name = StringValuePtr(rname) was taken before rb_hash_foreach(), which
#     calls rb_String() on every attribute value and so runs arbitrary Ruby.
#     Growing the name String there reallocs it and the closing tag was written
#     from the freed buffer.
#
# The first two abort the process on glibc with "realloc(): invalid next size".
# The third does not: it puts freed heap bytes in the output XML, which is what
# the assertions below look at.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class DumpGenElementTest < ::Test::Unit::TestCase
  # Called through rb_String() from dump_gen_attr, inside rb_hash_foreach, and
  # so from inside the dump.
  class Grower
    def initialize(str)
      @str = str
    end

    def to_s
      @str << ('B' * 100_000)
      'v'
    end
  end

  def setup
    # The memcheck lane runs every file in one process, so do not inherit
    # whatever options ran before this.
    @opts = Ox.default_options
  end

  def teardown
    Ox.default_options = @opts
  end

  # The reservation counts the name once and the :no_empty branch writes it
  # twice, so a name long enough to fill the initial 65336 byte buffer past the
  # halfway point is on its own enough. No attributes and no options but
  # :no_empty.
  def test_no_empty_closing_tag_of_a_long_name
    [32_700, 40_000, 60_000].each do |nlen|
      name = 'a' * nlen
      assert_equal("\n<#{name}></#{name}>\n", Ox.dump(Ox::Element.new(name), no_empty: true))
    end
  end

  # The attribute value is sized so the attribute loop leaves out->cur just
  # short of out->end, which is where the unchecked closing tag overflows.
  def test_no_empty_closing_tag_does_not_overrun_the_buffer
    name = 'a' * 200

    (64_800..65_120).step(7) do |vlen|
      el = Ox::Element.new(name)
      el['k'] = 'v' * vlen
      xml = Ox.dump(el, no_empty: true)

      assert(xml.start_with?("\n<#{name} k=\""), "value length #{vlen}")
      assert(xml.end_with?("\"></#{name}>\n"), "value length #{vlen}")
    end
  end

  # A margin longer than the 13 bytes of slack the old reservation happened to
  # leave. The first node sizes the buffer so the second element starts near the
  # end of it.
  def test_margin_at_depth_zero_is_reserved_for_both_writes
    margin = ' ' * 126
    name   = 'e' * 60

    (64_700..65_000).step(7) do |fill|
      doc = Ox::Document.new
      a   = Ox::Element.new('a')
      a << ('x' * fill)
      doc << a
      doc << Ox::Element.new(name)
      xml = Ox.dump(doc, margin: margin, indent: 2, with_xml: false)

      assert(xml.end_with?("#{margin}<#{name}/>\n"), "filler length #{fill}")
    end
  end

  # The name has to be long enough to be heap allocated rather than embedded in
  # the RVALUE, so 1000 bytes and not 100.
  def test_name_moved_by_the_attribute_loop_is_not_written_from_freed_memory
    el = Ox::Element.new('a' * 1000)
    el.attributes[:k] = Grower.new(el.value)

    xml = Ox.dump(el, no_empty: true)

    # el.value is the grown name by now, since to_s ran during the dump.
    assert(xml.end_with?("></#{el.value}>\n"), 'closing tag is not the element name')
  end

  # Same, with the closing tag written by the nodes branch instead.
  def test_name_moved_before_the_closing_tag_of_a_parent
    el = Ox::Element.new('a' * 1000)
    el << 'text'
    el.attributes[:k] = Grower.new(el.value)

    xml = Ox.dump(el)

    assert(xml.end_with?("</#{el.value}>\n"), 'closing tag is not the element name')
  end

  # Everything below must keep working.

  def test_plain_element_unchanged
    el = Ox::Element.new('top')
    el['a'] = '1'
    el << 'text'
    assert_equal(%(\n<top a="1">text</top>\n), Ox.dump(el))
  end

  def test_empty_element_forms
    el = Ox::Element.new('top')
    assert_equal("\n<top/>\n", Ox.dump(el))
    assert_equal("\n<top></top>\n", Ox.dump(el, no_empty: true))
  end

  def test_margin_and_indent_unchanged
    doc = Ox::Document.new
    doc << Ox::Element.new('a')
    doc << Ox::Element.new('b')
    assert_equal("--\n--<a/>--\n--<b/>\n", Ox.dump(doc, margin: '--', indent: 2, with_xml: false))
  end

  def test_nested_elements_round_trip
    xml = "<top>\n  <a x=\"1\">t</a>\n  <b/>\n</top>\n"
    assert_equal("\n#{xml}", Ox.dump(Ox.parse(xml), indent: 2))
  end
end
