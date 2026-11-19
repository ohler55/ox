#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: a character reference that decodes to a NUL cut off the rest
# of the value without an error.
#
# Text and attribute values are collapsed in place and then used as C strings.
# &#0; wrote a NUL, and so did a reference that ox_ucs_to_utf8_chars() copies
# out byte for byte (a surrogate or a value past U+10FFFF) when one of those
# bytes is zero:
#
#   Ox.load('<a b="x&#0;tail">x&#0;tail</a>')     => b: "x",  text "x"
#   Ox.load('<a>&#xD800;tail</a>')                  => "\xD8"
#   Ox.load('<a>&#x110000;tail</a>')                => "\u0011"
#
# The same went for Ox.sax_parse with :convert_special, :strict effort
# included. Those references are now treated like any other reference ox can
# not convert.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'stringio'
require 'test/unit'
require 'ox'

class CharRefNulTest < ::Test::Unit::TestCase
  REFS = ['&#0;', '&#x0;', '&#xD800;', '&#55296;', '&#x110000;', '&#x1000041;'].freeze

  class Collector < ::Ox::Sax
    attr_reader :texts, :attrs, :errors

    def initialize
      super
      @texts = []
      @attrs = []
      @errors = []
    end

    def text(value)
      @texts << value
    end

    def attr(_name, value)
      @attrs << value
    end

    def error(message, _line, _column)
      @errors << message
    end
  end

  def test_load_text_keeps_the_rest
    REFS.each do |ref|
      [:strict, :tolerant].each do |effort|
        doc = Ox.load(%(<a>x#{ref}tail</a>), mode: :generic, effort: effort)
        assert_equal("x#{ref}tail", doc.nodes[0], "#{ref} #{effort}")
      end
    end
  end

  def test_load_attr_tolerant_keeps_the_rest
    REFS.each do |ref|
      doc = Ox.load(%(<a b="x#{ref}tail"/>), mode: :generic, effort: :tolerant)
      assert_equal("x#{ref}tail", doc[:b], ref)
    end
  end

  def test_load_attr_strict_raises
    REFS.each do |ref|
      err = assert_raise(Ox::ParseError, ref) do
        Ox.load(%(<a b="x#{ref}tail"/>), mode: :generic, effort: :strict)
      end
      assert_match(/character reference resolves to a NUL/, err.message, ref)
    end
  end

  def test_sax_keeps_the_rest
    REFS.each do |ref|
      handler = Collector.new
      Ox.sax_parse(handler, StringIO.new(%(<a b="x#{ref}tail">x#{ref}tail</a>)), convert_special: true)
      assert_equal(["x#{ref}tail"], handler.attrs, ref)
      assert_equal(["x#{ref}tail"], handler.texts, ref)
      assert_equal(2, handler.errors.size, ref)
    end
  end

  def test_other_references_unchanged
    doc = Ox.load(%(<a b="&#x41;&#x30d4;&#xE282AC;">&#65;&#x10FFFF;&#xD8A7;</a>), mode: :generic)
    assert_equal("Aピ€", doc[:b])
    assert_equal("A\u{10FFFF}ا", doc.nodes[0])
  end
end
