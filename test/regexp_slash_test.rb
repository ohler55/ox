#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: a '/' in a Regexp pattern came back with a backslash in front
# of it, so the loaded Regexp was not == the one that was dumped.
#
# The dumper writes Regexp#inspect, which escapes a '/' in the source as '\/'.
# The loader took the bytes between the delimiters verbatim, so the backslash
# inspect had added stayed in the source:
#
#   Ox.dump(%r{a/b})           => "<g>/a\\/b/</g>"
#   Ox.parse_obj(that).source  => "a\\/b"     where the original was "a/b"
#
# Matching was never affected, since '\/' and '/' mean the same thing to a
# regexp engine. What changed was the source, and Regexp#== compares sources.
#
# The dumped form is ambiguous and cannot be made to round trip everything:
# Regexp.new('/') and Regexp.new('\/') hold different sources, are not ==, and
# both write "/\//". One of the two has to lose. This picks the one that Ruby
# itself cannot produce from a literal - /a\/b/.source is already "a/b", so a
# source holding '\/' only comes from Regexp.new with a redundant backslash, and
# that Regexp matches exactly what the unescaped one matches.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class RegexpSlashTest < ::Test::Unit::TestCase
  # tests.rb leaves mode: :object behind and :encoding changes what a source
  # comes back as, so establish both rather than inherit them.
  BASE_OPTIONS = {mode: :object, encoding: nil}.freeze

  def setup
    @opts = Ox.default_options
    Ox.default_options = BASE_OPTIONS
  end

  def teardown
    Ox.default_options = @opts
  end

  def round_trip(re)
    Ox.parse_obj(Ox.dump(re))
  end

  # The gate. Every one of these has a bare '/' somewhere and none of them
  # round tripped before.
  SLASHED = ['a/b', '/', '//', '/a', 'a/', 'a//b', '///',
             '[a/b]', '(?<n>/)', 'a{1,2}/', '\\A/\\z'].freeze

  def test_a_slash_in_the_pattern_survives
    SLASHED.each do |src|
      re = Regexp.new(src)
      back = round_trip(re)
      assert_instance_of(Regexp, back, src)
      assert_equal(re.source, back.source, src)
      assert_equal(re, back, src)
    end
  end

  # A '/' after a real backslash is the case a naive unescape would get wrong,
  # since the backslash pair has to be stepped over whole.
  def test_a_slash_after_an_escaped_backslash_survives
    ['a\\\\/b', 'a\\\\\\\\/b', '\\\\/', '/\\\\/'].each do |src|
      re = Regexp.new(src)
      assert_equal(re.source, round_trip(re).source, src)
      assert_equal(re, round_trip(re), src)
    end
  end

  # Sources with no '/' at all were never affected and must stay that way.
  def test_patterns_without_a_slash_are_unchanged
    ['abc', 'a.c', '\\d+', 'a|b', '\\\\', 'a\\.b', 'a\\nb', "a\nb",
     'a"b', "a'b", 'a<b', 'a&b', 'a\\\\', ''].each do |src|
      re = Regexp.new(src)
      assert_equal(re.source, round_trip(re).source, src)
      assert_equal(re, round_trip(re), src)
    end
  end

  # Every Regexp that can be written as a literal round trips, because Ruby
  # normalises the redundant backslash away when it reads the literal.
  def test_literals_round_trip
    [%r{a/b}, /a\/b/, %r{/}, %r{//}, /\A\/\z/, %r{[a/b]}, %r{a/b}i, %r{/}mx,
     //, /abc/, /a.c/i, /\d+/m, /a|b/x].each do |re|
      assert_equal(re, round_trip(re), re.inspect)
    end
  end

  def test_flags_survive_alongside_a_slash
    assert_equal(Regexp::IGNORECASE, round_trip(%r{a/b}i).options)
    assert_equal(Regexp::MULTILINE | Regexp::EXTENDED, round_trip(%r{/}mx).options)
    assert_equal(0, round_trip(%r{a/b}).options)
  end

  # The two spellings the dumped form cannot tell apart. Whichever way the
  # loader reads it, one of them comes back as the other, so pin which.
  def test_the_ambiguous_pair
    bare = Regexp.new('/')
    escaped = Regexp.new('\\/')
    assert_not_equal(bare.source, escaped.source)
    assert_not_equal(bare, escaped)
    assert_equal(bare.inspect, escaped.inspect, 'the dumped form is the same for both')

    assert_equal(bare, round_trip(bare))
    assert_equal(bare, round_trip(escaped))
  end

  # Neither spelling changes what the Regexp matches, which is why trading one
  # for the other is not a behaviour change.
  def test_the_redundant_escape_matches_the_same_things
    a = Regexp.new('a\\/b')
    b = Regexp.new('a/b')
    ['a/b', 'a\\/b', 'x', ''].each do |s|
      assert_equal(a.match?(s), b.match?(s), s)
      assert_equal(a.match?(s), round_trip(a).match?(s), s)
    end
  end

  def test_a_regexp_with_a_slash_inside_a_larger_document
    src = {a: %r{x/y}i, b: [%r{/}, 'z/w']}
    assert_equal(src, Ox.parse_obj(Ox.dump(src)))
  end

  # A pattern the loader cannot make sense of still raises rather than being
  # read past the end.
  def test_malformed_input_still_raises
    assert_raise(Ox::ParseError) { Ox.parse_obj('<g>abc</g>') }
    assert_raise(Ox::ParseError) { Ox.parse_obj('<g>/</g>') }
    assert_raise(Ox::ParseError) { Ox.parse_obj('<g></g>') }
  end

  # A trailing backslash has no byte after it to pair with.
  def test_a_trailing_backslash_is_not_read_past
    assert_raise(RegexpError) { Ox.parse_obj('<g>/a\\/</g>') }
  end
end
