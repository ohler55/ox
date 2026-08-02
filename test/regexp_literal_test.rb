#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: parse_regexp() built a pointer before the start of its text.
#
#   te = text + strlen(text) - 1;
#
# is text - 1 for an empty text, and the backward scan that follows stops on the
# opening '/' when there is no closing one, so
#
#   rb_reg_new(text + 1, te - text - 1, options)
#
# was handed a negative length. Ruby checked it and raised
#
#   ArgumentError: negative string size (or size too big)
#
# which is not a parse error the caller can tell apart from a real one. Object
# mode reports a malformed value with set_error(), the way FixnumCode does for
# "bad number format", so this does too.
#
# The text is Regexp#inspect output, which always starts with '/', so nothing
# ox itself wrote is affected.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class RegexpLiteralTest < ::Test::Unit::TestCase
  def load_obj(text)
    Ox.parse_obj("<g>#{text}</g>")
  end

  def b64(str)
    [str].pack('m0')
  end

  # The gate. On develop every one of these raises ArgumentError from deep
  # inside Ruby instead of an Ox parse error.
  def test_a_malformed_literal_is_a_parse_error
    ['/', 'x', 'abc', '/a', 'a/', '\\'].each do |text|
      assert_raise(Ox::ParseError, text.inspect) { load_obj(text) }
    end
  end

  # Same through the base64 arm, which never checked for a leading '/' at all.
  def test_a_malformed_base64_literal_is_a_parse_error
    ['', '/', 'x', 'abc', '/a', 'a/', 'a/b/'].each do |text|
      assert_raise(Ox::ParseError, text.inspect) { load_obj(b64(text)) }
    end
  end

  def test_the_parse_error_names_the_problem
    load_obj('abc')
  rescue Ox::ParseError => e
    assert_match(/Invalid regexp format/, e.message)
  end

  # Everything a Regexp#inspect can produce still loads, and to the same
  # Regexp. These pass on develop too.

  # A '/' in the pattern and a non-ASCII source do not survive the trip, on
  # develop and here alike -- Regexp#inspect writes '/' as '\/' and nothing
  # unescapes it, and rb_reg_new() leaves the source ASCII-8BIT. Both are
  # separate from the length calculation, so they are not pinned either way.
  ROUND_TRIP = [
    //, /a/, /a/i, /a/m, /a/x, /a/mix,
    /\A\d+\z/, /[[:alpha:]]+/i,
    /</, /a&b/, /"q"/, />/m, /a{2,3}/
  ].freeze

  def test_every_literal_ox_writes_loads_back
    ROUND_TRIP.each do |re|
      back = Ox.parse_obj(Ox.dump(re))
      assert_instance_of(Regexp, back, re.inspect)
      assert_equal(re.source, back.source, re.inspect)
      assert_equal(re.options, back.options, re.inspect)
    end
  end

  def test_an_empty_regexp_still_loads
    assert_equal(//, load_obj('//'))
  end

  def test_flags_are_still_parsed
    assert_equal(Regexp::IGNORECASE, load_obj('/a/i').options)
    assert_equal(Regexp::MULTILINE, load_obj('/a/m').options)
    assert_equal(Regexp::EXTENDED, load_obj('/a/x').options)
    assert_equal(Regexp::IGNORECASE | Regexp::MULTILINE | Regexp::EXTENDED,
                 load_obj('/a/mix').options)
  end

  # An unknown flag letter was ignored before and still is; only the length
  # calculation moved.
  def test_an_unknown_flag_is_still_ignored
    assert_equal(/a/, load_obj('/a/z'))
  end

  # Regexp#inspect escapes a '/' in the pattern, so scanning back from the end
  # still lands on the delimiter and the length covers the whole pattern.
  def test_an_escaped_slash_in_the_pattern
    re = load_obj('/a\\/b/')
    assert_instance_of(Regexp, re)
    assert_equal('a\\/b', re.source)
    assert_match(re, 'a/b')
  end

  def test_a_regexp_inside_a_larger_document
    doc = Ox.dump({a: /x/i, b: [/y/, 'z']})
    assert_equal({a: /x/i, b: [/y/, 'z']}, Ox.parse_obj(doc))
  end
end
