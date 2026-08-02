#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: the base64 decode path in object mode.
#
# add_text() sized a buffer with b64_orig_size() and filled it with
# from_base64(). The two did not agree, because one measured the whole string
# and the other stopped at the first character outside the alphabet:
#
#   * "AAAA=" measured 2 bytes but decoded 3, so the write and its terminator
#     went past the end of the buffer
#   * "=" and "==" took the size below zero, so it wrapped to ULONG_MAX, and
#     the measuring also read the byte before the string
#
# On top of that the size came from the document and went straight into
# ALLOCA_N, so a large enough element sized an unbounded stack allocation.
#
# All three call sites are covered here: <b> String, <d> Symbol and <g> Regexp.
# Ox itself never writes base64 (USE_B64 is 0 in dump.c), so every one of these
# documents is hand written, which is exactly the untrusted case.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class ObjLoadBase64Test < ::Test::Unit::TestCase
  # Base64.strict_encode64 without the require. base64 stopped being a default
  # gem in Ruby 3.4, so using it here would mean adding it to the Gemfile.
  def b64(str)
    [str].pack('m0')
  end

  def load64(code, text)
    Ox.parse_obj("<#{code}>#{text}</#{code}>")
  end

  # Valid base64 has to decode to exactly what it encoded, at every length
  # around the three character grouping. Verified byte for byte against the
  # pre-fix behaviour over these same 200 lengths.
  def test_valid_base64_round_trips
    (1..200).each do |n|
      raw = (0...n).map { |i| ((i * 37 + n) & 0xFF).chr }.join
      assert_equal(raw, load64('b', b64(raw)).b, "length #{n}")
    end
  end

  # "AAAA=" is a complete four character group with a stray pad after it. The
  # decoder stops at the '=' having written three bytes; the size said two.
  def test_padding_after_a_complete_group
    assert_equal("\x00\x00\x00".b, load64('b', 'AAAA=').b)
    assert_equal("\x00\x00\x00".b, load64('b', 'AAAA==').b)
    assert_equal('hello', load64('b', 'aGVsbG8='))
  end

  # Nothing decodable before the first pad, which used to take the size below
  # zero and read one byte in front of the text.
  def test_padding_only
    assert_equal('', load64('b', '='))
    assert_equal('', load64('b', '=='))
    assert_equal('', load64('b', '==='))
    assert_equal('', load64('b', '=A'))
  end

  # A pad in the middle stops the decode there, and a lone leading character
  # carries no byte at all.
  def test_truncated_and_interrupted
    assert_equal('', load64('b', 'A'))
    assert_equal("\x00".b, load64('b', 'AA').b)
    assert_equal("\x00\x00".b, load64('b', 'AAA').b)
    assert_equal('', load64('b', '=AAAA'))
    assert_equal('', load64('b', 'A=AAAA'))
    assert_equal('h', load64('b', 'aA=BCDEF'))
  end

  # Characters outside the alphabet end the encoding, the same as a pad.
  def test_characters_outside_the_alphabet
    assert_equal('', load64('b', '!!!!'))
    assert_equal("\x00".b, load64('b', 'AA!!').b)
  end

  # The size used to come from the text length and go into ALLOCA_N. A megabyte
  # of base64 is an ordinary document but was a megabyte of stack.
  def test_large_input_does_not_use_the_stack
    raw = 'x' * 1_000_000
    assert_equal(raw, load64('b', b64(raw)))
  end

  # Symbols and Regexps decode through the same pair of functions.
  def test_symbol_and_regexp_paths
    assert_equal(:foo, load64('d', b64('foo')))
    assert_equal(:'Ox::Bar', load64('d', b64('Ox::Bar')))
    assert_equal(/a.c/i, load64('g', b64('/a.c/i')))
  end

  def test_symbol_survives_bad_padding
    assert_nothing_raised { load64('d', '=') }
    assert_nothing_raised { load64('d', 'AAAA=') }
    assert_equal(:'', load64('d', '='))
  end

  # A Regexp element only decodes to something usable when the result is a
  # /.../ literal. Anything else is a malformed value rather than a decode
  # failure -- <g>/</g> does it with no base64 at all -- so parse_regexp()
  # reports it the way the rest of object mode reports one. It used to reach
  # rb_reg_new() with a negative length and raise ArgumentError from inside
  # Ruby; see regexp_literal_test.rb.
  def test_regexp_without_a_literal_is_a_parse_error
    ['=', 'AAAA=', b64('nope')].each do |text|
      assert_raise(Ox::ParseError, text) { load64('g', text) }
    end
    assert_raise(Ox::ParseError) { Ox.parse_obj('<g>/</g>') }
  end

  # Decoding into the String means the encoding is applied to the object the
  # bytes already live in rather than to a copy.
  def test_encoding_is_applied
    xml = "<b>#{b64('héllo')}</b>"
    assert_equal(Encoding::UTF_8, Ox.load(xml, mode: :object, encoding: 'UTF-8').encoding)
  end

  # A base64 String is still registered in the circular reference table.
  def test_circular_reference_to_a_base64_string
    a = Ox.parse_obj('<a i="1"><b i="2">aGVsbG8=</b><p i="2"/></a>')
    assert_equal(%w[hello hello], a)
    assert_same(a[0], a[1])
  end
end
