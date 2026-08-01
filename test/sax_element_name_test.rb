#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: the element name read_element_start() carries to stack_push().
#
# That name used to come from the name cache as a pointer into a Slot. The Slot
# is only reachable through the cache, and its lifetime ends when a GC retires
# it onto the reuse list and a later intern trims that list. In between,
# read_element_start() holds the pointer across the start_element callback --
# arbitrary Ruby -- and all of read_attrs. It now copies the name instead.
#
# The lengths below cover both copy paths: ebuf for a short name and
# ox_strndup() for one that does not fit, either side of the 128 byte boundary.
#
# The GC stress case is a guard, not a gate: it passes on the unpatched parser
# too. Whether the freed Slot is the one being read depends on where it lands in
# the reuse list, which follows the hash of the name, and that could not be
# forced. What was measured is that the Slot does get retired inside the window.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'stringio'
require 'test/unit'
require 'ox'

class SaxElementNameTest < ::Test::Unit::TestCase
  class Watch < ::Ox::Sax
    attr_reader :starts, :ends, :errors

    def initialize(gcs = 0)
      @gcs = gcs
      @starts = []
      @ends = []
      @errors = []
    end

    def start_element(name)
      @starts << name
      @gcs.times { GC.start(full_mark: true, immediate_sweep: true) }
    end

    def end_element(name)
      @ends << name
    end

    def error(message, line, column)
      @errors << "#{message} @#{line}:#{column}"
    end
  end

  def parse(xml, gcs = 0)
    w = Watch.new(gcs)
    Ox.sax_parse(w, StringIO.new(xml))
    w
  end

  # 34 is the last length the cache keeps in a Slot, 127 the last that fits
  # ebuf. Both sides of each matter because they pick different copy paths.
  def test_names_of_every_relevant_length
    [1, 10, 34, 35, 36, 63, 64, 127, 128, 129, 300].each do |n|
      name = 'e' + ('x' * (n - 1))
      w = parse("<r><#{name}/></r>")
      assert_equal([:r, name.to_sym], w.starts, "length #{n}")
      assert_equal([name.to_sym, :r], w.ends, "length #{n}")
      assert_empty(w.errors, "length #{n}")
    end
  end

  # The name has to survive the attributes, which intern names of their own.
  def test_name_survives_many_attributes
    attrs = (0...300).map { |i| %(a#{i}="v") }.join(' ')
    w = parse("<r><wrapper #{attrs}><c/></wrapper></r>")
    assert_equal(%i[r wrapper c], w.starts)
    assert_equal(%i[c wrapper r], w.ends)
    assert_empty(w.errors)
  end

  # And across a callback that collects, which is what retires cache Slots.
  def test_name_survives_gc_in_the_callback
    attrs = (0...50).map { |i| %(b#{i}="v") }.join(' ')
    doc = +'<r>'
    30.times { |i| doc << "<gc#{i} #{attrs}><c/></gc#{i}>" }
    doc << '</r>'

    w = parse(doc, 6)
    assert_empty(w.errors)
    assert_equal(w.starts.sort, w.ends.sort)
    30.times { |i| assert_include(w.starts, :"gc#{i}") }
  end

  # Names that only differ past the cache's key limit must not be confused for
  # each other, since the copy is what the end tag is matched against.
  def test_long_names_that_share_a_prefix
    a = 'p' * 40 + 'a'
    b = 'p' * 40 + 'b'
    w = parse("<r><#{a}/><#{b}/></r>")
    assert_equal([:r, a.to_sym, b.to_sym], w.starts)
    assert_empty(w.errors)
  end

  def test_mismatched_end_tag_is_still_reported
    w = parse('<r><abc></xyz></r>')
    assert(!w.errors.empty?, 'a mismatched end tag should be reported')
  end
end
